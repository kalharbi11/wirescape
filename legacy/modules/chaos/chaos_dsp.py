"""CHAOS voice buses: deliberately independent of GUTTR, Surge, and any GUI.

One complete DSP block in/out, stereo float32 (2, n). Routing presets are prototypes,
not the final algorithm collection. A future vocal module may connect via ports.py.

All processing is bounded and stable across blocks. No heavy Python sample-by-sample
vocoder, glottal model, or synchronous Node process in the audio callback.
"""
import numpy as np
from scipy.signal import lfilter


class VoiceRouting:
    """0 parallel; 1 bipolar AM; 2 4-ms audio-modulated delay of A by B.

    The processing location is explicit: after both Surge generators, before
    resonant body, looper, master FX. Amount 0 = parallel for every route.
    """
    def __init__(self, sr=48000):
        self.sr = sr
        self.history = np.zeros((2, max(1024, int(sr * 0.02))), np.float32)
        self.smooth = 0.0

    def process(self, a, b, state):
        assert a.shape == b.shape and a.ndim == 2 and a.shape[0] == 2
        mode, depth = int(state["ROUTE_MODE"]), float(state["ROUTE_DEPTH"])
        base = a + b
        if depth <= 0.00001 or mode == 0:
            self.history = np.concatenate((self.history, a), axis=1)[:, -self.history.shape[1]:]
            return base.astype(np.float32)
        if mode == 1:
            # Ring and AM interaction is a continuously variable departure from parallel.
            # atan-style normalization and soft clipping keep the output controlled.
            am = a * (0.6 + 1.25 * np.tanh(b * 2.0))
            return ((1.0-depth)*base + depth*(am + 0.35*b)).astype(np.float32)
        if mode == 2:
            # Read only older samples. A fixed minimum delay prevents time-travel
            # reads (and data hazards) when B modulates delay time.
            n = a.shape[1]
            stacked = np.concatenate((self.history, a), axis=1)
            history_n = self.history.shape[1]
            driver = np.tanh(np.mean(b, axis=0) * 2.0)
            smooth, zi = lfilter([0.08], [1.0, -0.92], driver, zi=[0.92*self.smooth])
            self.smooth = float(smooth[-1])
            delays = (0.0035 + (0.0025 * smooth)) * self.sr
            delays = np.maximum(1.0, delays)
            p = history_n + np.arange(n) - delays
            indices = np.floor(p).astype(np.int64)
            indices = np.clip(indices, 0, stacked.shape[1] - 2)
            frac = (p - indices).astype(np.float32)
            # delay interpolated in stereo; old history is kept for next block
            warped = stacked[:, indices] * (1.0 - frac) + stacked[:, indices + 1] * frac
            self.history = stacked[:, -history_n:].copy()
            return ((1.0-depth)*base + depth*(warped + 0.5*b)).astype(np.float32)
        raise ValueError(f"unknown route mode: {mode}")


class ResonantBody:
    """Three short feedback combs with controlled gain and damped feedback.

    Intentionally generic resonator, NOT a model of a human vocal tract. The three
    delays scale around BODY_TONE to create resonant color from either oscillator.
    Feedback is bounded <0.85 and each circulation softly saturates. Per-sample
    inner loops here are modest at 0-1 wet; only run when BODY_MIX > 0.
    """
    def __init__(self, sr=48000):
        self.sr = sr
        self.n = max(2048, int(sr * .15))
        self.lines = np.zeros((3, 2, self.n), np.float32)
        self.lp = np.zeros((3, 2), np.float32)
        self.pos = 0

    def process(self, x, st):
        wet = float(st["BODY_MIX"])
        if wet < 1e-5:
            # Free-running pointer is not required while bypassed: freezing line state
            # is preferred to consuming CPU in bypass.
            return x
        tone = float(st["BODY_TONE"])
        fb = float(np.clip(st["BODY_FEED"], 0.0, 0.85))
        # 3 resonant delays, staggered; frequency rises as BODY_TONE rises.
        lengths = [max(32, int(self.sr * s * (1.7-1.3*tone))) for s in (.012, .019, .031)]
        y = np.zeros_like(x)
        # Chunk by the shortest delay: all reads strictly precede writes.
        # Vectorized NumPy + compiled SciPy IIR replaces ~1,500 Python
        # inner-loop iterations per callback. Holds continuity across blocks.
        chunk = min(lengths)
        for off in range(0, x.shape[1], chunk):
            size = min(chunk, x.shape[1]-off)
            indexes = (self.pos + off + np.arange(size)) % self.n
            for i, delay in enumerate(lengths):
                read_indices = (indexes - delay) % self.n
                delayed = self.lines[i][:, read_indices]
                damped = np.empty_like(delayed)
                for ch in range(2):
                    damped[ch], zi = lfilter([0.18], [1.0, -0.82], delayed[ch],
                                              zi=[0.82 * self.lp[i, ch]])
                    self.lp[i, ch] = float(damped[ch, -1])
                self.lines[i][:, indexes] = np.tanh(x[:, off:off+size] * 0.5 + fb*damped)
                y[:, off:off+size] += delayed * (1/3)
        self.pos = (self.pos + x.shape[1]) % self.n
        return ((1.0-wet)*x + wet*(x*0.45+y)).astype(np.float32)
