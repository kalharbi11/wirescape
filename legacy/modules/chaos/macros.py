"""The paper-mapped macros.

Every macro is a plain number in `state`. `surge_targets(state)` turns the state into concrete Surge
parameter values (names verified against params.json from tools/introspect.py). Host-side pieces
(roughness AM, jitter, falling pitch, looper, wow) read the same state in engine.py.

Scene A = scene index 1 in surgepy; global/master FX = scene 0.
"""

# name: (lo, hi, default)
SPEC = {
    # THE SCREAM: linear -> nonlinear
    "BREAK": (0.0, 1.0, 0.0),        # master: drive + FM + ring + jitter + sub + feedback
    "DRIVE_TYPE": (0, 7, 0),         # index into DRIVE_TYPES
    "ROUGH_RATE": (5.0, 150.0, 70.0),  # Hz, host AM (Fig 2: scream band 30-150 Hz)
    "ROUGH_DEPTH": (0.0, 1.0, 0.0),
    "JITTER": (0.0, 1.0, 0.0),       # random pitch wobble + occasional frequency jumps
    "SUB": (0.0, 1.0, 0.0),          # subharmonic: Classic osc sub-oscillator
    "FM_RATIO": (0.0, 1.0, 0.0),     # 0 = harmonic (1:1); up = non-integer ratio (biphonation)
    # THE UNSTABLE
    "DETUNE": (0.0, 60.0, 0.0),      # cents between the unison voices (Fig 4)
    "VOICES": (1, 9, 2),             # unison voices on osc 1
    "DRIFT": (0.0, 1.0, 0.0),        # Surge's per-voice Osc Drift
    "OFFSET": (-50.0, 50.0, 0.0),    # cents off the 12-note grid (Fig 5B)
    "CLUSTER": (1, 12, 1),           # number of adjacent notes per key (Fig 7B)
    "QUARTER": (0, 1, 0),            # 1 = cluster steps are quarter tones (24-EDO), Fig 7C
    "FALL": (0.0, 1.0, 0.0),         # pitch sinks while notes are held
    "CUTOFF": (0.0, 1.0, 0.75),
    # DECAY
    "TAPE": (0.0, 1.0, 0.0),         # 'AGE': Surge Tape FX + host wow/flutter
    "SPACE": (0.0, 1.0, 0.4),        # reverb size/decay
    "SPACE_MIX": (0.0, 1.0, 0.0),
    # LOOPER (Oneiroi)
    "LOOP_SPEED": (-2.0, 2.0, 1.0),
    "LOOP_FEEDBACK": (0.0, 0.95, 0.0),
    "LOOP_MIX": (0.0, 1.0, 0.0),
    # SELF-MOD LFO (Oneiroi)
    "LFO_RATE": (0.02, 12.0, 0.3),
    "LFO_DEPTH": (0.0, 1.0, 0.0),
    "LFO_SELF": (0.0, 1.0, 0.0),
    # OUTPUT
    "MASTER_DB": (-48.0, 6.0, 0.0),
    # A and B are Surge generator layers, neither depends on a vocal processor.
    "A_LEVEL": (-48.0, 6.0, 0.0), "A_PAN": (-1.0, 1.0, 0.0), "A_MUTE": (0, 1, 0),
    "B_LEVEL": (-48.0, 6.0, -9.0), "B_PAN": (-1.0, 1.0, 0.0), "B_MUTE": (0, 1, 1),
    # Architecture prototype: 0=parallel, 1=AM/ring, 2=audio-controlled delay bend.
    # All off by default, preserving the existing clean Surge voice.
    "ROUTE_MODE": (0, 2, 0), "ROUTE_DEPTH": (0.0, 1.0, 0.0),
    "BODY_MIX": (0.0, 1.0, 0.0), "BODY_TONE": (0.0, 1.0, 0.5),
    "BODY_FEED": (0.0, 0.85, 0.0), "EXT_MIX": (0.0, 1.0, 0.0),
    # A few composable modulation slots, separate from Surge's own per-voice LFO routings.
    "MOD1_SRC": (0, 4, 0), "MOD1_DST": (0, 11, 0), "MOD1_AMT": (-1.0, 1.0, 0.0),
    "MOD2_SRC": (0, 4, 0), "MOD2_DST": (0, 11, 0), "MOD2_AMT": (-1.0, 1.0, 0.0),
    "MOD3_SRC": (0, 4, 0), "MOD3_DST": (0, 11, 0), "MOD3_AMT": (-1.0, 1.0, 0.0),
    "MOD4_SRC": (0, 4, 0), "MOD4_DST": (0, 11, 0), "MOD4_AMT": (-1.0, 1.0, 0.0),
}

DEFAULTS = {k: v[2] for k, v in SPEC.items()}
LIMITS = {k: (v[0], v[1]) for k, v in SPEC.items()}
INT_MACROS = {"DRIVE_TYPE", "VOICES", "CLUSTER", "QUARTER", "A_MUTE", "B_MUTE", "ROUTE_MODE"} | {f"MOD{i}_{key}" for i in range(1, 5) for key in ("SRC", "DST")}

# Surge waveshaper type ids (from introspection): name -> id
DRIVE_TYPES = [("Soft", 1), ("Hard", 2), ("Asymmetric", 3), ("Digital", 5),
               ("Double Fold", 15), ("West Coast", 16), ("Heavy Fuzz", 26), ("OJD", 41)]

FX_TAPE, FX_REVERB2 = 23, 11  # Surge fxt_ enum


def clamp(name, v):
    lo, hi = LIMITS[name]
    v = min(hi, max(lo, v))
    return int(round(v)) if name in INT_MACROS else v


# ---------------------------------------------------------------- voice + fx setup (once)
INIT_VOICE = [
    (1, "Osc 1 Type", 0),            # Classic (saw)
    (1, "Osc 2 Type", 1),            # Sine: FM modulator / ring partner
    (1, "Osc 2 Volume", 0.0),        # heard only through FM and ring mod
    (1, "Osc 3 Volume", 0.0),
    (1, "Filter 1 Type", 10),        # LP Vintage Ladder
    (1, "Filter 1 Resonance", 0.25),
    (1, "Amp EG Attack", -6.0),
    (1, "Amp EG Sustain", 1.0),
    (1, "Amp EG Release", 0.0),
    (1, "Pitch Bend Up Range", 24),
    (1, "Pitch Bend Down Range", 24),
    (1, "Polyphony Limit", 32),
    (0, "Polyphony Limit", 32),
    (0, "FX G1 FX Type", FX_TAPE),
    (0, "FX G2 FX Type", FX_REVERB2),
]


def surge_targets(st):
    """state -> list of (scene, param name, value). Curves keep the first half of each knob subtle."""
    b = st["BREAK"]
    t = []
    # --- THE SCREAM
    ws_type = DRIVE_TYPES[int(st["DRIVE_TYPE"])][1]
    t.append((1, "Waveshaper Type", ws_type if b > 0.01 else 0))
    t.append((1, "Waveshaper Drive", -6.0 + 30.0 * b ** 1.2))
    t.append((1, "FM Routing", 1 if (b > 0.01 or st["FM_RATIO"] > 0.01) else 0))  # 2 > 1
    t.append((1, "FM Depth", -48.0 + 64.0 * b ** 1.5))
    t.append((1, "Osc 2 Pitch", 7.0 * st["FM_RATIO"]))  # non-integer ratio -> inharmonic sidebands
    t.append((1, "Ring Modulation 1x2 Mute", 0 if b > 0.05 else 1))
    t.append((1, "Ring Modulation 1x2 Volume", 0.8 * b ** 2))
    t.append((1, "Feedback", 0.35 * b ** 2))
    t.append((1, "Osc 1 Sub Mix", max(st["SUB"], 0.5 * b ** 2)))
    t.append((1, "Volume", 1.0 - 0.35 * b))  # loudness compensation as it breaks
    # --- THE UNSTABLE
    voices = int(st["VOICES"]) if st["DETUNE"] > 0.05 else 1
    t.append((1, "Osc 1 Unison Voices", voices))
    t.append((1, "Osc 1 Unison Detune", st["DETUNE"] / 100.0))  # Surge: 0.01 = 1 cent
    t.append((1, "Osc Drift", st["DRIFT"]))
    t.append((1, "Filter 1 Cutoff", -20.0 + 90.0 * st["CUTOFF"]))
    # --- DECAY: Tape (G1) and Reverb 2 (G2)
    a = st["TAPE"]
    t += [(0, "FX G1 Mix", min(1.0, a * 3.0)), (0, "FX G1 Drive", 0.45 + 0.45 * a),
          (0, "FX G1 Saturation", 0.4 + 0.5 * a), (0, "FX G1 Speed", 30.0 - 24.0 * a),
          (0, "FX G1 Gap", 1.0 + 25.0 * a ** 2), (0, "FX G1 Depth", a),
          (0, "FX G1 Amount", a * 0.8), (0, "FX G1 Variance", a)]
    sp = st["SPACE"]
    t += [(0, "FX G2 Room Size", -0.6 + 1.6 * sp), (0, "FX G2 Decay Time", -1.0 + 6.0 * sp),
          (0, "FX G2 Mix", st["SPACE_MIX"]), (0, "FX G2 HF Damping", 0.5)]
    return t
