// SPDX-License-Identifier: GPL-3.0-only
// Eight authored, self-contained v2 patches. Test-session fixtures live elsewhere.
// Each patch is cloned before use; no user document is modified.
import type { PatchV2 } from '../patch/types';

export interface CuratedPatch { id: string; name: string; note: string; patch: PatchV2 }

// One module and one cable per line keeps the musical signal paths auditable.
export const CURATED_PATCHES: CuratedPatch[] = [
  {
    id: "hollow-chapel", name: "01 · Hollow Chapel",
    note: "Autoplay · True stereo CHAOS bodies with independently filtered left/right shadows and slow counterphase drift.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "01 · Hollow Chapel", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1101,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"essentials.chaos_lite","parameters":{"shape_a":1.72,"shape_b":1.96,"octave_a":-2,"octave_b":-1,"detune":-17,"route":2,"depth":0.68,"level_a":0.33,"level_b":0.25,"body_tune":-0.52,"feedback":0.72,"damping":0.81,"body_mix":0.7,"roughness":0.08,"drift":0.54,"jitter":0.1,"lfo_target":1,"lfo_rate":0.09,"lfo_depth":0.55,"lfo_self":0.4,"cutoff":2200,"level":0.23},"version":1},
        {"id":2,"module":"essentials.chaos_lite","parameters":{"shape_a":1.95,"shape_b":1.25,"octave_a":-2,"octave_b":-2,"detune":11,"route":1,"depth":0.27,"level_a":0.25,"level_b":0.22,"body_tune":0.5,"feedback":0.62,"damping":0.78,"body_mix":0.56,"roughness":0.1,"drift":0.64,"jitter":0.07,"lfo_target":2,"lfo_rate":0.07,"lfo_depth":0.42,"lfo_self":0.57,"cutoff":2700,"level":0.2},"version":1},
        {"id":3,"module":"essentials.mixer","parameters":{"level_a":0.48,"level_b":0.43},"version":2},
        {"id":4,"module":"essentials.ladder","parameters":{"cutoff":1450,"resonance":0.28,"drive":0.14},"version":2},
        {"id":5,"module":"essentials.output","parameters":{},"version":2},
        {"id":6,"module":"essentials.lfo","parameters":{"rate":0.037,"depth":0.85},"version":2},
        {"id":7,"module":"essentials.lfo","parameters":{"rate":0.071,"depth":0.7},"version":2},
        {"id":8,"module":"essentials.mixer","parameters":{"level_a":0.47,"level_b":0.44},"version":2},
        {"id":9,"module":"essentials.ladder","parameters":{"cutoff":1120,"resonance":0.32,"drive":0.13},"version":2},
        {"id":10,"module":"essentials.output","parameters":{},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"left"},"to":{"module":3,"port":"a"}},
        {"kind":"signal","from":{"module":2,"port":"left"},"to":{"module":3,"port":"b"}},
        {"kind":"signal","from":{"module":3,"port":"audio"},"to":{"module":4,"port":"audio"}},
        {"kind":"signal","from":{"module":4,"port":"audio"},"to":{"module":5,"port":"audio"}},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":1,"port":"body_tune"},"amount":0.16},
        {"kind":"modulation","from":{"module":7,"port":"control"},"to":{"module":2,"port":"cutoff"},"amount":0.22},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":4,"port":"cutoff"},"amount":0.19},
        {"kind":"modulation","from":{"module":7,"port":"control"},"to":{"module":2,"port":"roughness"},"amount":0.09},
        {"kind":"signal","from":{"module":1,"port":"right"},"to":{"module":8,"port":"a"}},
        {"kind":"signal","from":{"module":2,"port":"right"},"to":{"module":8,"port":"b"}},
        {"kind":"signal","from":{"module":8,"port":"audio"},"to":{"module":9,"port":"audio"}},
        {"kind":"signal","from":{"module":9,"port":"audio"},"to":{"module":10,"port":"audio"}},
        {"kind":"modulation","from":{"module":7,"port":"control"},"to":{"module":9,"port":"cutoff"},"amount":-0.16}
      ],
      outputs: [{"module":5,"port":"audio","bus_channel":0},{"module":10,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[80,1270],"3":[740,80],"4":[1400,80],"5":[2060,80],"6":[740,2350],"7":[1400,2350],"8":[740,1270],"9":[1400,1270],"10":[2060,1270]},
        labels: {"1":"Stone choir · low L/R","2":"Stone choir · high L/R","6":"Slow tide","7":"Counterphase haze","8":"Right body mix","9":"Right-hand shadow"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  },
  {
    id: "breathing-walls", name: "02 · Breathing Walls",
    note: "Autoplay · CHAOS delay-bend, a dust-noise layer, and clocked random movement.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "02 · Breathing Walls", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1102,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"essentials.chaos_lite","parameters":{"shape_a":0.55,"shape_b":1.83,"octave_a":-2,"octave_b":-1,"detune":23,"route":2,"depth":0.83,"level_a":0.38,"level_b":0.18,"body_tune":-0.85,"feedback":0.77,"damping":0.9,"body_mix":0.76,"roughness":0.18,"rough_rate":13,"drift":0.72,"jitter":0.16,"lfo_target":1,"lfo_rate":0.13,"lfo_depth":0.66,"lfo_self":0.76,"cutoff":1700,"level":0.22},"version":1},
        {"id":2,"module":"essentials.noise","parameters":{"level":0.08},"version":2},
        {"id":3,"module":"essentials.mixer","parameters":{"level_a":0.52,"level_b":0.18},"version":2},
        {"id":4,"module":"essentials.ladder","parameters":{"cutoff":760,"resonance":0.47,"drive":0.2},"version":2},
        {"id":5,"module":"essentials.output","parameters":{},"version":2},
        {"id":6,"module":"essentials.lfo","parameters":{"rate":0.052,"depth":0.8},"version":2},
        {"id":7,"module":"example.random_source","parameters":{"smooth":0.8,"range":0.5},"version":1},
        {"id":8,"module":"essentials.clock","parameters":{"bpm":27,"ticks_per_beat":1,"duty":0.35,"run":1},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"left"},"to":{"module":3,"port":"a"}},
        {"kind":"signal","from":{"module":2,"port":"audio"},"to":{"module":3,"port":"b"}},
        {"kind":"signal","from":{"module":3,"port":"audio"},"to":{"module":4,"port":"audio"}},
        {"kind":"signal","from":{"module":4,"port":"audio"},"to":{"module":5,"port":"audio"}},
        {"kind":"signal","from":{"module":8,"port":"clock"},"to":{"module":7,"port":"clock"}},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":1,"port":"feedback"},"amount":0.11},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":1,"port":"roughness"},"amount":0.1},
        {"kind":"modulation","from":{"module":7,"port":"smooth"},"to":{"module":1,"port":"body_tune"},"amount":0.19},
        {"kind":"modulation","from":{"module":7,"port":"smooth"},"to":{"module":4,"port":"cutoff"},"amount":0.27}
      ],
      outputs: [{"module":5,"port":"audio","bus_channel":0},{"module":5,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[80,1180],"3":[740,80],"4":[1400,80],"5":[2060,80],"6":[740,1180],"7":[1400,1180],"8":[2060,1180]},
        labels: {"1":"Bent wall","2":"Air leakage","7":"Slow random wander","8":"Structural pulse"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  },
  {
    id: "black-ice-choir", name: "03 · Black Ice Choir",
    note: "Play keys · Long-attack CHAOS choir with a seven-second release and independent body/cutoff tides.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "03 · Black Ice Choir", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1103,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"host.keyboard","parameters":{"voices":4},"version":1},
        {"id":2,"module":"essentials.chaos_lite","parameters":{"shape_a":1.93,"shape_b":1.76,"octave_a":-1,"octave_b":0,"detune":7,"route":2,"depth":0.56,"level_a":0.42,"level_b":0.31,"pan_a":-0.4,"pan_b":0.4,"body_tune":0.14,"feedback":0.68,"damping":0.82,"body_mix":0.63,"roughness":0.07,"drift":0.46,"jitter":0.06,"lfo_target":1,"lfo_rate":0.08,"lfo_depth":0.45,"lfo_self":0.35,"cutoff":3400,"level":0.3},"version":1},
        {"id":3,"module":"essentials.vca","parameters":{"gain":0.65},"version":2},
        {"id":4,"module":"essentials.output","parameters":{},"version":2},
        {"id":5,"module":"essentials.adsr","parameters":{"attack":2.2,"decay":2.8,"sustain":0.76,"release":7.2},"version":2},
        {"id":6,"module":"essentials.lfo","parameters":{"rate":0.043,"depth":0.9},"version":2},
        {"id":7,"module":"essentials.lfo","parameters":{"rate":0.093,"depth":0.8},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"pitch"},"to":{"module":2,"port":"pitch"}},
        {"kind":"signal","from":{"module":1,"port":"gate"},"to":{"module":5,"port":"gate"}},
        {"kind":"signal","from":{"module":2,"port":"left"},"to":{"module":3,"port":"audio"}},
        {"kind":"signal","from":{"module":5,"port":"envelope"},"to":{"module":3,"port":"gain_cv"}},
        {"kind":"signal","from":{"module":3,"port":"audio"},"to":{"module":4,"port":"audio"}},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":2,"port":"body_tune"},"amount":0.17},
        {"kind":"modulation","from":{"module":7,"port":"control"},"to":{"module":2,"port":"cutoff"},"amount":0.28},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":2,"port":"roughness"},"amount":0.07}
      ],
      outputs: [{"module":4,"port":"audio","bus_channel":0},{"module":4,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[740,80],"3":[1400,80],"4":[2060,80],"5":[80,1180],"6":[740,1180],"7":[1400,1180]},
        labels: {"2":"Frozen voices","5":"Slow apparition","6":"Body tide","7":"Filter tide"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  },
  {
    id: "vowel-apparitions", name: "04 · Vowel Apparitions",
    note: "Play keys · Vocal/formant textures with independent vowel and filter-breath modulation.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "04 · Vowel Apparitions", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1104,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"host.keyboard","parameters":{"voices":4},"version":1},
        {"id":2,"module":"ports.macro_oscillator","parameters":{"model":7,"coarse":-12,"harmonics":0.44,"timbre":0.53,"morph":0.37,"timbre_amount":0.36,"morph_amount":0.27,"decay":0.72,"lpg_colour":0.47},"version":1},
        {"id":3,"module":"essentials.vcf","parameters":{"cutoff":2700,"resonance":0.33},"version":2},
        {"id":4,"module":"essentials.vca","parameters":{"gain":0.68},"version":2},
        {"id":5,"module":"essentials.output","parameters":{},"version":2},
        {"id":6,"module":"essentials.adsr","parameters":{"attack":0.8,"decay":1.7,"sustain":0.64,"release":4.8},"version":2},
        {"id":7,"module":"essentials.lfo","parameters":{"rate":0.082,"depth":0.65},"version":2},
        {"id":8,"module":"essentials.lfo","parameters":{"rate":0.031,"depth":0.71},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"pitch"},"to":{"module":2,"port":"pitch"}},
        {"kind":"signal","from":{"module":1,"port":"gate"},"to":{"module":2,"port":"trigger"}},
        {"kind":"signal","from":{"module":1,"port":"velocity"},"to":{"module":2,"port":"level"}},
        {"kind":"signal","from":{"module":1,"port":"gate"},"to":{"module":6,"port":"gate"}},
        {"kind":"signal","from":{"module":2,"port":"main"},"to":{"module":3,"port":"audio"}},
        {"kind":"signal","from":{"module":3,"port":"low12"},"to":{"module":4,"port":"audio"}},
        {"kind":"signal","from":{"module":6,"port":"envelope"},"to":{"module":4,"port":"gain_cv"}},
        {"kind":"signal","from":{"module":4,"port":"audio"},"to":{"module":5,"port":"audio"}},
        {"kind":"modulation","from":{"module":7,"port":"control"},"to":{"module":2,"port":"morph"},"amount":0.19},
        {"kind":"modulation","from":{"module":8,"port":"control"},"to":{"module":3,"port":"cutoff"},"amount":0.2}
      ],
      outputs: [{"module":5,"port":"audio","bus_channel":0},{"module":5,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[740,80],"3":[1400,80],"4":[2060,80],"5":[2720,80],"6":[80,1180],"7":[740,1180],"8":[2060,1180]},
        labels: {"2":"Vocal engine · model 7","6":"Long breath","7":"Vowel drift","8":"Independent filter breath"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  },
  {
    id: "glass-cathedrals", name: "05 · Glass Cathedrals",
    note: "Play keys · Macro model 12, main/aux resonant blend and slowly drifting overtones.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "05 · Glass Cathedrals", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1105,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"host.keyboard","parameters":{"voices":4},"version":1},
        {"id":2,"module":"ports.macro_oscillator","parameters":{"model":12,"coarse":-12,"harmonics":0.34,"timbre":0.6,"morph":0.42,"timbre_amount":0.25,"morph_amount":0.3,"decay":0.88,"lpg_colour":0.58},"version":1},
        {"id":3,"module":"essentials.ladder","parameters":{"cutoff":4500,"resonance":0.18,"drive":0.07},"version":2},
        {"id":4,"module":"essentials.output","parameters":{},"version":2},
        {"id":5,"module":"essentials.lfo","parameters":{"rate":0.11,"depth":0.75},"version":2},
        {"id":6,"module":"essentials.lfo","parameters":{"rate":0.047,"depth":0.7},"version":2},
        {"id":7,"module":"essentials.mixer","parameters":{"level_a":0.72,"level_b":0.19},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"pitch"},"to":{"module":2,"port":"pitch"}},
        {"kind":"signal","from":{"module":1,"port":"gate"},"to":{"module":2,"port":"trigger"}},
        {"kind":"signal","from":{"module":1,"port":"velocity"},"to":{"module":2,"port":"level"}},
        {"kind":"signal","from":{"module":2,"port":"main"},"to":{"module":7,"port":"a"}},
        {"kind":"signal","from":{"module":2,"port":"aux"},"to":{"module":7,"port":"b"}},
        {"kind":"signal","from":{"module":7,"port":"audio"},"to":{"module":3,"port":"audio"}},
        {"kind":"signal","from":{"module":3,"port":"audio"},"to":{"module":4,"port":"audio"}},
        {"kind":"modulation","from":{"module":5,"port":"control"},"to":{"module":2,"port":"timbre"},"amount":0.18},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":3,"port":"cutoff"},"amount":0.2}
      ],
      outputs: [{"module":4,"port":"audio","bus_channel":0},{"module":4,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[740,80],"3":[2060,80],"4":[2720,80],"5":[740,1180],"6":[2060,1180],"7":[1400,80]},
        labels: {"2":"Modal engine · model 12","5":"Glass shimmer","6":"Slow shadow","7":"Modal + overtone blend"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  },
  {
    id: "dust-reactor", name: "06 · Dust Reactor",
    note: "Autoplay · Stock VCO/noise, dual filters, and slowly changing random cutoff.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "06 · Dust Reactor", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1106,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"essentials.vco","parameters":{"frequency":55},"version":2},
        {"id":2,"module":"essentials.noise","parameters":{"level":0.1},"version":2},
        {"id":3,"module":"essentials.mixer","parameters":{"level_a":0.32,"level_b":0.18},"version":2},
        {"id":4,"module":"essentials.vcf","parameters":{"cutoff":510,"resonance":0.53},"version":2},
        {"id":5,"module":"essentials.ladder","parameters":{"cutoff":1750,"resonance":0.23,"drive":0.19},"version":2},
        {"id":6,"module":"essentials.output","parameters":{},"version":2},
        {"id":7,"module":"essentials.clock","parameters":{"bpm":33,"ticks_per_beat":1,"duty":0.4,"run":1},"version":2},
        {"id":8,"module":"example.random_source","parameters":{"smooth":0.73,"range":0.62},"version":1},
        {"id":9,"module":"essentials.lfo","parameters":{"rate":0.061,"depth":0.76},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"audio"},"to":{"module":3,"port":"a"}},
        {"kind":"signal","from":{"module":2,"port":"audio"},"to":{"module":3,"port":"b"}},
        {"kind":"signal","from":{"module":3,"port":"audio"},"to":{"module":4,"port":"audio"}},
        {"kind":"signal","from":{"module":4,"port":"low24"},"to":{"module":5,"port":"audio"}},
        {"kind":"signal","from":{"module":5,"port":"audio"},"to":{"module":6,"port":"audio"}},
        {"kind":"signal","from":{"module":7,"port":"clock"},"to":{"module":8,"port":"clock"}},
        {"kind":"modulation","from":{"module":8,"port":"smooth"},"to":{"module":4,"port":"cutoff"},"amount":0.27},
        {"kind":"modulation","from":{"module":9,"port":"control"},"to":{"module":5,"port":"cutoff"},"amount":0.23},
        {"kind":"modulation","from":{"module":9,"port":"control"},"to":{"module":1,"port":"frequency"},"amount":0.018}
      ],
      outputs: [{"module":6,"port":"audio","bus_channel":0},{"module":6,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[80,1180],"3":[740,80],"4":[1400,80],"5":[2060,80],"6":[2720,80],"7":[740,1180],"8":[1400,1180],"9":[2060,1180]},
        labels: {"1":"Sub-bass sine","2":"Grain field","8":"Random geology","9":"Thermal drift"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  },
  {
    id: "witching-hour", name: "07 · Witching Hour",
    note: "Autoplay · D-minor eight-step ostinato with ties/rests, shaped by a stock filter and envelope.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "07 · Witching Hour", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1107,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"essentials.clock","parameters":{"bpm":82,"ticks_per_beat":2,"duty":0.56,"run":1},"version":2},
        {"id":2,"module":"example.step_sequencer","parameters":{"run":1,"length":8,"pitch1":-0.8333333333333334,"gate1":1,"vel1":0.86,"pitch2":-0.5833333333333334,"gate2":1,"vel2":0.62,"pitch3":-0.25,"gate3":1,"vel3":0.76,"pitch4":0,"gate4":2,"vel4":0.76,"pitch5":-0.16666666666666666,"gate5":1,"vel5":0.88,"pitch6":-0.4166666666666667,"gate6":1,"vel6":0.65,"pitch7":-0.5833333333333334,"gate7":0,"vel7":0.55,"pitch8":-0.25,"gate8":1,"vel8":0.79},"version":1},
        {"id":3,"module":"essentials.vco","parameters":{"frequency":110},"version":2},
        {"id":4,"module":"essentials.vcf","parameters":{"cutoff":1150,"resonance":0.37},"version":2},
        {"id":5,"module":"essentials.vca","parameters":{"gain":0.6},"version":2},
        {"id":6,"module":"essentials.output","parameters":{},"version":2},
        {"id":7,"module":"essentials.adsr","parameters":{"attack":0.006,"decay":0.16,"sustain":0.42,"release":0.23},"version":2},
        {"id":8,"module":"essentials.lfo","parameters":{"rate":0.17,"depth":0.66},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"clock"},"to":{"module":2,"port":"clock"}},
        {"kind":"signal","from":{"module":2,"port":"pitch"},"to":{"module":3,"port":"pitch"}},
        {"kind":"signal","from":{"module":2,"port":"gate"},"to":{"module":7,"port":"gate"}},
        {"kind":"signal","from":{"module":3,"port":"audio"},"to":{"module":4,"port":"audio"}},
        {"kind":"signal","from":{"module":4,"port":"low24"},"to":{"module":5,"port":"audio"}},
        {"kind":"signal","from":{"module":7,"port":"envelope"},"to":{"module":5,"port":"gain_cv"}},
        {"kind":"signal","from":{"module":5,"port":"audio"},"to":{"module":6,"port":"audio"}},
        {"kind":"modulation","from":{"module":8,"port":"control"},"to":{"module":4,"port":"cutoff"},"amount":0.18}
      ],
      outputs: [{"module":6,"port":"audio","bus_channel":0},{"module":6,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[740,80],"3":[1400,80],"4":[2060,80],"5":[2720,80],"6":[3380,80],"7":[80,1180],"8":[740,1180]},
        labels: {"2":"D minor · eight steps","3":"Ostinato voice","7":"Note contour"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  },
  {
    id: "neon-elegy", name: "08 · Neon Elegy",
    note: "Autoplay · A-minor/F/C/G-inspired eight-step line through macro model 2 (FM) and a warm ladder.",
    patch: {
      "SPDX-License-Identifier": "GPL-3.0-only",
      name: "08 · Neon Elegy", format: "wirescape.native.patch", version: 2,
      settings: {"sample_rate":48000,"max_block_frames":128,"max_channels":16,"feedback_block":32,"seed":1108,"timing_mode":"immediate"},
      modules: [
        {"id":1,"module":"essentials.clock","parameters":{"bpm":94,"ticks_per_beat":2,"duty":0.58,"run":1},"version":2},
        {"id":2,"module":"example.step_sequencer","parameters":{"run":1,"length":8,"pitch1":-0.25,"gate1":1,"vel1":0.78,"pitch2":0,"gate2":1,"vel2":0.64,"pitch3":0.3333333333333333,"gate3":1,"vel3":0.84,"pitch4":0.5833333333333334,"gate4":1,"vel4":0.6,"pitch5":-0.5833333333333334,"gate5":1,"vel5":0.83,"pitch6":-0.25,"gate6":2,"vel6":0.83,"pitch7":0,"gate7":1,"vel7":0.71,"pitch8":-0.4166666666666667,"gate8":0,"vel8":0.5},"version":1},
        {"id":3,"module":"ports.macro_oscillator","parameters":{"model":2,"coarse":-12,"harmonics":0.38,"timbre":0.62,"morph":0.31,"timbre_amount":0.29,"morph_amount":0.24,"decay":0.3,"lpg_colour":0.59},"version":1},
        {"id":4,"module":"essentials.ladder","parameters":{"cutoff":3300,"resonance":0.24,"drive":0.12},"version":2},
        {"id":5,"module":"essentials.output","parameters":{},"version":2},
        {"id":6,"module":"essentials.lfo","parameters":{"rate":0.093,"depth":0.72},"version":2}
      ],
      connections: [
        {"kind":"signal","from":{"module":1,"port":"clock"},"to":{"module":2,"port":"clock"}},
        {"kind":"signal","from":{"module":2,"port":"pitch"},"to":{"module":3,"port":"pitch"}},
        {"kind":"signal","from":{"module":2,"port":"gate"},"to":{"module":3,"port":"trigger"}},
        {"kind":"signal","from":{"module":2,"port":"gate"},"to":{"module":3,"port":"gate"}},
        {"kind":"signal","from":{"module":2,"port":"velocity"},"to":{"module":3,"port":"level"}},
        {"kind":"signal","from":{"module":3,"port":"main"},"to":{"module":4,"port":"audio"}},
        {"kind":"signal","from":{"module":4,"port":"audio"},"to":{"module":5,"port":"audio"}},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":3,"port":"morph"},"amount":0.16},
        {"kind":"modulation","from":{"module":6,"port":"control"},"to":{"module":4,"port":"cutoff"},"amount":0.18}
      ],
      outputs: [{"module":5,"port":"audio","bus_channel":0},{"module":5,"port":"audio","bus_channel":1}],
      ui: {
        positions: {"1":[80,80],"2":[740,80],"3":[1400,80],"4":[2060,80],"5":[2720,80],"6":[80,1180]},
        labels: {"2":"A minor · eight steps","3":"FM engine · model 2","6":"Shifting harmonics"}, view: {"x":0,"y":0,"zoom":1},
      },
    },
  }
];
