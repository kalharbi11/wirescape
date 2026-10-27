"""Surge XT A/B raw parameter groups and second oscillator-voice starting patches.
No vocal synthesis or throat model is included in CHAOS.
"""
import numpy as np
from scipy.signal import lfilter

SR = 48000
TAU = 2 * np.pi

# ------------------------------------------------------------------ Surge raw panel
def _osc(k):
    return [f"Osc {k} Type", f"Osc {k} Octave", f"Osc {k} Pitch"] + [f"Osc {k} P{j}" for j in range(1, 8)] + \
           [f"Osc {k} Volume", f"Osc {k} Mute"]

RAW_GROUPS = [
    ("OSC 1", _osc(1)), ("OSC 2", _osc(2)), ("OSC 3", _osc(3)),
    ("NOISE / SCENE", ["Noise Volume", "Noise Color", "Octave", "Portamento", "Pan", "Width"]),
    ("FILTERS", ["Filter Configuration", "Filter 1 Type", "Filter 1 Subtype", "Filter 1 Cutoff", "Filter 1 Resonance",
                 "Filter 1 FEG Mod Amount", "Filter 1 Keytrack", "Filter 2 Type", "Filter 2 Cutoff",
                 "Filter 2 Resonance", "Highpass"]),
    ("AMP ENV", ["Amp EG Attack", "Amp EG Decay", "Amp EG Sustain", "Amp EG Release"]),
    ("FILTER ENV", ["Filter EG Attack", "Filter EG Decay", "Filter EG Sustain", "Filter EG Release"]),
    ("LFO 1", ["LFO 1 Type", "LFO 1 Rate", "LFO 1 Amplitude", "LFO 1 Deform", "LFO 1 Phase"]),
    ("LFO 2", ["LFO 2 Type", "LFO 2 Rate", "LFO 2 Amplitude", "LFO 2 Deform", "LFO 2 Phase"]),
    ("MOD (centre = off)", ["MOD LFO1>CUTOFF", "MOD LFO1>PITCH", "MOD LFO2>OSC1 P1", "MOD LFO2>CUTOFF",
                            "MOD VELOCITY>CUTOFF"]),
]
# (source constant name, target raw key)
MODS = {"MOD LFO1>CUTOFF": ("ms_lfo1", "Filter 1 Cutoff"), "MOD LFO1>PITCH": ("ms_lfo1", "Osc 1 Pitch"),
        "MOD LFO2>OSC1 P1": ("ms_lfo2", "Osc 1 P1"), "MOD LFO2>CUTOFF": ("ms_lfo2", "Filter 1 Cutoff"),
        "MOD VELOCITY>CUTOFF": ("ms_velocity", "Filter 1 Cutoff")}

# Voice B starting points: raw key -> natural value (Surge units)
B_TYPES = {
    "warm pad": {"Osc 1 Type": 0, "Osc 1 P7": 3, "Osc 1 P6": 0.12, "Filter 1 Type": 10, "Filter 1 Cutoff": 10,
                 "Amp EG Attack": -1.0, "Amp EG Release": 1.0, "Filter 1 Resonance": 0.2},
    "speech": {"Osc 1 Type": 10, "Osc 1 P1": 7, "Filter 1 Type": 0, "Amp EG Attack": -4.0, "Amp EG Release": 0.5},
    "formant": {"Osc 1 Type": 10, "Osc 1 P1": 3, "Filter 1 Type": 0, "Amp EG Attack": -3.0, "Amp EG Release": 0.5},
    "glass": {"Osc 1 Type": 1, "Osc 2 Type": 1, "Osc 2 Octave": 1, "Osc 2 Volume": 0.4, "Filter 1 Type": 0,
              "Amp EG Attack": -2.0, "Amp EG Release": 1.5},
    "string": {"Osc 1 Type": 9, "Filter 1 Type": 1, "Filter 1 Cutoff": 30, "Amp EG Release": 0.5},
    "wavetable": {"Osc 1 Type": 2, "Filter 1 Type": 2, "Filter 1 Cutoff": 25, "Amp EG Attack": -2.0,
                  "Amp EG Release": 1.0},
}

