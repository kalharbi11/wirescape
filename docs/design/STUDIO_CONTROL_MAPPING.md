<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio control presentation map (#142)

This is *presentation metadata*, not an engine or patch schema. The IDs below
are the numeric stable IDs in the existing `native/modules/*/module.json`
descriptors. Every control writes via `studio.setParameter(nodeId, id, value)`,
and the `parameterChanged` signal is the only accepted-value feedback. If a
write is rejected, the visual control returns to the last accepted value.

| Module ID | Parameter IDs and presentations |
| --- | --- |
| `essentials.vco` | 0 frequency: primary rotary |
| `essentials.lfo` | 0 rate: primary rotary; 1 depth / 2 offset: related rotaries |
| `essentials.vcf` | 0 cutoff: primary rotary; 1 resonance: related rotary |
| `essentials.ladder` | 0 cutoff: primary rotary; 1 resonance / 2 drive: related rotaries |
| `essentials.adsr` | 0 attack / 1 decay / 3 release: stage sliders; 2 sustain: distinct level rotary |
| `essentials.vca` | 0 gain: horizontal slider |
| `essentials.mixer` | 0 level A / 1 level B: paired vertical faders |
| `essentials.clock` | 0 BPM: primary rotary; 1 ticks/beat: integer stepper; 2 duty: slider; 3 run: Start/Stop toggle (#151) |
| `essentials.function_envelope` | 0 rise / 1 fall: paired rotaries; 2 curve: related rotary |
| `essentials.slew` | 0 rise / 1 fall: paired rotaries |
| `essentials.scale_offset` | 0 scale / 1 offset: paired compact rotaries |
| `essentials.noise` | 0 level: slider |
| `essentials.output` | No parameters; existing output-route selection/status remains |

**Descriptor exceptions.** VCO has no waveform parameter; LFO has no shape
parameter; VCF has no drive parameter. These are deliberately not invented.
Clock ticks/beat (1–24) uses an integer stepper; run (0–1) uses the existing
Clock-local Start/Stop toggle. Neither alters parameter bounds/defaults or phase. Existing
continuous controls retain their current unit-based linear/log mapping.
Unknown/future parameter IDs, including any CHAOS/Plaits modules, use the
shared stock slider rather than an invented skin or catalog registration.

No modulation-value telemetry or animation is generated. Modulation indicators
remain hidden until a real telemetry API exists. The output route is not treated
as a gain parameter. No `Main.qml` layout changes are required (#145 owns it).

To inspect representative appearances, use a Qt-enabled Studio build with a
VCO + VCF/Ladder, ADSR, Mixer and Clock on the patch canvas; screenshots must
be captured from the rendered UI (not synthesized).
