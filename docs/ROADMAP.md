<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Requested module roadmap

Current app inventory and requested scope, October26,2026. Module availability below refers to
the ABI1 application. Isolated ABI2 ports/proofs are not installed app capabilities.
The milestone order belongs to [PLAN.md](PLAN.md); communication details to
[specs/COMMUNICATION_MODULES_PLAN.md](specs/COMMUNICATION_MODULES_PLAN.md), section4.

| Requested module | Current status | Who should build it | Boundary / acceptance |
|---|---|---|---|
| Clock | Built and Windows-tested | Keep: CONTROL-pulse clock | Host transport/event clock is separate |
| VCA with LPG mode | VCA built; LPG missing | Local integration; external LPG DSP prototype possible | Preserve essentials.vca identity, old gain/CV/default behavior |
| ADSR envelope | Built, event-gated | Keep; cable gate enhancement here | No current gate input port; host note/gate events drive it |
| Maths-style Rise/Fall envelope | Built as Function Envelope | Keep: gated AR/curve section | Gate CONTROL input, rise/fall/curve; no full Maths clone |
| Basic sequencer | Missing in Essentials; legacy Python sequencer exists | Core/control owner here first | Ordered note/gate outputs, transport and persisted patterns |
| Chord sequencer + strum | Missing | Core here; chord/strum math can later be delegated | Voice/note ownership and polyphony must exist; not polyphony by stacking mono CV |
| Quantizer | Missing | Core pitch contract first; external kernel afterward | V/oct vs MIDI units, root/scales/ties; existing VCO has no pitch-CV port |
| Plaits or Braids | Missing; sine VCO is not either | Pinned-source port/integration here; later external DSP adapter | Choose one first; preserve upstream notices, sample-rate/block/memory requirements |
| Mixer | Built: two mono audio inputs | Keep | Not stereo, control mixer or general message merge |
| VCF 12/24 dB | Built; numerical review/Windows tests passed | Keep; listening/performance study remains | One low-pass module with separate 12/24 outputs; stability/response tests |
| Ladder filter | Built; numerical review/Windows tests passed | Keep; listening/performance study remains | Original bounded ladder-inspired DSP; name/model claims must match implementation |
| Noise | Built: deterministic white noise | Keep | Deterministic white noise v1; extra colours later |
| Attenuverter / Offset | Built as Scale/Offset | Keep existing module | Scale -2..2 includes inversion; offset -1..1; no clipping |
| Slew / Lag | Linear Slew built; exponential Lag missing | Keep Slew; Lag later | Exponential Lag remains a separate later mode; do not mislabel semantics |
| Oscilloscope | Missing; peak meter exists | Core analysis/UI here; capture/decimation kernels can be delegated later | Bounded tap, no callback locks/allocations, UI-rate snapshots |
| Spectrum monitor | Missing | Core analysis/UI here; FFT/window kernel later | Decimated worker analysis, overlap/window/rate budgets; not FFT in the audio callback |
| MIDI In | Missing | Core provider here | Pure event source; shared endpoint service, filters, timestamps |
| MIDI → CV | Missing in app; allocator/offline proof exists | Core integration here | Poly pitch/gate/velocity/expression, explicit allocation and sustain |
| MIDI CC Map | Missing | Core integration here | Config-dependent stable row IDs, per-row Learn and smoothing |
| MIDI Out | Missing | Core provider here | Notes/MIDI/poly-CV conversion; endpoint scheduler, off/panic on shutdown |
| MIDI Clock | Missing | Core provider here | MIDI clock input sync and transport-derived clock/start/stop output |
| OSC Input | Missing | Core provider here | UDP parsing/address filters, numeric/string/trigger/structured types need message contract |
| OSC Output | Missing | Core provider here | Nonblocking worker, address/value encoding, destination/rate policy |
| UDP Receive | Missing | Core provider here | Bounded text/binary packet sizes, source metadata and ingress policy |
| UDP Send | Missing | Core provider here | Worker-owned socket, text/binary encoding, bounded send/backpressure |
| Monitor (Print) | Missing | Core bounded logging/UI here | One Monitor with MIDI/OSC/UDP decoders; event fan-in +4 signals; log/table, pause/filter/copy/rate/drop counters |

Also built: sine VCO, LFO and Output.13 Essentials currently ship in the app.
Do not commission duplicate versions of Noise, Slew, Function Envelope, Clock, VCF or Ladder;
their original external-source assignments were completed and Windows-tested.

## Contract gates

The ABI1 app has mono AUDIO/CONTROL ports, numeric parameters and host-to-module events.
ABI2's isolated SDK/engine already proves poly cables, event outputs, modulation, voice allocation
and state/query boundaries, but requires review and performance work before product migration.
The VCO pitch input and ADSR gate input exist only in the isolated ABI2 ports today.

Implement communication using shared bounded services, not per-module duplicate port ownership.
MIDI Monitor is replaced by the single general Monitor (Print) plus the always-available Activity
panel. MIDI→CV, MIDI CC Map and MIDI Clock are separate explicit modules, not hidden routing.
Test simulated ingress/egress and localhost before hardware; string/binary data are events,
never ad-hoc float-buffer messages. Use the shared allocator for sequencing/chords.

Scope/Spectrum need bounded taps and UI/worker-rate analysis, not FFT/log formatting inside DSP.
Choose one pinned, licence-verified Plaits/Braids source before considering another instrument.
Hardware reuse is a portable-DSP/thin-adapter goal, not a promise every desktop patch fits Daisy.
