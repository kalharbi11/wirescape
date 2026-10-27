# Requested module status and ownership

Audited October 26, 2026 against current descriptors/source. A legacy Python/browser module does not count as a current Essentials module.

| Requested module | Current status | Who should build it | Boundary / acceptance |
|---|---|---|---|
| Clock | Missing | External READY: control-pulse clock only | Host transport/event clock is separate |
| VCA with LPG mode | VCA built; LPG missing | Local integration; external LPG DSP prototype possible | Preserve essentials.vca identity, old gain/CV/default behavior |
| ADSR envelope | Built, event-gated | Keep; cable gate enhancement here | No current gate input port; host note/gate events drive it |
| Maths-style Rise/Fall envelope | Missing | External READY: original one-channel Function Envelope | Gate CONTROL input, rise/fall/curve; no full Maths clone |
| Basic sequencer | Missing in Essentials; legacy Python sequencer exists | Core/control owner here first | Ordered note/gate outputs, transport and persisted patterns |
| Chord sequencer + strum | Missing | Core here; chord/strum math can later be delegated | Voice/note ownership and polyphony must exist; not polyphony by stacking mono CV |
| Quantizer | Missing | Core pitch contract first; external kernel afterward | V/oct vs MIDI units, root/scales/ties; existing VCO has no pitch-CV port |
| Plaits or Braids | Missing; sine VCO is not either | Pinned-source port/integration here; later external DSP adapter | Choose one first; preserve upstream notices, sample-rate/block/memory requirements |
| Mixer | Built: two mono audio inputs | Keep | Not stereo, control mixer or general message merge |
| VCF 12/24 dB | Missing | External READY, advanced review | One low-pass module with separate 12/24 outputs; stability/response tests |
| Ladder filter | Missing | External READY, advanced review | Original bounded ladder-inspired DSP; name/model claims must match implementation |
| Noise | Missing | External READY, easiest first | Deterministic white noise v1; extra colours later |
| Attenuverter / Offset | Built as Scale/Offset | Keep existing module | Scale -2..2 includes inversion; offset -1..1; no clipping |
| Slew / Lag | Missing | External READY: linear Slew v1 | Exponential Lag remains a separate later mode; do not mislabel semantics |
| Oscilloscope | Missing; peak meter exists | Core analysis/UI here; capture/decimation kernels can be delegated later | Bounded tap, no callback locks/allocations, UI-rate snapshots |
| Spectrum monitor | Missing | Core analysis/UI here; FFT/window kernel later | Decimated worker analysis, overlap/window/rate budgets; not FFT in the audio callback |
| MIDI In | Missing | Core provider here | Device lifecycle, one enqueue owner, timestamps, channel/note identities |
| MIDI Out | Missing | Core provider here | Scheduled event sink runs even without an audio-output path; off/panic on shutdown |
| MIDI Monitor | Missing | Core provider/UI here | Share MIDI service; bounded logging, no duplicate port ownership |
| OSC Input | Missing | Core provider here | UDP parsing/address filters, numeric/string/trigger/structured types need message contract |
| OSC Output | Missing | Core provider here | Nonblocking worker, address/value encoding, destination/rate policy |
| UDP Receive | Missing | Core provider here | Bounded text/binary packet sizes, source metadata and ingress policy |
| UDP Send | Missing | Core provider here | Worker-owned socket, text/binary encoding, bounded send/backpressure |
| Monitor / Print | Missing | Core bounded logging/UI here | Pause/clear/filter/copy, acquisition timestamps and dropped-entry counter |

Also built: sine VCO, LFO and Output. These complement the requested list and should not be rebuilt by external agents.

## Sequence

1. Hand out Noise and Slew first; then Function Envelope and control Clock. One task per conversation; folders/IDs below are reserved to avoid collisions.
2. VCF and Ladder are independent DSP work but need deeper numerical review here before acceptance. A returned ZIP or a green example test is not evidence its filters are stable or sound correct.
3. Locally settle pitch/gate/event, transport, note/voice ownership and persistence contracts; then Quantizer/basic sequencer/chord+strum and VCA LPG integration. Keep existing ADSR/VCA/Scale-Offset/Mixer IDs and saved parameter meanings.
4. Implement bounded provider ingress/egress and logs once for MIDI/OSC/UDP and their monitors. Test with simulated input and loopback before devices. String/binary/message traffic never becomes an audio-buffer hack.
5. Add bounded shared analysis taps for Scope/Spectrum. UI formatting/log copying and FFT analysis stay outside rendering.
6. Port ONE of Plaits/Braids from pinned official DSP sources, adapting initialization, sample rate, render quantum, memory and notes; compare before a second instrument or polyphony expansion.

The current ABI has AUDIO/CONTROL mono buffer ports, and host-to-processor PARAMETER/NOTE_ON/NOTE_OFF/GATE events. It has NO module event-output API, polyphonic cable lanes, strings/binary/structured message ports or arbitrary module-state serializer. Generic parameter UI has numeric sliders rather than richer enums/custom editors. F06 provider bridge is an OFFLINE one-owner proof, not a live device service. None of this should be bypassed by editing copied headers in a returned module.

The first four tasks are useful scalar/control modules with explicit local conventions. A Clock buffer is not a MIDI clock output or a host transport. The gate input in Function Envelope is CONTROL 0/1; the existing ADSR is event-controlled. These are distinct paths until local integration defines an adapter.
