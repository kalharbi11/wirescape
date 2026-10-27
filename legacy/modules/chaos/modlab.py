"""Oneiroi-inspired modulation: a self-modulating LFO and a randomizer with undo.

Both run at control rate (once per audio callback, ~100 Hz), which is plenty for LFOs under 20 Hz.
"""
import math
import random

SHAPES = ["sine", "tri", "s&h", "smooth"]
TARGETS = ["off", "BREAK", "DETUNE", "ROUGH_RATE", "ROUGH_DEPTH", "LOOP_SPEED", "LOOP_FEEDBACK", "CUTOFF", "DRIFT", "B_LEVEL", "ROUTE_DEPTH", "BODY_MIX", "BODY_FEED", "TAPE", "SPACE_MIX"]


class SelfModLFO:
    """LFO whose own output bends its rate (SELF), so its rhythm never settles.

    A second, slow LFO wobbles the depth. Output is -1..1 times depth.
    Ties to the study guide: unpredictable modulation resists habituation (Blumstein 2010).
    """

    def __init__(self):
        self.shape = "smooth"
        self.rate = 0.3  # Hz
        self.depth = 0.0  # 0..1
        self.self_amt = 0.0  # 0..1
        self.target = "off"
        self.phase = 0.0
        self.slow_phase = 0.0
        self.out = 0.0
        self._hold = 0.0
        self._from = 0.0
        self._to = random.uniform(-1, 1)

    def tick(self, dt):
        # rate bends up/down by up to +/-2 octaves with its own previous output
        r = self.rate * (2.0 ** (2.0 * self.self_amt * self.out / max(self.depth, 1e-3)))
        self.phase += r * dt
        wrapped = self.phase >= 1.0
        self.phase %= 1.0
        self.slow_phase = (self.slow_phase + 0.031 * dt) % 1.0
        if wrapped:
            self._hold = random.uniform(-1, 1)
            self._from, self._to = self._to, random.uniform(-1, 1)
        p = self.phase
        if self.shape == "sine":
            v = math.sin(2 * math.pi * p)
        elif self.shape == "tri":
            v = 4 * abs(p - 0.5) - 1
        elif self.shape == "s&h":
            v = self._hold
        else:  # smooth random: cosine interpolation between random points
            t = 0.5 - 0.5 * math.cos(math.pi * p)
            v = self._from + (self._to - self._from) * t
        depth_wobble = 0.75 + 0.25 * math.sin(2 * math.pi * self.slow_phase)
        self.out = v * self.depth * depth_wobble
        return self.out


GROUPS = {
    "SCREAM": ["BREAK", "ROUGH_RATE", "ROUGH_DEPTH", "JITTER", "SUB", "FM_RATIO"],
    "UNSTABLE": ["DETUNE", "VOICES", "DRIFT", "OFFSET", "CLUSTER", "FALL"],
    "DECAY": ["TAPE", "SPACE", "SPACE_MIX"],
    "LOOPER": ["LOOP_SPEED", "LOOP_FEEDBACK", "LOOP_MIX"],
    "ROUTING": ["ROUTE_MODE", "ROUTE_DEPTH", "BODY_MIX", "BODY_TONE", "BODY_FEED"],
    "MODULATION": ["LFO_RATE", "LFO_DEPTH", "LFO_SELF"],
}
INTENSITY = {"LOW": 0.10, "MID": 0.30, "HIGH": 0.70}


class Randomizer:
    """Oneiroi-style randomizer: target group, 3 intensities, undo stack, repeat."""

    def __init__(self, limits):
        self.limits = limits  # name -> (lo, hi)
        self.history = []
        self.last = None

    def roll(self, state, group, intensity):
        self.history.append(dict(state))
        self.last = (group, intensity)
        spread = INTENSITY[intensity]
        new = dict(state)
        names = GROUPS[group] if group != "ALL" else sum(GROUPS.values(), [])
        for k in names:
            if k not in self.limits:
                continue
            lo, hi = self.limits[k]
            span = hi - lo
            v = state[k] + random.uniform(-spread, spread) * span
            new[k] = min(hi, max(lo, v))
        return new

    def repeat(self, state):
        if not self.last:
            return state
        return self.roll(state, *self.last)

    def undo(self, state):
        return self.history.pop() if self.history else state

# Control-rate patch matrix. Source zero and destination zero mean disconnected.
MATRIX_SOURCES = ("off", "self LFO", "sample & hold", "smooth random", "output envelope")
MATRIX_TARGETS = ("off", "BREAK", "DETUNE", "CUTOFF", "ROUGH_DEPTH",
                  "LOOP_SPEED", "LOOP_FEEDBACK", "TAPE", "SPACE_MIX",
                  "ROUTE_DEPTH", "BODY_MIX", "BODY_FEED")

class ModulationMatrix:
    """Four additive, bipolar modulation routes with normalized target-range depths.

    Runs on audio blocks only. Envelope source follows the preceding output block:
    no audio-thread recursion, no lookahead, no external UI/Surge access.
    """
    def __init__(self, sr=48000, seed=21):
        self.sr = sr
        self.rng = random.Random(seed)
        self.phase = 0.0
        self.hold = 0.0
        self.smooth = 0.0
        self.smooth_target = 0.0
        self.envelope = 0.0

    def update_envelope(self, signal):
        import numpy as np
        peak = float(np.sqrt(np.mean(np.asarray(signal, dtype=np.float64) ** 2)))
        k = 0.25 if peak > self.envelope else 0.06
        self.envelope += k * (min(1.0, peak * 2.0) - self.envelope)

    def apply(self, state, dt, primary_lfo, limits):
        rate = max(0.02, float(state["LFO_RATE"]))
        old = self.phase
        self.phase = (self.phase + rate * dt) % 1.0
        if self.phase < old:
            self.hold = self.rng.uniform(-1, 1)
            self.smooth_target = self.rng.uniform(-1, 1)
        self.smooth += (self.smooth_target - self.smooth) * min(1.0, dt * 3.0)
        sources = (0.0, primary_lfo, self.hold, self.smooth, 2*self.envelope-1)
        st = dict(state)
        for i in range(1, 5):
            src = int(state[f"MOD{i}_SRC"])
            dst = int(state[f"MOD{i}_DST"])
            amt = float(state[f"MOD{i}_AMT"])
            if src == 0 or dst == 0 or abs(amt) < 1e-5:
                continue
            target = MATRIX_TARGETS[dst]
            lo, hi = limits[target]
            st[target] = min(hi, max(lo, st[target] + sources[src] * amt * (hi-lo) * 0.5))
        return st
