<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Beta 0.8 musical core — reference-behaviour research brief (B08-0, issue #60)

**Status:** docs-only proposal for Claude; **not** a change to `docs/PLAN.md`, `docs/specs/**` or the ABI. This brief covers precisely **MIDI In, MIDI→CV, Quantizer and Basic Sequencer**. The Beta 0.8 milestone in [PLAN](../PLAN.md) is *Preview 0.7 plus these four modules*, ending in a **60-minute external-keyboard live session with no stuck notes, dropouts or crashes**. Do not quietly implement the rest of M4/M6 (MIDI Out, CC Map, MIDI Clock, full transport UI, chord/strum, MPE or full Scala editor).

## 0. Contract, notation, dependencies and authoritative references

- [ABI2 D2](../specs/ABI2_PLAN.md): **one shared portable allocator**, 1–16 *voice channels* per signal cable; mono-to-poly broadcast, change channel count without allocation; explicit Rotate/Reuse/Reset/Mono modes, policy and saved voice count. No hidden channel assignment.
- [ABI2 D4](../specs/ABI2_PLAN.md): `pitch=(MIDI_note-60)/12`; C4/MIDI 60 = `0.0`, MIDI 72 = `+1.0`, MIDI 48 = `-1.0`. `gate` is 0 or 1; `trigger` is a rising-edge pulse **at least 1 ms**, not a 10 V Eurorack signal. CV bipolar -1..1; unipolar 0..1. Pitch bend expressed in *semitones* is added to pitch in units of 1/12 octave.
- [ABI2 D8](../specs/ABI2_PLAN.md): events have sample offsets and stable same-offset order; dedicated `MIDI` events with UMP-style fields, output `NOTE_ON`/`NOTE_OFF`/`NOTE_CHOKE` and `note_id`; bounded producer rings, note-off reservation. D9 transport **may be absent**; D11 no realtime allocations, D12 persisted state, D13 host ingress. Module DSP remains C++17; a separate non-audio MIDI service is permitted C++20.
- [Communication plan `2–4.2](../specs/COMMUNICATION_MODULES_PLAN.md): device service owns physical endpoints, enumeration, timestamping, reconnect, ring accounting; portable MIDI In module has no device API. Endpoint labels/system names must be editable, not enumerated-order guesses; disconnect injects releases; the MIDI→CV output set and allocator modes are specified there. The **Beta minimum** can implement only the infrastructure required for these two MIDI modules, without rebranding that subset as full Track C completion.
- [ROADMAP](../ROADMAP.md): all four are missing in the ABI1 installed app. The existing Clock, ADSR, sine VCO, VCA and Scale/Offset are reuse targets. Isolated ABI2 ports/proofs are not an already-integrated Beta 0.8 experience.
- Identifiers below are **proposed stable public IDs**, not approvals. Use lowercase ASCII snake_case for ports/params; persist IDs and numeric slot indices once shipped. Prototype candidate module IDs: `midi.in`, `midi.to_cv`, `essentials.quantizer` and `essentials.basic_sequencer`. Claude must ratify naming *before* implementation.

### Reference evidence (behavioural inspirations only; not implementation imports)

| Reference | Direct evidence / relevant design lesson |
|---|---|
| [VCV Rack Core MIDI-CV](https://vcvrack.com/manual/Core) and [VCV polyphony](https://vcvrack.com/manual/Polyphony) | Per-device MIDI events become V/OCT, gate, velocity, aftertouch, pitch wheel and mod-wheel; distinct retrigger; visible voice count; Rotate/Reuse/Reset voice allocation. Source uses 10 V gates; **Wirescape uses 0/1 instead**. |
| [MIDI Association: Control Change](https://midi.org/midi-1-0-control-change-messages), [MIDI 1.0 message summary](https://midi.org/summary-of-midi-1-0-messages) | CC64 sustain: 0–63 off, 64–127 on; pitch bend is 14-bit and center = 8192; pitch bend sensitivity can be set with RPN 0. MIDI 1.0 Note On at velocity zero is note-off (also discussed in [Windows MIDI Services' translation note](https://microsoft.github.io/MIDI/kb/data-translation/)); MIDI 2.0 Note On at zero velocity is **not** equivalent. |
| [Doepfer A-156](https://doepfer.de/a156.htm) | Continuous nearest-semitone quantization or selectable scale/chord; optional trigger-sampled quantization and separate trigger when conversion happens. Our reference is the behaviour, **not** analogue ±10 V ranges. |
| [Mutable Instruments Marbles manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/marbles/manual/) | User-selectable quantization scales, output behaviour coordinated with clocked CV and gates. Its randomized algorithms/probability distributions are **out of scope**. |
| [VCV Fundamental SEQ-3 source](https://github.com/VCVRack/Fundamental/blob/v2/src/SEQ3.cpp) | Concrete stepped clock/run/reset, gate enable, pitch CV and step index design. It has **eight** steps; our proposed 16-step size is not being attributed to it. |
| [Arturia BeatStep Pro](https://www.arturia.com/products/hybrid-synths/beatstep-pro/overview) | Hardware example of 16-step editing, note/velocity/gate time, ties, independent swing and external clock. These inform a restrained monophonic starter; not a clone. |
| [Scala `.scl` specification](https://www.huygens-fokker.org/scala/scl_format.html) | Comments prefixed `!`; count and interval list; intervals may be cents or ratios; implicit 1/1; `.kbm` is **separate** keyboard mapping. Do **not** claim full tuning support when only 12-TET scale masks exist. |
| [VCV voltage standards](https://vcvrack.com/manual/VoltageStandards) | 1 V/oct anchored at C4; clock/reset racing is a documented concern, so choose a deterministic same-frame precedence. Wirescape's digital gate range still follows D4. |

**Scope boundary:** reference these products as prior art; do not copy their source or promise exact UI/DSP equivalence. Reference links establish observed behaviours; all numeric values and policies **proposed below** are Wirescape design recommendations needing Claude approval.

## 1. MIDI In — endpoint-filtered event source

**Reference:** VCV Core MIDI-CV's selectable input driver/device and channel routing; Wirescape intentionally splits **MIDI In** from **MIDI→CV** to allow one source to feed multiple processors (see references and [Communication plan](../specs/COMMUNICATION_MODULES_PLAN.md)).

**Module ID:** `midi.in` (proposed), display **MIDI In**.

| Item | ID / type | Range / proposed default | Behavior |
|---|---|---|---|
| Input ports | *none* | — | Ingress from shared host MIDI service, **never** `libremidi` from `process`. |
| Output port | `midi` / EVENT | 1 stream | Original bounded UMP-style MIDI events including channel/group, endpoint identity, timestamp-to-frame offset; never convert MIDI into a float signal here. |
| Config (D12) | `endpoint` | specific saved label + system-name fallback / default `all_enabled` | Host resolves against explicitly enabled endpoints; enumeration order never a persistent ID. Device missing ⇒ editable/offline, no silent substitution. |
| Parameter | `channel_filter` / STEPPED | `0` = Omni, `1..16` = MIDI channels; default `0` | Filter channel messages only by channel; preserve identity of origin/source. |
| Config/possible parameter | `group_filter` | default all / **to decide** | UMP group handling should not unintentionally mix MIDI2 groups. For Beta MIDI 1.0, define group 0 mapping; full group UI later. |

**Events:** preserve frames/ingress ordering and data on fan-out. Channel-specific note, CC, pitch wheel, pressure pass through if matching. Timing/system realtime messages (when representable) pass regardless of a voice-channel filter; nonchannel messages must be documented and safely ignored if unsupported, **not** reinterpret as channel 1. Zero-velocity MIDI **1.0** Note On may be kept raw on this port so downstream can normalize it; if service normalizes instead, it must preserve MIDI1/MIDI2 semantics and be tested.

**Safety/edge cases:** multiple instances bound to one endpoint must not reopen it; "all enabled" is an explicit choice; disconnected device emits balanced release information downstream, marks status, increments counters and **never blocks patch loading**. Configure resolution/rebinding while quiescent; no device scanning, locks, memory allocation or I/O in realtime callback. Endpoint overflow has observable counters and reserves note-offs according to D8. No physical MIDI or microphone tests in CI.

**Implementable fake-backend tests:**

| Case | Feed to fake service / configuration | Expected `midi` output and state |
|---|---|---|
| IN-01 channel filter | endpoint A, events ch1 note 60 at frame 6, ch2 note 62 at frame 9; filter=1 | **Only ch1** event at offset 6, unchanged payload/endpoint |
| IN-02 omni fanout | same event stream, Omni; two MIDI In subscribers A and B | Each sees both events once, order 6 then 9; service endpoint opened exactly once |
| IN-03 two endpoints | A and B both send ch1 note60, distinct source IDs; select A | Only A event; after changing to `all_enabled` both source identities preserved, deterministic tie order |
| IN-04 disconnect | on A send MIDI1 Note On 60 and sustain; service reports A unplugged | Downstream release/choke for A's tracked notes, no stuck gates; B notes unaffected; source marked offline |
| IN-05 malformed/overflow | out-of-range status, ring saturation plus NOTE_OFF, reordered equal-time messages | Invalid events counted, no OOB; NOTE_OFF survives D8 reserve and deterministic tie ordering; counters expose drops |
| IN-06 MIDI protocol zero | MIDI1 Note On(60,0) vs MIDI2 Note On(60,0) | Distinct typed messages until explicitly decoded; **never** universally rewrite both as note-off |
| IN-07 persistence | save endpoint=A, filter=2; reopen with endpoints reordered/missing | Same binding identity and filter; missing shows offline, no automatic reassignment |

## 2. MIDI→CV — stable polyphonic voice allocation

**Reference:** VCV Core MIDI-CV explicitly offers Rotate, Reuse, Reset, poly voice count, velocity, pitch wheel and separate retrigger. MIDI Association defines CC64 and 14-bit pitch bending. MPE is mentioned by [Communication plan `4.2](../specs/COMMUNICATION_MODULES_PLAN.md), but **is not designed in this Beta brief**.

**Module ID:** `midi.to_cv` (proposed), display **MIDI→CV**.

| Direction | Stable port ID / kind | Range / meaning |
|---|---|---|
| In | `midi` / EVENT | UMP-style events from MIDI In; no direct hardware device access |
| Out | `pitch` / POLY SIGNAL | D4 1/octave, note 60 = 0; optional pitch wheel combined (explicit parameter) |
| Out | `gate` / POLY SIGNAL | 0 or 1 per assigned channel; legato can remain high |
| Out | `velocity` / POLY SIGNAL | 0..1 note-on velocity |
| Out | `aftertouch` / POLY SIGNAL | 0..1 channel-pressure value (broadcast to voices belonging to that MIDI channel); separate from poly key pressure |
| Out | `pressure` / POLY SIGNAL | 0..1 poly key pressure / per-voice expression where supported; 0 if absent |
| Out | `bend` / POLY SIGNAL | normalized -1..+1 signed 14-bit bend, **not** pitch units |
| Out | `mod` / POLY SIGNAL | CC1 normalized 0..1 |
| Out | `notes` / EVENT | NOTE_ON/OFF/CHOKE with offset, assigned voice channel, stable note_id/key/velocity/source_id; emitted to downstream modules in same block |

| Parameter ID | Type / range | Proposed default / exact semantics |
|---|---|---|
| `voice_count` | STEPPED integer 1..16 | 4; fixed configured cable channel count unless deliberately resized at block boundary |
| `allocation_mode` | STEPPED enum `rotate`, `reuse`, `reset`, `mono` | `rotate`; shared `native/voice/` allocator, not duplicate custom allocation |
| `steal_policy` | STEPPED enum `oldest`, `quietest`, `lowest_priority` | `oldest`; only select algorithms actually supported by shared allocator |
| `mono_priority` | STEPPED enum `last`, `low`, `high` | `last`; only used when `mono` |
| `legato` | bool | false; when true and another key still held in mono, preserve gate and emit a note transition/retrigger event per policy below |
| `retrigger` | STEPPED enum `off`, `on_new_note` | `on_new_note`: new note has distinct NOTE_ON regardless of continuously high gate; never invent a sub-1ms digital trigger |
| `sustain` | STEPPED enum `hold`, `ignore` | `hold`; CC64 ≥64 defers sounding-note release until pedal off |
| `pitch_bend_range_semitones` | integer 0..48 | 2 semitones; channel wheel converts to pitch when `apply_bend` is true |
| `apply_bend` | bool | true; raw `bend` always separate; do **not** add the same bend again downstream |
| `channel_filter` | STEPPED 0=Omni, 1..16 | 0; avoids accidental cross-controller channel assignment |

**Proposed conversion math (MIDI1):**
`base_pitch=(key-60)/12`; `signed14=value14-8192` with `value14` in 0..16383. `bend_normalized = signed14/(signed14<0 ? 8192 : 8191)` (center=0; both end stops reach ±1). `pitch = base_pitch + (apply_bend ? bend_normalized*pitch_bend_range_semitones/12 : 0)`. Release a voice by **(endpoint/source, MIDI channel, key, activation identity/note_id)**, not by pitch alone. A stolen activation's later NOTE_OFF must not silence its replacement. MIDI1 Note On velocity 0 behaves as Note Off; MIDI2 must be dispatched according to its protocol.

**Voice lifecycle / ambiguity to settle:** Rotate/Re-use/Reset follow shared allocator semantics; voice count limits **cable channels**, not "up to 16 independent copies of the module." A new note at capacity emits NOTE_CHOKE for stolen note (when applicable) **before** replacement NOTE_ON at the same sample offset; guarantees no untracked gate. On sustain press keep held notes sounding after physical note-off, then release deferred notes at pedal release. In mono legato, subsequent note can change pitch at the same frame while gate remains 1; the `notes` event provides explicit retrigger intent, since gate-alone receivers cannot infer a fresh attack. On all-notes-off, panic, source disconnect or patch replacement release/choke all active voices **even with CC64 held**. Sustain state is per source/channel. Repeated Note Ons for the same key need a deterministic stack/counting rule (recommend FIFO releases) and explicit tests before implementation; don't flatten by pitch key.

**MPE:** recognize that D2 already includes MPE mode and [the communication plan](../specs/COMMUNICATION_MODULES_PLAN.md) describes zones/per-note pitch, pressure and CC74. **Beta 0.8's acceptance should not be blocked by a new MPE implementation** unless Claude explicitly pulls it in; preserve events/source identity and document future hook. Similarly RPN0 auto-adjustment of bend range is an **open question**, not silently implied by a manually configured parameter.

**Implementable allocator tests:**

| Case | Frame-offset MIDI input and config | Exact expected observable outcome |
|---|---|---|
| CV-01 basic pitch | Note On(60,127), (69,64), (72,1) with 3 voices | `pitch` = 0, 0.75, 1 on allocated channels; gates=1; velocities=1, 64/127, 1/127; NOTE_ON events with unique identities |
| CV-02 octave / negative | Note On(48), then (84) | pitch=-1 and +2 respectively, independent of sample rate |
| CV-03 wheel | note60; wheel 8192/0/16383, range 2, apply=true | pitch 0, -1/6, +1/6; raw bend 0,-1,+1; toggle apply=false ⇒ pitch 0 while raw bend changes |
| CV-04 stealing | voices=2 Rotate/oldest; NoteOn 60 @0, 64 @5, 67 @10; NoteOff 60 @20 | At @10 choke 60 then note on 67 (stable tie order); @20 old off cannot close 67; no stuck or duplicate gate |
| CV-05 sustain | note60 @0, CC64=127 @3, NoteOff60 @5, CC64=0 @12 | gate held through frame11, NOTE_OFF and gate=0 at frame12; another endpoint/channel untouched |
| CV-06 velocity zero | MIDI1 NoteOn60 velocity0 after note on; MIDI2 NoteOn60 velocity0 | MIDI1 releases; MIDI2 does **not** automatically release; appropriate typed policy/validation |
| CV-07 legato | mono, legato on: note60 @0, note64 @8, off64 @16 while 60 held | gate remains 1; pitch 0 → 1/3 → 0; note transitions retain sample offsets; no 0-length gate glitch |
| CV-08 pressure/mod | ch2 note, then channel pressure 64, poly pressure key-specific 100, CC1=127 | matching channel's aftertouch=64/127, note pressure=100/127, mod=1; nonmatching voice unaffected |
| CV-09 allocator policies | fixed note sequence 60,62,64, release62,66 with each Rotate/Reuse/Reset and mono priorities | Compare assigned channels, steals, notes and gate to **shared allocator's** reference semantics, not a duplicated test-only algorithm |
| CV-10 teardown and state | synth holds a note while pedal down, then endpoint disconnect / panic; serialize config and restore | all gates immediately 0 and release events emitted; restore modes/range/voice count without reviving stale sounding notes |
| CV-11 audio boundaries | 1,17,128,max frames; 1,4,16 channels; notes at first/last frame and ties | offsets correct, all output samples finite; no allocation/reconfigure in `process`, constant flag correct |
| CV-12 stress | 10k event/s synthetic input with > event capacity | bounded work, dropped-event counters, NOTE_OFF/CHOKE preserved, no hanging notes |

## 3. Quantizer — deterministic scale-to-pitch converter

**Reference:** Doepfer A-156 supports nearest-note quantization with chromatic/scale masks, trigger-sampled mode and a trigger output; Mutable Marbles offers clock-synchronous quantization and selectable scales. For Beta the implementation is **a simple signal processor**, not a replacement sequencer or random generator.

**Module ID:** `essentials.quantizer` (proposed), display **Quantizer**.

| Direction | ID / type | Meaning |
|---|---|---|
| In | `pitch` / POLY SIGNAL, hint pitch | Plain octave pitch (0=C4), supports 1–16 independently quantized voices |
| In | `trigger` / SIGNAL, hint trigger, optional | In sampled mode, rising edge captures input pitch per channel; unpatched = continuous mode by explicit parameter default |
| In | `transpose` / POLY SIGNAL, hint pitch, optional | Offset in octave units, applied **before** scale projection, optional; constant 0 if unpatched |
| Out | `pitch` / POLY SIGNAL, hint pitch | Nearest allowed scale pitch; finite and continuous between resets except quantized jumps |
| Out | `changed` / POLY SIGNAL, hint trigger | ≥1ms 1.0 pulse when quantized pitch changes or a triggered capture succeeds (choose one semantics below); otherwise 0 |

| Parameter ID | Type / range | Default / recommended rule |
|---|---|---|
| `scale` | STEPPED enum `chromatic`, `major`, `natural_minor`, `major_pentatonic`, `minor_pentatonic`, `custom_12` | `chromatic`; named intervals relative to root |
| `root` | STEPPED integer 0..11 | 0=C, 1=C#, … 11=B; root specifies pitch class in C4-based semitone lattice |
| `scale_mask` | STEPPED 12-bit positive integer 1..4095 | 4095; bits encode chromatic pitch classes 0..11 **relative to root**; used only for `custom_12`; mask=0 rejected on prepare/config update |
| `mode` | STEPPED enum `continuous`, `sample_trigger` | `continuous`; sampled mode holds last quantized value until valid rising input trigger |
| `tie_break` | STEPPED enum `down`, `up` | `down` at mathematically equal distances; applies across octave bounds |
| `hysteresis_cents` | float 0..25 | 0; in continuous mode a new candidate wins only past old→new decision boundary by this many **cents**; per-channel remembered output |

**Definition:** for chromatic/default masks, candidates are `(12*k + root + degree)/12` for integers `k` and selected `degree` in `0..11`; pick the nearest absolute pitch across **all** adjacent octaves, tie down by default. Apply optional transpose **before** quantizing. Major degrees: {0,2,4,5,7,9,11}; natural minor {0,2,3,5,7,8,10}; major pentatonic {0,2,4,7,9}; minor pentatonic {0,3,5,7,10}. This fixes negative pitch modulo, nonzero root and exact half-step ties. `root` plus `custom_12` mask is absolute relative to root rather than a copy of a preset.

**Sample/edge behaviour:** change detection must evaluate each sample (or signal CONSTANT fast path), not once per host block, because D1 supports audio-rate pitch. Hysteresis uses *previous output* to prevent limit cycling at a semitone boundary; reset and scale/root changes invalidate the held candidate, then compute a deterministic new pitch. In sampled mode the output holds until each channel's Schmitt-detected rising trigger (D4 threshold >0.5 on, <0.25 off); a 1-channel trigger broadcasts to poly pitch through SDK. For `changed` propose a pulse **only when the output value changes**, ≥1ms at the sample rate (48 samples @48k, 44.1→ceil 45 @44.1k), even across blocks; repeated triggers on same pitch don't duplicate it. Claude should approve this versus the A-156's pulse on *each* quantization event. Output remains stable and finite if input stays constant.

**Scala `.scl`:** documented format supports non-octave periods, cents/ratios and optional separate `.kbm` note mappings. **Recommendation: do not ship a partial parser or label 12-note scale masks “Scala support” in Beta 0.8.** Track as follow-up: load and validate `.scl` **quiescently**, cap entries, store normalized pitches/hash in patch state and handle `.kbm` separately; source file I/O never occurs on audio thread. If Claude requires imported tunings for this beta, scope a separate explicit issue and add tests for nonoctave periods/missing files/state portability.

**Implementable golden tests** (values in D4 pitch units; allow only documented float epsilon e.g. 1e-6):

| Case | Input & config | Expected output |
|---|---|---|
| Q-01 chromatic | input 0, 1/12, 1, -1 | output same exactly to float epsilon; preserve 4 voices |
| Q-02 major | root C, input MIDI63-equivalent `3/12`, tie=down/up | nearest D `2/12` (down) or E `4/12` (up); output stable |
| Q-03 negative | root C major, input `-1/12` (B3) | -1/12, not +11/12; nearest across octave |
| Q-04 changed root | C-major input `1/12` => C; change root to D-major | choose among D-major classes using absolute proximity, deterministically re-evaluate without losing cable |
| Q-05 custom | root C mask only C and G bits (1+128), input `6/12` | nearest G `7/12`; mask0 is invalid |
| Q-06 sample hold | sampled mode; input0 @frame0; trigger rising @3; input 0.75 @8 with no new trigger | capture 0 @3 then hold 0; capture 0.75 only on subsequent rising trigger |
| Q-07 hysteresis | chromatic, previous pitch 0, input oscillates around +0.5 semitone | at 0 cents hysteresis output follows tie/threshold; with 10-cent hysteresis does **not** chatter until crossing extended boundary |
| Q-08 edges | trigger pulses >0.5/<0.25, at block end then following block; channel count 1→4→16 | one capture per true rising edge, broadcast properly, state per voice, no allocation |
| Q-09 1ms change | held 0→1/12 at last sample of 17-frame block @48k | `changed` 1 from that sample for 48 samples **across subsequent blocks**, then 0 |
| Q-10 reset/save | use nonzero root, mask, held sample and hysteresis; save/load and reset | parameters restored, no random choices; state format documented, finite and reproducible output |

## 4. Basic Sequencer — one 16-step, monophonic musical lane

**Reference:** VCV SEQ-3 demonstrates a small explicit clock/run/reset sequencer; Arturia BeatStep Pro confirms per-step pitch/velocity/gate length, tie, swing and external synchronization. **Beta design deliberately remains one lane, 16 steps max**, rather than a full multitrack/polyrhythmic arranger.

**Module ID:** `essentials.basic_sequencer` (proposed), display **Basic Sequencer**.

| Direction | Stable ID / kind | Meaning |
|---|---|---|
| In | `clock` / SIGNAL, trigger | Optional external rising-edge clock; **takes precedence** over internal tempo while patched |
| In | `reset` / SIGNAL, trigger | Schmitt rising edge; reset index/phase, same-frame reset wins clock |
| In | `run` / SIGNAL, gate, optional | Optional explicit run gate; with unpatched input, saved `running` parameter controls run |
| Out | `pitch` / MONO SIGNAL, pitch | Current step's note as `(midi_note-60)/12`, not MIDI note number-as-CV |
| Out | `gate` / MONO SIGNAL, gate | 0/1; held for gate length or across tied steps |
| Out | `trigger` / MONO SIGNAL, trigger | ≥1ms on *new sounding step*; persists across process boundaries |
| Out | `notes` / EVENT | NOTE_ON/OFF with stable note_id/key/velocity and exact offsets; mono voice channel 0 |

| Parameter / state ID | Type / range | Proposed default / meaning |
|---|---|---|
| `length` | STEPPED 1..16 | 16 active steps, fixed saved array of 16 |
| `tempo_bpm` | 20..300 BPM (log) | 120; internal fallback while host transport absent |
| `division` | STEPPED `1_4`, `1_8`, `1_16`, `1_32` | `1_16`; steps per quarter = 1,2,4,8 |
| `swing_pct` | 50..75 (linear) | 50 = straight; at 60% first of a two-step pair lasts 60% of pair, second 40%; *internal tempo only* initially |
| `running` | bool | false after new instance; persisted config controls run when `run` unpatched |
| `reset_on_start` | bool | true; start positions at step 0 and **emits step 0 on first tick**, not an extra silent step |
| `gate_length_pct` | 10..95 | 50; for ON steps gate high for this fraction of current step's actual (swung) duration |
| `transpose_semitones` | integer -24..24 | 0; transpose before D4 pitch conversion |
| `steps[i].note` | per step 0..127 MIDI note | default 60 for each (stored data, not 16 ABI parameters) |
| `steps[i].velocity` | per step 0..127 | 100; for note-on convert to 0..1 |
| `steps[i].state` | enum `off`, `on`, `tie` | `off` default; ON starts note, OFF releases current, TIE continues previous note if active; otherwise silent |
| `steps[i].gate_length` | optional per-step 10..95 / override | **defer in Beta**; start with global gate length to limit UI/state/test surface |

**Clock and event rules:** 120 BPM, 1/16 division means **6000 frames per straight step @48 kHz**. A 60% swing pair lasts 12000 frames with **7200 first / 4800 second**; do not change average tempo. Use a frame/beat accumulator that schedules exact *sample offsets* even when the host block is 1 or 17 samples or when 44.1 kHz yields fractional samples/step; never round each step independently and accumulate drift. When `clock` is patched, advance **once per rising edge** and suppress internal ticks; external-clock swing must be disabled/bypassed unless a later implementation explicitly schedules offset/delay with bounded lookahead. If D9 transport is available, a future "follow transport" option can use beat position; **do not make Beta sequencing silently depend on M6's later transport rollout**. Tempo changes preserve musical phase rather than jumping or duplicating notes.

**Step/pulse semantics:** on first start/reset tick, play index 0; next tick index 1; wrap at `length-1`. If a new ON step begins while the prior gate is active, emit prior NOTE_OFF before new NOTE_ON at the same offset, with distinct note_ids. If OFF, emit NOTE_OFF and gate 0. If TIE with an active note, do not retrigger or change pitch/velocity, and suppress intermediate gate-off; otherwise silent. **A step whose following step is TIE must extend its gate through that tie ahead of time**, overriding the ordinary gate-length expiration; when a tie chain ends at OFF or ON, release/retrigger at that boundary. This requires a bounded one-step lookahead, including wraparound, and avoids an already-expired note being mistakenly restarted. Note-off also happens at gate-length expiry, on stop, reset, patch replacement and panic. **Reset plus clock at identical frame:** reset wins, ignore that clock; next valid clock enters step0. No output NOTE_ON occurs merely because reset was pressed while stopped. Gate, trigger and note event scheduling must be sample accurate at boundaries; trigger is **1ms minimum** and may overlap adjacent very fast ticks (then duration/retrigger policy needs an explicit deterministic test). Run gate optional; Schmitt guards bouncing inputs.

**Persistence:** D12 versioned patch data saves 16 records, active length, tempo/division/swing, step state/velocity and run/reset settings. Save at quiescence; validate out-of-range/clamped data and preserve unknown future versions at host level. Playback cursors and active note_id are runtime state, not blindly resurrected into a sounding note after load. Pattern copies/reorder, editing UI, randomization, ratchets, probability, multitrack and arranger are later work.

**Implementable golden tests:**

| Case | Input/config | Expected ordered output |
|---|---|---|
| SEQ-01 simple cycle | length=2, step0 ON C4 vel127, step1 ON E4 vel64; 120 BPM, 1/16, 50% gate, 48k; start at frame0 | NOTE_ON60/pitch0/gate1 @0; NOTE_OFF60 and gate0 @3000; NOTE_ON64/pitch1/3/gate1 @6000; NOTE_OFF64 @9000; note60 again @12000 |
| SEQ-02 swing | as above, `swing_pct`=60 | step0 at frame0, step1 at frame7200, next step0 at frame12000; 2-step pair remains 12000 frames; gate-offs at halfway through each *actual* interval |
| SEQ-03 varying block | same as SEQ-01 with blocks 1,17,128 and 44.1/48/96 kHz | exact same musical onset times to ≤1 sample, no lost/doubled note or drift across 1 minute |
| SEQ-04 tie | step0 ON C4, step1 TIE, step2 OFF; arbitrary clocks @0, @100, @200 | one NOTE_ON @0, **no** retrigger or gate drop @100 (tie), NOTE_OFF/gate0 @200, even if ordinary gate_length would have expired |
| SEQ-05 reset/clock | same-frame reset+clock @300; clock @400 | at 300 release active note, set index0, ignore clock; @400 emit step0 only; no accidental step1 |
| SEQ-06 external priority | transport absent; external clock present and internal tempo ticking | notes advance exactly on external edges, no internal double ticks; external swing not falsely advertised |
| SEQ-07 stop/panic | held gate or TIE, then stop/reset/panic | note off/choke emitted exactly once and gate 0 even across patch reload; no stuck note |
| SEQ-08 edit/wrap | 3 steps, edit length 3→1 while running; step 2 off; write transpose=+12 | next valid index wraps to 0; pitch changes one octave; no OOB reads or old note hanging |
| SEQ-09 save/restore | saved 16-step pattern with velocity/tie/swing/tempo/length; reload | exact validated step records and parameters; playback begins deterministically, does not resurrect note |
| SEQ-10 finite/RT | all rates and 1/4/16 input channels, extreme control values, block 1/17/max | truthful MONO output, bounded note events, no allocation/locks/I/O/exceptions in `process` |

## 5. End-to-end Beta acceptance and Codex execution order

**Recommended implementation order (after M2/M3 gates, not as part of #60):** (1) minimum fakeable MIDI endpoint/ingress subset and MIDI In; (2) connect existing shared allocator for MIDI→CV, note safety and events; (3) Quantizer pure DSP/test harness; (4) Basic Sequencer, clock/run/reset, state and integration. Quantizer's signal-only work can run in parallel with the MIDI backend; sequencer notes/event tests use the same voice/event semantics. No parallel modifications to protected ABI specs. Use a single owning agent per implementation issue, and delegate only separable **tests or reference research**, not competing changes to the same files.

**Four end-to-end offline fixtures Codex can implement directly:**

1. **Keyboard→voice:** fake MIDI endpoint A sends ch1 NoteOn(60,127) @frame7 and NoteOff @frame77 across irregular blocks → MIDI In `midi` offsets maintained → MIDI→CV `pitch`=0 and `gate`=1 until off → ABI2 VCO pitch/ADSR gate voice channel0, no stranded gate.
2. **Voice steal/sustain/disconnect:** voice_count=2, CC64 on, 3 simultaneous notes from A, then A unplugged while B remains active → allocator emits balanced CHOKE/OFF before replacement and on disconnect, B unaffected; no residual active notes even if sustain on.
3. **Sequenced melody:** step0 C4 step1 D4 step2 rest step3 tie (after rest => silent), 120 BPM, 48k, clock from internal source, Quantizer in C-major with C4 root → exact note/event/gate schedule and output pitch 0, 2/12, then gate0; no double-trigger on pause/reset.
4. **Adversarial timing:** host runs 1/17/128-frame calls at 44.1,48,96 kHz with bursts at frame0 and end, transport missing, midi note-off overflow and quantizer near-threshold input → same timestamp-ordered logical events irrespective of block partitions; no allocations, finite outputs, no stuck notes.

**Non-optional validation for future implementation:** C-ABI SDK callbacks, separate module unit tests and a graph-level deterministic offline test; config and state round trip, invalid values, max16 channels, CONSTANT fast path, 1/17/max blocks, 44.1/48/96k, simultaneous event ordering, overflow note-offs, phantom/disconnected MIDI sources, sample-rate changes, no audio/MIDI device activation in automated tests, CI `linux-arm64` and `windows-x64` when review-ready. **Manual Beta release gate** remains the authorized 60-minute external MIDI keyboard session, logged with dropout/stuck-note/crash observations; no such session performed for this study.

## 6. Questions for Claude — decisions required before coding

| Priority | Question | Recommendation / acceptance consequence |
|---|---|---|
| P0 | Confirm module IDs (`midi.in`, `midi.to_cv`, `essentials.quantizer`, `essentials.basic_sequencer`), port IDs and whether the latter two qualify as Essentials? | Freeze once chosen; never rename after patch serialization. |
| P0 | What **minimum subset** of C1 endpoint service ships *before* full M4—identity persistence, single-backend, counters, fake service, disconnect release—and who owns it? | Keep services out of C++17 DSP, do not claim full Learn/CC Map/MIDI Out in Beta. |
| P0 | What exact shared allocator semantics govern Rotate/Reuse/Reset, duplicate MIDI note keys, stealing, channel count and legato/retrigger? | Point Codex to the `native/voice/` implementation as oracle; choose repeated-note FIFO/count policy, preserve note_id. |
| P0 | Is MPE explicitly postponed? Does MIDI1 RPN0 override the manual pitch-bend range or get ignored with notice? | Beta: no MPE design; configurable ±2st default. Preserve groups/source/protocol for later. |
| P0 | Does Beta need fully serialized sequencer patterns and a minimal editor before the 60-minute live test? | Yes, persist 16 steps and versioned state; do not silently substitute an on-the-fly uneditable generator. |
| P1 | Quantizer `changed` pulse on output *change* or on every trigger-sampled evaluation? How much hysteresis and which midpoint tie? | Prefer changed-only, tie-down, 0-cent default with optional 0–25 cents. |
| P1 | Scala `.scl`/`.kbm` in Beta or later separate issue? | Later. Support named 12-TET/masks now, import both Scala format and keyboard mapping only when validation and state portability are specified. |
| P1 | Sequencer reset/start, note-off/retrigger, tie semantics, external clock priority, swing under external clock and D9 transport dependency? | Adopt explicit sample-accurate rules above; disable external swing, internal tempo fallback, reset wins same-frame clock. |
| P1 | Should external MIDI channels from multiple endpoints share a MIDI→CV allocator or remain isolated? | Default identity distinguishes sources; explicit Omni mixing is user choice, with voice cap and predictable steals. |
| P1 | Which parts of Beta need physical Pi5/MSVC proof vs CI? | Never label docs-only research as performance proof; CI/fake tests first, user-authorized live keyboard gate last. |

**Study-only validation:** inspected issue #60, AGENTS, Beta 0.8 PLAN, MIDI sections of communication plan, ABI2 D2/D4/D8, ROADMAP and the cited official reference pages. No source code, schema, protected file, tests, audio/MIDI device or app was changed or executed. This is a **proposed brief**, not four implementation tasks already completed.
