"""CHAOS: dual-Surge complex synthesis engine. One instance per app.

The UI thread only calls `set()`, `note_on()`, `note_off()` (they enqueue). Everything that touches
Surge happens inside `render()`, which the audio callback (or the offline renderer) calls.
"""
import math
import os
import queue
import random
import threading

import numpy as np


def midi_to_hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)

import surge_path  # noqa: F401
import surgepy

import chaos_dsp
import hostdsp
import voices as V
import macros as M
from modlab import SelfModLFO, ModulationMatrix

SR = 48000
BLOCK = 512  # frames per callback; must be a multiple of Surge's 32-sample block
HERE = os.path.dirname(os.path.abspath(__file__))


class Engine:
    def __init__(self, sr=SR):
        self.sr = sr
        self.s = surgepy.createSurge(sr)
        self.bs = self.s.getBlockSize()
        self.q = queue.Queue()
        self.state = dict(M.DEFAULTS)
        self._P = self._param_map()
        self._sent = {}
        for sc, name, v in M.INIT_VOICE:
            self._set_param(sc, name, v)
        for _ in range(8):  # let FX slots load so their real parameter names exist
            self.s.process()
        self._alias_fx_names()
        # ---- voice B: a second Surge instance; raw panels for A and B
        self.sB = surgepy.createSurge(sr)
        self._PB = self._param_map(self.sB)
        for sc, name, v in M.INIT_VOICE:
            p = self._PB.get((sc, name))
            if p is not None and (sc == 0 or name in ("Pitch Bend Up Range", "Pitch Bend Down Range", "Polyphony Limit")):
                self.sB.setParamVal(p, float(v))
        for _ in range(8):
            self.sB.process()
        self._alias_fx_names(self.sB, self._PB)
        self.inst = {"A": self.s, "B": self.sB}
        self.raw = {"A": self._raw_map(self.s, self._P), "B": self._raw_map(self.sB, self._PB)}
        self.labels = {"A": {}, "B": {}}
        self.raw_overrides = {"A": {}, "B": {}}
        self.surge_raw_overrides = {"A": {}, "B": {}}
        self.b_type = "warm pad"
        self._apply_b_type(self.b_type)
        self._mbB = {}
        self.cents = 0.0
        self.tick = 0
        self._mb = {}
        # host DSP
        self.routing = chaos_dsp.VoiceRouting(sr)
        self.body = chaos_dsp.ResonantBody(sr)
        self.matrix = ModulationMatrix(sr)
        self.rough = hostdsp.RoughAM(sr)
        self.looper = hostdsp.Looper(5.0, sr)
        self.out = hostdsp.Output()
        self.lfo = SelfModLFO()
        self.lfo_target = "off"
        # pitch state (cents), sent to Surge as pitch bend (range set to 24 st)
        self.held = []            # root notes in press order
        self.sounding = set()     # actual keys sent to Surge
        self.fall_cents = 0.0
        self.jit = 0.0
        self.jit_target = 0.0
        self.jump = 0.0
        self.jump_t = 0.0
        self.t = 0.0
        self.last_note = 57
        self._tuned_qt = False
        # analysis ring buffer (mono, 4 s) + stats
        self.ring = np.zeros(sr * 4, np.float32)
        self.ring_pos = 0
        self.lock = threading.Lock()
        self.cpu = 0.0
        self.last_stems = {}  # A/B post-mixer inputs, CHAOS pre-master; audio-thread owned
        self.eff = dict(self.state)  # state after LFO offsets (for display)
        # Optional output capture for diagnostic rendering. No offline vocal LAB.
        self.rec_on = False
        self.rec_chunks = []
        self._apply_surge(self.state)

    # ------------------------------------------------------------ parameter plumbing
    def _param_map(self, inst=None):
        inst = inst or self.s
        P = {}
        for gid in range(32):
            try:
                cg = inst.getControlGroup(gid)
            except Exception:
                continue
            for e in cg.getEntries():
                for p in e.getParams():
                    P.setdefault((e.getScene(), p.getName()), p)
        return P

    def _alias_fx_names(self, inst=None, P=None):
        inst, P = inst or self.s, P if P is not None else self._P
        for (sc, name), p in list(P.items()):
            if sc == 0 and name.startswith("FX ") and "Param" in name:
                real = inst.getParameterName(p.getId())
                P.setdefault((0, real), p)

    # ------------------------------------------------------------ raw (full synth) parameters
    def _raw_map(self, inst, P):
        byid = {p.getId().getSynthSideId(): p for (sc, _), p in P.items() if sc == 1}
        out = {}
        for _, keys in V.RAW_GROUPS:
            for k in keys:
                if k in V.MODS:
                    out[k] = ("mod",) + V.MODS[k]
                elif " P" in k and k.startswith("Osc ") and k[-1].isdigit() and k.split()[-1].startswith("P"):
                    base = P.get((1, f"Osc {k.split()[1]} Type"))
                    if base is not None:
                        q = byid.get(base.getId().getSynthSideId() + 2 + int(k.split()[-1][1:]))
                        if q is not None:
                            out[k] = q
                elif (1, k) in P:
                    out[k] = P[(1, k)]
        return out

    def _set_raw(self, voice, key, norm):
        inst, ref = self.inst[voice], self.raw[voice].get(key)
        if ref is None:
            return
        if isinstance(ref, tuple):  # modulation routing: centre = off
            _, src, tgt = ref
            tp = self.raw[voice].get(tgt)
            if tp is not None and not isinstance(tp, tuple):
                inst.setModDepth01(tp, inst.getModSource(getattr(surgepy.constants, src)), float(norm) * 2 - 1)
            return
        lo, hi = inst.getParamMin(ref), inst.getParamMax(ref)
        v = lo + float(norm) * (hi - lo)
        if inst.getParamValType(ref) in ("int", "bool"):
            v = round(v)
        inst.setParamVal(ref, v)

    def _set_surge_raw(self, voice, scene, name, normalized):
        """All true Surge params, scene 0/1/2, with explicit last-write precedence.

        Name/id tables come from the actual loaded Surge build; an incompatible
        saved value is ignored rather than writing a different parameter.
        """
        inst = self.inst[voice]
        mapping = self._P if voice == "A" else self._PB
        ref = mapping.get((scene, name))
        if ref is None:
            return False
        low, high = inst.getParamMin(ref), inst.getParamMax(ref)
        val = low + float(normalized) * (high-low)
        if inst.getParamValType(ref) in ("int", "bool"):
            val = round(val)
        inst.setParamVal(ref, val)
        return True

    def raw_norm(self, voice, key):
        inst, ref = self.inst[voice], self.raw[voice].get(key)
        if ref is None:
            return 0.0
        if isinstance(ref, tuple):
            return 0.5
        lo, hi = inst.getParamMin(ref), inst.getParamMax(ref)
        return (inst.getParamVal(ref) - lo) / (hi - lo) if hi > lo else 0.0

    def _apply_b_type(self, name):
        rec = V.B_TYPES[name]
        for key in [k for k in rec if k.endswith("Type")]:  # switch types first, let Surge rebuild
            ref = self.raw["B"].get(key)
            if ref is not None:
                self.sB.setParamVal(ref, float(rec[key]))
        for _ in range(3):
            self.sB.process()
        for key, v in rec.items():
            ref = self.raw["B"].get(key)
            if ref is not None and not isinstance(ref, tuple):
                self.sB.setParamVal(ref, float(v))
        self.b_type = name

    def _refresh_labels(self):
        for voice, inst in self.inst.items():
            lab = {}
            for key, ref in self.raw[voice].items():
                if isinstance(ref, tuple):
                    lab[key] = key[4:]
                else:
                    nm = inst.getParameterName(ref.getId())
                    lab[key] = f"{nm[2:] if nm[:2] in ('A ', 'B ') else nm}: {inst.getParamDisplay(ref)}"
            self.labels[voice] = lab

    def _set_param(self, scene, name, v):
        p = self._P.get((scene, name))
        if p is None:
            return False
        key = (scene, name)
        if self._sent.get(key) == v:
            return True
        self.s.setParamVal(p, float(v))
        self._sent[key] = v
        return True

    def param_display(self, scene, name):
        p = self._P.get((scene, name))
        return self.s.getParamDisplay(p) if p else "-"

    # ------------------------------------------------------------ public (thread-safe)
    def set(self, name, value):
        self.q.put(("set", name, value))

    def set_many(self, d):
        self.q.put(("many", d, None))

    def note_on(self, n):
        self.q.put(("on", n, None))

    def note_off(self, n):
        self.q.put(("off", n, None))

    def panic(self):
        self.q.put(("panic", None, None))

    # ------------------------------------------------------------ audio-thread side
    def _keys_for(self, root):
        n = int(self.state["CLUSTER"])
        if self.state["QUARTER"]:
            base = 60 + 2 * (root - 60)  # 24-EDO: one key = a quarter tone
        else:
            base = root
        return [k for k in (base + i for i in range(n)) if 0 <= k <= 127]

    def _revoice(self):
        want = set()
        for r in self.held:
            want.update(self._keys_for(r))
        for k in self.sounding - want:
            self.s.releaseNote(0, k, 0)
            self.sB.releaseNote(0, k, 0)
        for k in sorted(want - self.sounding):
            self.s.playNote(0, k, 100, 0)
            self.sB.playNote(0, k, 100, 0)
        self.sounding = want

    def _set_quarter(self, on):
        if on == self._tuned_qt:
            return
        if on:
            self.s.loadSCLFile(os.path.join(HERE, "tunings", "24edo.scl"))
            self.sB.loadSCLFile(os.path.join(HERE, "tunings", "24edo.scl"))
        else:
            self.s.retuneToStandardTuning()
            self.sB.retuneToStandardTuning()
        self._tuned_qt = on

    def _drain(self):
        revoice = False
        while True:
            try:
                kind, a, b = self.q.get_nowait()
            except queue.Empty:
                break
            if kind == "set":
                self.state[a] = M.clamp(a, b)
                revoice |= a in ("CLUSTER", "QUARTER")
            elif kind == "many":
                for k, v in a.items():
                    if k in self.state:
                        self.state[k] = M.clamp(k, v)
                    elif k == "LFO_SHAPE":
                        self.lfo.shape = v
                    elif k == "LFO_TARGET":
                        self.lfo_target = v
                revoice |= any(k in ("CLUSTER", "QUARTER") for k in a)
            elif kind == "on":
                if a not in self.held:
                    self.held.append(a)
                    self.last_note = a
                    self.fall_cents = 0.0
                    revoice = True
            elif kind == "off":
                if a in self.held:
                    self.held.remove(a)
                    revoice = True
            elif kind == "panic":
                self.held.clear()
                self.s.allNotesOff()
                self.sB.allNotesOff()
                self.sounding = set()
            elif kind == "lfo":
                setattr(self.lfo, a, b) if a != "target" else setattr(self, "lfo_target", b)
            elif kind == "raw":
                self.raw_overrides[a][b[0]] = b[1]
                self._set_raw(a, b[0], b[1])
            elif kind == "raw_clear":
                self.raw_overrides[a].pop(b, None)
            elif kind == "surge_raw":
                scene, name, norm = b
                self.surge_raw_overrides[a][(scene, name)] = norm
                self._set_surge_raw(a, scene, name, norm)
            elif kind == "surge_raw_clear":
                self.surge_raw_overrides[a].pop(b, None)
            elif kind == "btype":
                self._apply_b_type(a)
            elif kind == "loop_clear":
                self.looper.clear()
            elif kind == "loop_freeze":
                self.looper.frozen = bool(a)
            elif kind == "loop_load":  # optional external audio buffer into the looper
                buf = np.zeros_like(self.looper.buf)
                k = min(a.shape[1], buf.shape[1])
                buf[:, :k] = a[:, :k]
                self.looper.buf = buf
                self.looper.w, self.looper.r = 0, 0.0
                self.looper.frozen = True
        if revoice:
            self._set_quarter(bool(self.state["QUARTER"]))
            self._revoice()

    def _effective_state(self, dt):
        st = dict(self.state)
        self.lfo.rate = st["LFO_RATE"]
        self.lfo.depth = st["LFO_DEPTH"]
        self.lfo.self_amt = st["LFO_SELF"]
        v = self.lfo.tick(dt)
        tg = self.lfo_target
        if tg != "off" and tg in st and st["LFO_DEPTH"] > 0:
            lo, hi = M.LIMITS[tg]
            st[tg] = min(hi, max(lo, st[tg] + v * 0.5 * (hi - lo)))
        return self.matrix.apply(st, dt, v, M.LIMITS)

    def _apply_surge(self, st):
        classic = self.s.getParamVal(self._P[(1, "Osc 1 Type")]) == 0 if hasattr(self, "sB") else True
        for sc, name, v in M.surge_targets(st):
            if name.startswith("Osc 1 ") and not classic:
                continue  # the macros' osc-1 targets only mean something on the Classic oscillator
            v = round(v, 4) if isinstance(v, float) else v
            self._set_param(sc, name, v)
            if sc == 0 and hasattr(self, "sB"):  # DECAY (tape + reverb) also on voice B
                p = self._PB.get((0, name))
                if p is not None and self._sent.get(("B", name)) != v:
                    self.sB.setParamVal(p, float(v))
                    self._sent[("B", name)] = v

    def _pitch_tick(self, st, dt):
        """Host pitch: OFFSET + FALL + JITTER (+ jumps) + tape wow/flutter, sent as pitch bend."""
        self.t += dt
        jitter = max(st["JITTER"], 0.6 * st["BREAK"] ** 2)
        if random.random() < 0.35:
            self.jit_target = random.gauss(0, 1) * 60.0 * jitter
        self.jit += (self.jit_target - self.jit) * 0.5
        # frequency jumps (an NLP): rare sudden steps held for a moment
        if self.jump_t > 0:
            self.jump_t -= dt
            if self.jump_t <= 0:
                self.jump = 0.0
        elif jitter > 0.3 and random.random() < dt * 2.0 * (jitter - 0.3):
            self.jump = random.choice([-1, 1]) * random.uniform(150, 500) * jitter
            self.jump_t = random.uniform(0.05, 0.35)
        if self.held and st["FALL"] > 0:
            self.fall_cents = max(-2400.0, self.fall_cents - st["FALL"] * 300.0 * dt)
        a = st["TAPE"]
        wow = a * (22.0 * math.sin(2 * math.pi * 0.55 * self.t) + 5.0 * math.sin(2 * math.pi * 6.7 * self.t))
        cents = st["OFFSET"] + self.fall_cents + self.jit + self.jump + wow
        bend = int(max(-8192, min(8191, cents / 2400.0 * 8192)))
        self.s.pitchBend(0, bend)
        self.sB.pitchBend(0, bend)
        self.cents = cents

    def render(self, n=BLOCK, input_block=None):
        """Render n frames (multiple of 32), with optional external stereo audio.

        Only the owner of the audio thread may call this method; GUI changes use
        the event queue. `input_block` is the future module-to-module audio port
        and is mixed at EXT_MIX before the resonator. None means silence.
        """
        self._drain()
        dt = n / self.sr
        st = self._effective_state(dt)
        self.eff = st
        self._apply_surge(st)
        # Explicit raw edits have last-write authority over front-panel macros.
        for voice, overrides in self.raw_overrides.items():
            for key, norm in overrides.items():
                self._set_raw(voice, key, norm)
        for voice, overrides in self.surge_raw_overrides.items():
            for (scene, name), norm in overrides.items():
                self._set_surge_raw(voice, scene, name, norm)
        self._pitch_tick(st, dt)
        nb = n // self.bs
        mb = self._mb.get(nb)
        if mb is None:
            mb = self._mb[nb] = self.s.createMultiBlock(nb)
        self.s.processMultiBlock(mb)
        mbB = self._mbB.get(nb)
        if mbB is None:
            mbB = self._mbB[nb] = self.sB.createMultiBlock(nb)
        self.sB.processMultiBlock(mbB)
        x = self._mix(st, mb, mbB, n, input_block=input_block)
        self.tick += 1
        if self.tick % 16 == 1:
            self._refresh_labels()
        self.rough.rate = st["ROUGH_RATE"]
        self.rough.depth = st["ROUGH_DEPTH"]
        x = self.rough.process(x)
        self.looper.speed = st["LOOP_SPEED"]
        self.looper.feedback = st["LOOP_FEEDBACK"]
        self.looper.mix = st["LOOP_MIX"]
        x = self.looper.process(x)
        self.out.gain_db = st["MASTER_DB"]
        y = self.out.process(x)
        self._push(y)
        self.matrix.update_envelope(y)
        if self.rec_on:
            self.rec_chunks.append(y.copy())
        return y

    def _mix(self, st, a, b, n, input_block=None):
        """Stereo A/B generator buses -> optional coupling -> resonant body.

        The GUTTR vocal module can later supply `external` to the stable DSP port,
        without a reference to GUTTUR or a vocal parameter anywhere in CHAOS.
        """
        a = np.asarray(a, dtype=np.float32).reshape(2, n)
        b = np.asarray(b, dtype=np.float32).reshape(2, n)
        def voice(name, signal):
            if st[f"{name}_MUTE"]:
                return np.zeros_like(signal)
            g = 10.0 ** (st[f"{name}_LEVEL"] / 20.0)
            ang = (st[f"{name}_PAN"] + 1.0) * np.pi / 4.0
            pan = np.array([np.cos(ang), np.sin(ang)], np.float32)[:, None] * np.sqrt(2.0)
            return signal * g * pan
        a, b = voice("A", a), voice("B", b)
        out = self.routing.process(a, b, st)
        if input_block is not None and st["EXT_MIX"] > 0:
            external = np.asarray(input_block, dtype=np.float32)
            if external.shape != (2, n) or not np.isfinite(external).all():
                raise ValueError("input_block must be finite float audio, shape (2, n)")
            out = out + st["EXT_MIX"] * external
        out = self.body.process(out, st)
        self.last_stems = {"A": a, "B": b, "CHAOS": out}
        return out

    def start_rec(self):
        self.rec_chunks = []
        self.rec_on = True

    def stop_rec(self):
        self.rec_on = False
        chunks, self.rec_chunks = self.rec_chunks, []
        return np.concatenate(chunks, axis=1) if chunks else None

    def _push(self, y):
        mono = 0.5 * (y[0] + y[1])
        n = len(mono)
        with self.lock:
            p = self.ring_pos
            end = p + n
            if end <= len(self.ring):
                self.ring[p:end] = mono
            else:
                k = len(self.ring) - p
                self.ring[p:] = mono[:k]
                self.ring[: n - k] = mono[k:]
            self.ring_pos = end % len(self.ring)

    def recent(self, n):
        """Last n mono samples, oldest first (for the UI thread)."""
        with self.lock:
            p = self.ring_pos
            r = np.roll(self.ring, -p)
        return r[-n:].copy()

    # ------------------------------------------------------------ offline rendering
    def render_offline(self, seconds, notes=(57,), automation=None, release_at=None):
        """automation(t) -> dict of macro values; returns (2, N) float32."""
        for k in notes:
            self.note_on(k)
        total = int(seconds * self.sr) // BLOCK
        out = []
        for i in range(total):
            t = i * BLOCK / self.sr
            if automation:
                for k, v in automation(t).items():
                    self.set(k, v)
            if release_at is not None and abs(t - release_at) < BLOCK / self.sr:
                for k in notes:
                    self.note_off(k)
            out.append(self.render(BLOCK))
        return np.concatenate(out, axis=1)
