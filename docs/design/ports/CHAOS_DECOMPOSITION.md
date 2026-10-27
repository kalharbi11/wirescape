<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CHAOS decomposition study — Track X-2, issue #52

**Status:** architecture study only; no DSP changes, code, performance proof or adoption decision. CHAOS source is read-only. Follow `AGENTS.md`, `docs/tasks/X_THIRD_PARTY_PORTS.md` and `docs/specs/ABI2_PLAN.md`. Track X does not block M2; nothing merges before M2-R and planner review. **Scope decision (2026-10-27): do not add tape.** `TAPE` is inventoried solely to account for the legacy prototype, but is **excluded** from proposed ports, build order, spike and CHAOS-lite.

## 0. Upstream identity, licence and pinned source

| Repo / candidate material | Inspected commit (full SHA) | Licence file and target conditions |
|---|---|---|
| [Surge XT](https://github.com/surge-synthesizer/surge) — `src/common/dsp/oscillators/ClassicOscillator.{h,cpp}`, `src/common/dsp/effects/` | `dab78c6170732b3883216e569a84bf084ae0ea97` | [`LICENSE`](https://github.com/surge-synthesizer/surge/blob/dab78c6170732b3883216e569a84bf084ae0ea97/LICENSE), GPL-3.0. Whole-engine dependencies are **not** assumed to be isolated reusable components. |
| [sst-basic-blocks](https://github.com/surge-synthesizer/sst-basic-blocks) — `dsp/` utilities, oscillator/lag/DC blocker | `38cf04ae6a16994cc6c82635ce7c206f2a0ca31d` | [`LICENSE.md`](https://github.com/surge-synthesizer/sst-basic-blocks/blob/38cf04ae6a16994cc6c82635ce7c206f2a0ca31d/LICENSE.md), primarily GPL-3.0; README identifies specific separately MIT-licensed headers, to verify per chosen file. |
| [sst-filters](https://github.com/surge-synthesizer/sst-filters) — `include/sst/filters/` comb, filters | `87a171bcba52c014850848a29174a54f0adc6606` | [`LICENSE.md`](https://github.com/surge-synthesizer/sst-filters/blob/87a171bcba52c014850848a29174a54f0adc6606/LICENSE.md), GPL-3.0. README: raw SSE intrinsics; non-x86 needs SIMDe or equivalent. |
| [sst-waveshapers](https://github.com/surge-synthesizer/sst-waveshapers) — `include/sst/waveshapers/` | `dd12f31a5a9016c9895e52d1a00eee0e1eebe6ce` | [`LICENSE.md`](https://github.com/surge-synthesizer/sst-waveshapers/blob/dd12f31a5a9016c9895e52d1a00eee0e1eebe6ce/LICENSE.md), GPL-3.0. README: raw SSE intrinsics; SIMDe for non-x86. |
| [sst-effects](https://github.com/surge-synthesizer/sst-effects) — `include/sst/effects/Reverb2.h`, `Delay.h`, `Flanger.h` | `987b39dcb2e2e61e6225c3bc390fd00da824d226` | [`LICENSE.md`](https://github.com/surge-synthesizer/sst-effects/blob/987b39dcb2e2e61e6225c3bc390fd00da824d226/LICENSE.md), GPL-3.0. README: hand-coded SSE2, SIMDe/equivalent for non-x86. |

The pinned SHAs identify inspected repository heads on 2026-10-27; **transitive compatibility and upstream copyright for each selected source are to verify**. Never vendor upstream trees or put their sources under `native/modules2/`. Later builds must use CMake `FetchContent` pinned to full *compatible* SHAs; retain each original copyright notice and give credit in `DESIGN.md`. All proposed derived modules use GPL-3.0 and stable `ports.*` IDs (not vendor branding or `essentials.*`). No upstream DSP rewrite for the first port; compare output against the *same* upstream sources. All proposed targets require Windows x64 and Linux ARM64, plus the portability probe; Cortex-M7/Daisy budget is **not yet validated**. Pure x86 SSE cannot be shipped unchanged on ARM64; SIMDe support is a build hypothesis, not benchmark evidence.

## 1. Inventory: blocks, their signal boundaries, source, contract

Source paths in these tables are relative to `legacy/modules/chaos/` unless explicitly prefixed with `docs/`. Actual prototype works at 48 kHz with 512-frame callbacks, while Surge internally processes 32-frame blocks (`engine.py:27-28,35-36,383-411`). Its A/B are **two entire independent Surge synth instances**, not two oscillator objects. The prototype's Surge global FX is also installed in both (`engine.py:345-356`). This is not evidence that a complete Surge voice fits a primitive Wirescape module.

| Block (one per row) | Inputs → outputs | Source / proposal | Evidence; ABI2 decisions |
|---|---|---|---|
| Surge generator A | note on/off, pitch bend, three oscillators, filters, envelopes, raw/native modulation → stereo A | Surge XT; **decompose later**, not full-engine port | `README.md:9`, `engine.py:35-45,403-407`, `voices.py:10-31`; D2,D7,D8 |
| Surge generator B | same controls plus selected recipe → independent stereo B | Surge XT, six B presets; not independent algorithms | `engine.py:46-63,408-411`, `voices.py:33-44`; D2,D8,D12 |
| Oscillator/sub/FM/waveshaper | pitch, type, P1-P7, sub, FM ratio, shape/drive → raw oscillator content | Surge oscillator DSP; possible sst-waveshapers **only for isolated shaper** | `macros.py:76-80,99-110`, `voices.py:10-17`; D2,D7,D15 |
| Filter 1/2 and native envelope paths | filter/EG/velocity inputs → filtered, enveloped output | Existing **Essential VCF, Ladder, ADSR** for lite; sst-filters for quality variants | `voices.py:18-24`, `macros.py:81-85,115-116`; D2,D7 |
| Two native LFOs / five native modulation routes **per generator** | LFO1/2/velocity → filter cutoff, pitch and oscillator P1 | Existing **Essential LFO** + D7 modulation for reconstructed graph, or Surge native DSP for equivalence | `ARCHITECTURE.md:21-25`, `voices.py:23-31`, `engine.py:138-152`; D7,D12 |
| Key/cluster/24-EDO note fanout | held keys, CLUSTER, QUARTER → expanded notes into A and B | **Planned Channel Expander**; shared voice allocator and tuning resource, not stacked mono CV | `engine.py:237-267,269-331`; D2,D8,D12 |
| Pitch instability and wow | OFFSET,FALL,JITTER,DRIFT and optional legacy TAPE amount → pitch bends | **New** seeded pitch drift/fall sources; Essential Scale/Offset/LFO | `engine.py:358-381`; D7,D15. **Exclude tape behavior now** |
| A/B stereo mixer and pan | A/B LEVEL,PAN,MUTE → two stereo generator buses | **Essential VCA/Mixer** + planned stereo panner | `engine.py:431-446`; D3,D7 |
| Parallel bus mode | A+B → stereo summed bus | **Essential Mixer** (stereo pair composition) | `chaos_dsp.py:24-30`; D3 |
| Coupled ring/AM bus mode | A and nonlinear B, depth → blended stereo bus | **New** AM/ring router; not phase FM inside oscillator | `chaos_dsp.py:31-35`, `ARCHITECTURE.md:13-17`; D3,D7 |
| Coupled delay-bend bus mode | A stereo signal, B audio driver, ROUTE_DEPTH → modulated ~3.5±2.5 ms delay | **New** bounded modulated-delay module; `sst-effects/Delay.h` is **not** proved equivalent | `chaos_dsp.py:36-55`; D3,D11 |
| External stereo injection | input L/R × EXT_MIX → summed pre-body bus | **Essential Mixer/VCA** | `PORTS.md:5-16`, `engine.py:447-453`; D3,D7 |
| Three-comb resonant body | stereo input, BODY_TONE/MIX/FEED → saturated damped body | **New** exact CHAOS topology or sst-filters comb **as alternative DSP**, not interchangeable | `chaos_dsp.py:58-102`; D3,D11,D15 |
| Roughness AM | stereo audio, ROUGH_RATE/DEPTH → amplitude chopped output | **Essential LFO + VCA** or new single-purpose AM module | `hostdsp.py:11-29`, `engine.py:416-418`; D1,D7 |
| Tape processor | legacy TAPE → Surge global Tape FX settings plus pitch wobble | **DEFERRED by request; no tape module or implementation** | `macros.py:30,66,90-91,117-122`, `engine.py:375-380`; D3,D11 |
| Space / reverb | SPACE,SPACE_MIX → Surge Reverb2 effects (in A and B) | **sst-effects/Reverb2.h** candidate for a *later* stereo port | `macros.py:31-32,66,90-92,123-125`; D3,D11 |
| Five-second varispeed looper | stereo in; speed -2..+2, feedback, mix, freeze/clear/load → stereo loop out | **New** looper; not a Surge/sst equivalence claim; **later** | `hostdsp.py:36-77`, `engine.py:318-328,419-422`; D3,D11,D12 |
| Master output conditioning | gain dB, stereo in → DC block, soft knee, -1 dBFS hard ceiling | **Essential VCA** + new DC/safety stage; basic-blocks DCBlocker as candidate, *not parity by default* | `hostdsp.py:80-106`, `engine.py:423-429`; D3,D15 |
| Self-modulating LFO | rate/depth/feedback/shape, RNG → -1..+1 modulation | **New** seeded LFO or Essential LFO with delayed CV feedback | `modlab.py:8-54`, `engine.py:333-343`; D7,D10,D15 |
| S&H and smooth random | LFO clock, seeded random → bipolar stepped/smoothed CV | **New** seeded CV sources | `modlab.py:99-145`; D7,D15 |
| Previous output envelope follower | prior output block RMS → next-block bipolar modulation | **New** bounded envelope follower; D10 feedback delay explicit | `modlab.py:105-125`, `engine.py:425-426`; D7,D10 |
| Four-slot macro matrix | 4×{source,destination,amount}, base controls → additive clamped effective controls | **D7 host modulation cables**; optional saved routing UI D12/D16; *not* new matrix DSP | `modlab.py:99-145`, `PORTS.md:18-25`; D7,D12,D16 |
| Macro surface, front, raw A/B | macro knob + 766-entry source snapshot + raw overrides → base parameters | **D16 subpatch/macros** + D6 stable params, D12 state. Raw Surge catalog isn't an ABI module itself | `macros.py:10-60,95-126`, `engine.py:120-170,345-356`; D6,D12,D16 |
| Randomization / undo / presets / scenes | parameter groups, intensity, seed and stored scene → parameter/state changes | **Host snapshot/state**, seeded random and quiescent UI; not audio-thread allocation | `modlab.py:57-97`, `README.md:110-112`, `ARCHITECTURE.md:31-37`; D12,D15,D16 |
| Analysis, stems, capture | A/B/CHAOS pre-master and final output → meter/scope/recording | **Planned Scope/Spectrum/Monitor** plus bounded host capture; not DSP ports | `PORTS.md:12-16`, `engine.py:87-96,454-485`, `README.md:124-125`; D3,D13 |
| Optional future vocal coupling | stereo external generator → EXT_MIX before body | **Mixer/VCA** only; *no* GUTTR dependence or vocal processing port | `README.md:15-20`, `ARCHITECTURE.md:27-29`; D3,D5 |

**Catalog check:** `params.json` is an *introspected Surge parameter catalog* (766 entries across `cg_GLOBAL` 68, `cg_OSC` 72, `cg_MIX` 50, `cg_FILTER` 24, `cg_ENV` 32, `cg_LFO` 312 and `cg_FX` 208; `params.json:1-9193`); it is not a list of 766 planned Wirescape knobs or an API stability guarantee. See `voices.py:10-31` for the curated raw oscillator/filter/EG/LFO/native modulation subsets and `engine.py:99-170` for scene/name lookup. Parameter names and numeric IDs must be resolved against the **pinned** DSP sources; mismatches are **to verify**. Legacy `PORTS.md:21` calls the schema “48 parameters”; the literal `macros.SPEC` contains **49 keys** (37 controls plus four three-field slots); do not silently drop one.

### Macro inventory (one row per `macros.SPEC` key)

Inputs are base control values (plus modulators where noted); outputs are parameter/CV targets or module state. All numeric ranges and defaults are directly from `macros.py:10-60`. `MOD1..4` routing sources are explicitly defined in `modlab.py:99-104`. These are legacy compatibility names, **not** promised final public parameter IDs.

| Macro (min, max, default) | Input → effect/output; owning module class | Source |
|---|---|---|
| `BREAK` (`0.0, 1.0, 0.0`) | nonlinear intensity; base → drive/FM/ring/sub/feedback controls (Surge macro; later D16) | `macros.py:13` |
| `DRIVE_TYPE` (`0, 7, 0`) | waveshaper selection → Surge shape enum; stepped (sst-waveshapers candidate) | `macros.py:14` |
| `ROUGH_RATE` (`5.0, 150.0, 70.0`) | Hz → roughness-AM oscillator frequency (new/Essential LFO) | `macros.py:15` |
| `ROUGH_DEPTH` (`0.0, 1.0, 0.0`) | depth → stereo roughness AM gain (Essential VCA + LFO) | `macros.py:16` |
| `JITTER` (`0.0, 1.0, 0.0`) | amount → randomized pitch/jump offsets (new seeded CV) | `macros.py:17` |
| `SUB` (`0.0, 1.0, 0.0`) | level → oscillator subharmonic blend (Surge oscillator) | `macros.py:18` |
| `FM_RATIO` (`0.0, 1.0, 0.0`) | amount → oscillator-2 pitch/FM ratio (Surge oscillator) | `macros.py:19` |
| `DETUNE` (`0.0, 60.0, 0.0`) | cents → unison pitch spread (Channel Expander planned) | `macros.py:21` |
| `VOICES` (`1, 9, 2`) | count → native unison voices (Channel Expander planned) | `macros.py:22` |
| `DRIFT` (`0.0, 1.0, 0.0`) | amount → oscillator pitch drift (new seeded CV or Surge native) | `macros.py:23` |
| `OFFSET` (`-50.0, 50.0, 0.0`) | cents → keyboard pitch offset (Essential Scale/Offset) | `macros.py:24` |
| `CLUSTER` (`1, 12, 1`) | count → adjacent note events (planned Channel Expander) | `macros.py:25` |
| `QUARTER` (`0, 1, 0`) | on/off → optional 24-EDO tuning (planned quantizer/tuning; resource load quiescent) | `macros.py:26` |
| `FALL` (`0.0, 1.0, 0.0`) | amount → held-note falling-pitch CV (new bounded slew) | `macros.py:27` |
| `CUTOFF` (`0.0, 1.0, 0.75`) | normalized cutoff → filter-1 frequency (Essential VCF/Ladder) | `macros.py:28` |
| `TAPE` (`0.0, 1.0, 0.0`) | legacy tape amount → native Surge Tape FX and wow; **DEFERRED: no port or CHAOS-lite use** | `macros.py:30` |
| `SPACE` (`0.0, 1.0, 0.4`) | size/decay → Surge Reverb2 or sst-effects candidate (deferred from lite) | `macros.py:31` |
| `SPACE_MIX` (`0.0, 1.0, 0.0`) | wet amount → reverb return (sst-effects Reverb2 candidate) | `macros.py:32` |
| `LOOP_SPEED` (`-2.0, 2.0, 1.0`) | -2..2 → looper playback step/direction (new looper, later) | `macros.py:34` |
| `LOOP_FEEDBACK` (`0.0, 0.95, 0.0`) | 0..0.95 → stereo overdub feedback (new looper, later) | `macros.py:35` |
| `LOOP_MIX` (`0.0, 1.0, 0.0`) | 0..1 → dry/loop return (new looper, later) | `macros.py:36` |
| `LFO_RATE` (`0.02, 12.0, 0.3`) | Hz → self-LFO/sample-hold rate (Essential LFO or new seeded LFO) | `macros.py:38` |
| `LFO_DEPTH` (`0.0, 1.0, 0.0`) | amount → self-LFO destination gain (D7 modulation) | `macros.py:39` |
| `LFO_SELF` (`0.0, 1.0, 0.0`) | amount → previous LFO output modulates its own rate (new seeded LFO) | `macros.py:40` |
| `MASTER_DB` (`-48.0, 6.0, 0.0`) | dB → output gain, DC block, ceiling (Essential VCA + new safety stage) | `macros.py:42` |
| `A_LEVEL` (`-48.0, 6.0, 0.0`) | dB → A stereo bus gain (Essential VCA) | `macros.py:44` |
| `A_PAN` (`-1.0, 1.0, 0.0`) | -1..1 → A equal-power stereo panning (planned stereo pan) | `macros.py:44` |
| `A_MUTE` (`0, 1, 0`) | toggle → A bus mute (Essential VCA) | `macros.py:44` |
| `B_LEVEL` (`-48.0, 6.0, -9.0`) | dB → B stereo bus gain (Essential VCA) | `macros.py:45` |
| `B_PAN` (`-1.0, 1.0, 0.0`) | -1..1 → B equal-power stereo panning (planned stereo pan) | `macros.py:45` |
| `B_MUTE` (`0, 1, 1`) | toggle → B bus mute (Essential VCA) | `macros.py:45` |
| `ROUTE_MODE` (`0, 2, 0`) | 0 parallel / 1 AM / 2 delay-bend → bus topology selection (new routing primitive; D12) | `macros.py:48` |
| `ROUTE_DEPTH` (`0.0, 1.0, 0.0`) | blend → clean/coupled bus crossfade (new routing primitive) | `macros.py:48` |
| `BODY_MIX` (`0.0, 1.0, 0.0`) | wet amount → resonant-body stereo mix (new three-comb DSP) | `macros.py:49` |
| `BODY_TONE` (`0.0, 1.0, 0.5`) | 0..1 → comb spacing/frequency (new three-comb DSP) | `macros.py:49` |
| `BODY_FEED` (`0.0, 0.85, 0.0`) | 0..0.85 → damped saturating comb feedback (new three-comb DSP) | `macros.py:50` |
| `EXT_MIX` (`0.0, 1.0, 0.0`) | 0..1 → injected stereo input pre-body (Essential Mixer/VCA) | `macros.py:50` |
| `MOD1_SRC` (`0, 4, 0`) | 0..4 → choose off/self-LFO/S&H/smooth/output-envelope source (D7/D12) | `macros.py:52` |
| `MOD1_DST` (`0, 11, 0`) | 0..11 → choose additive destination from 11 macro targets (D7/D12) | `macros.py:52` |
| `MOD1_AMT` (`-1.0, 1.0, 0.0`) | -1..1 → bipolar depth scaled to destination span (D7) | `macros.py:52` |
| `MOD2_SRC` (`0, 4, 0`) | 0..4 → choose off/self-LFO/S&H/smooth/output-envelope source (D7/D12) | `macros.py:53` |
| `MOD2_DST` (`0, 11, 0`) | 0..11 → choose additive destination from 11 macro targets (D7/D12) | `macros.py:53` |
| `MOD2_AMT` (`-1.0, 1.0, 0.0`) | -1..1 → bipolar depth scaled to destination span (D7) | `macros.py:53` |
| `MOD3_SRC` (`0, 4, 0`) | 0..4 → choose off/self-LFO/S&H/smooth/output-envelope source (D7/D12) | `macros.py:54` |
| `MOD3_DST` (`0, 11, 0`) | 0..11 → choose additive destination from 11 macro targets (D7/D12) | `macros.py:54` |
| `MOD3_AMT` (`-1.0, 1.0, 0.0`) | -1..1 → bipolar depth scaled to destination span (D7) | `macros.py:54` |
| `MOD4_SRC` (`0, 4, 0`) | 0..4 → choose off/self-LFO/S&H/smooth/output-envelope source (D7/D12) | `macros.py:55` |
| `MOD4_DST` (`0, 11, 0`) | 0..11 → choose additive destination from 11 macro targets (D7/D12) | `macros.py:55` |
| `MOD4_AMT` (`-1.0, 1.0, 0.0`) | -1..1 → bipolar depth scaled to destination span (D7) | `macros.py:55` |

Other discrete but **not** `SPEC` keys: `LFO_SHAPE` (sine/tri/S&H/smooth), `LFO_TARGET` (off or a macro), `B_TYPE` (warm pad/speech/formant/glass/string/wavetable), clear/freeze/load loop commands, raw A/B overrides and randomize/repeat/undo. These are state/config/events, not omitted audio blocks (`modlab.py:8-9,57-103`; `voices.py:33-44`; `engine.py:279-328`). Legacy UI `docs/history/CHAOS_CONTROLS.md:13-36,61-63` maps the **front** MORPH/CHAOS/FOLD/MOTION/COUPLE and per-generator ADSR controls onto these same internals; those front controls are not extra DSP engines. The old RAW/front/preset ownership precedence (`docs/history/CHAOS_CONTROLS.md:42-52`) should be **replaced**, not duplicated, by D7's non-destructive modulation and saved D12 base state.

## 2. ABI2 contract check and gaps

| Requirement / evidence | ABI2 solution | Open engineering question |
|---|---|---|
| Multivoice A/B/CLUSTER, monophonic and poly cables (`engine.py:237-267`) | D1 planar 1–16 channels; D2 shared note allocator; D8 note events; D3 L/R as **two stereo-paired ports** | Channel Expander planned, not built. Legacy CLUSTER 1–12 and native unison 1–9 can exceed 16: provide explicit cap/refusal or configurable voice steal; **never** silently lose notes. |
| Macros and raw native modulation (`macros.py:95-126`; `voices.py:23-31`) | D6 stepped/plain-unit stable IDs; D7 attenuverters and per-voice mod; D16 subpatch macro fanout | M7 panel/macros/snapshots absent. Raw Surge mod destinations aren't all portable without extraction; automated translation and preset migrations **to verify**. |
| Bus ring/delay coupling and three-comb body (`chaos_dsp.py:13-102`) | D3 stereo, D7 CV, D10 bounded graph feedback; sample-exact recursive comb delay stays **inside** module | D10 inter-module feedback adds ≥ configured sub-block delay and cannot emulate cross-oscillator phase/FM within Surge. Mode changes need explicit smoothing/staged config. |
| Five-second stereo loop and space (`hostdsp.py:36-77`) | D11 FAST/LARGE host allocator, D12 optional size-capped loop state/latency, D3 stereo | At 48 kHz one stereo 5-s buffer is `5×48,000×2×4 = 1,920,000 B`; 16 independent voice buffers imply 30,720,000 B before other state. Explicit mono-global policy or prepare refusal required. No allocation in process. Looper deferred from lite. |
| Finite safe output (`hostdsp.py:80-106`) | D15 finite floats, no global fast-math; explicit safety stage | Clipping/normalization change has to be independently tested; the host's output protection does not substitute an exact CHAOS reference. |
| Realtime prototype calls SciPy/NumPy, unseeded global RNG, Python queue/lock and dynamic alloc (`engine.py:37,90,269-331,403-428,466-485`; `modlab.py:30,40-41`) | D11 preallocation, D15 deterministic per-channel seeds, D8 bounded events, D13 host ingress/capture | **Not portable as-is**. Do not copy the Python callback, dynamic queue, random module or lock into audio processing. |
| Fixed Surge 32 vs arbitrary frame counts (`engine.py:27-28,383-411`) | D1 any frame count 1/17/max; D12 report real latency | Any 32-frame packing adapter needs preallocated FIFO/scratch, correct timestamp/reset, finite outputs and documented/reportable latency; equivalence only at matching host conditions. Test at 44.1/48/96 kHz; refuse unsupported configs in prepare. |
| Presets, algorithms, raw B recipes (`ARCHITECTURE.md:31-37`) | D12 state and query_ports; D16 snapshots/scenes/subpatches | Named algorithms/topology are *future candidates*, not existing distinct DSP. Unknown future state must remain round-trippable at host level. |
| Physical target / performance | D14 C++17 SDK and D15 SIMD scalar/NEON/SIMDe, D11 memory reporting | SSE-only assumptions in sst need explicit Windows x64/Linux ARM64 validation and Cortex-M7 feasibility; no performance assertions from Python offline renders. |

**Expressibility decision:** all *functional* CHAOS blocks can be described with D1–D16 at the graph/module boundary; **none requires a new ABI field yet**. But the **product/host** does not yet implement every D16 feature (M7 subpatch front panels, macros, scenes), nor the Channel Expander. The following are **not** honestly expressible as currently behaving: (1) native Surge oscillator-to-oscillator phase/FM across separate generators via D7 cables, (2) sample-exact *inter-module* feedback without D10's specified delay, (3) arbitrary runtime reassignment of macro destinations without a graph/config transition, (4) unlimited CLUSTER×unison beyond 16 channels. These are DSP-topology/capacity/host-feature limits, not license to change the ABI or bypass realtime rules.

## 3. Independent primitive build order (after review; **no tape**)

### First — `ports.waveshaper` · **Waveshaper**

A self-contained GPL port of one pinned **sst-waveshapers** shaper family, not a Surge-branded oscillator. **Ports:** `in` (audio; poly), `out` (audio; poly); optional `drive_cv` only if exact reference semantics demand an explicit port. **Parameters:** `type` stepped enum chosen from verified upstream implementations, `drive` normalized 0–1 with an explicitly documented map to source units, `mix` linear 0–1 (if wet/dry added, isolate from reference equivalence). No note events; per-channel hot-state FAST, zero LARGE unless selected upstream routine requires tables. Caller owns a small fixed scratch/SIMD tail for 1..max frames and 1..16 channels. First spike **only one family/mode**, compare exactly to upstream built into tests before adding multiple waveforms, extra mixing or macros. This offers a reusable distortion/fold stage even outside CHAOS. **Risks:** SSE lane layouts/ARM SIMDe, lookup tables, exact source parameter units, tails, denormals and CPU on Pi. Proposed byte count and admissible rates are **to verify**.

### Second — `ports.comb_resonator` · **Comb Resonator**

Independent stereo three-comb resonant body, usable after any instrument. **Ports:** `in_l`/`in_r`, `out_l`/`out_r` as stereo pairs; poly channels mean simultaneous voices, *not* left/right. **Parameters:** `tone` 0–1, `feedback` 0–0.85, `mix` 0–1, all modulate in plain units with coefficient update bounded per frame or block. Two distinct possible designs: (A) reimplement the *specific new CHAOS algorithm* with exact three delays, damping and saturator (`chaos_dsp.py:66-102`) and independent legacy reference tests; or (B) first wrap an **sst-filters comb** exactly, then explicitly document **different sound** rather than claiming A/B equivalence. Choose B for a strict upstream port if performance and licence checks pass; A can be a separately reviewed non-external module. Fixed 48-kHz legacy storage: 3×2×max(2048,0.15×48000)×4 = **172,800 B per poly voice**, ×16 = **2,764,800 B**; request LARGE, plus small FAST damping/indices. At other rates recalculate and reject oversize budgets at prepare; report latency if any. No global allocations or algorithm change in a first port. **Risks:** SIMD, feedback stability, inter-block continuity and dynamic tone interpolation.

### Third — `ports.classic_oscillator` · **Classic Oscillator**

Candidate wrapper for an isolated **Surge XT ClassicOscillator** (dependency graph from the pinned `surge/src/common/dsp/oscillators/` is **to verify**). **Ports:** `pitch` (D4 1/octave), `fm` (audio-rate, independently documented scaling), `gate` or `note_in` event when genuinely used, and `out` (poly audio). **Parameters:** `shape`/`pulse_width`/`sub_level`/`unison` only as supported by pinned upstream units; typed min/max/taper **to verify** from that DSP, not guessed from `params.json`. 16 isolated per-channel phase/voice states via FAST, any required tables via LARGE, deterministic reset and source-equivalent reference. Explicit note channel mapping D2/D8 and internal 32-frame scheduling only if unavoidable. **Risks:** upstream oscillator isn't a standalone API, sample-rate/block constraints, state graph and large source footprint; spike may explicitly stop if minimal extraction requires a redesign. No entire Surge engine or built-in FX.

**Why this order:** waveshaping provides the smallest credible third-party code/ABI/SIMD probe (X-4). The comb body then validates LARGE memory, stereo and feedback; oscillator integration comes after the dependency, voice and sample-rate risks are understood. Neither the planned looper nor reverb nor tape is among the three. X-3 Plaits from issue #51 is a **separate** study/spike with no shared branch or edits; review the two studies together only for SDK gaps.

**Wait for M7 D16 subpatches/macros/snapshots:** MORPH, CHAOS, FOLD, MOTION and other many-to-one performance controls, front panel, alternate routing scenes, whole-patch preset management and deep RAW UI. **Wait for the Channel Expander:** CLUSTER, microtonal note fanout, unison DETUNE/VOICES and explicit notes×unison voice budget. The primitive modules and a hand-patched minimal graph do not depend on either future host feature.

## 4. Whole-Surge module versus components (X-6)

Wrapping one complete Surge XT engine as `ports.surge_engine` could preserve Surge's native 3-oscillator voice, LFO, preset and FX behavior more directly, but exposes a single monolith to the patch graph. The cost is the full synth control hierarchy, asset/tuning/preset compatibility, multi-voice allocator interactions, block scheduling, dynamic resources, latency, GPL notice audit and CPU/Daisy footprint; internal oscillator and filter connections remain unavailable as Wirescape cables. It is a **separate optional desktop feature X-6 after Beta 0.8**, not a shortcut for CHAOS. CHAOS should instead combine explicit independently useful primitives plus ordinary Essentials, because bus coupling, body, feedback and modulation must remain editable on the graph, independently profileable, and potentially exportable. Do not use the vendor name as a new module display name.

## 5. CHAOS-lite: Pi 5 **target** (not measured)

**Target:** 48,000 frames/s, 128-frame blocks → period **2,666.7 µs**, 25% CPU allotment **≤666.7 µs per block**, including host dispatch and the whole patch; use **worst-case measured block time**, not average. Proposed starting patch is **one or two channels** (no hidden 16-voice promise): 2× existing **Essential VCO** (or one ported oscillator after equivalence), simple detune/pitch drift through **Essential LFO + Scale/Offset**, Essential VCF/Ladder + ADSR/VCA per voice, Essential Mixer for stereo split, basic AM by LFO×VCA, and **one** optional three-comb body if it passes profiling. A single seeded slow instability CV gives the characteristic unstable motion. Use low feedback, limit polyphony explicitly, bypass heavy distortion where possible. Exclude **tape, looper, reverb/space, whole-Surge instances, raw Surge modulation and dynamic M7 scenes/macros**. Preserve the modules' own D1 frame/voice requirements; cap patch voice usage in the host rather than violating ABI.

Measure on **physical Pi 5** with `wirescape_soak` or dedicated offline probe at 48 kHz/128 frames: median, p95 and **worst block µs**, per-module CPU, startup/prepare memory FAST+LARGE, per-channel memory×16, denormal/extreme-signal behavior, reset/state and parameter-mod activity (including worst resonator feedback). Profile Windows x64 for regressions. Stage gates: (1) two VCOs+Essentials ≤ ~350 µs worst case (a **provisional sub-budget**, not a measurement); (2) AM/instability ≤ ~100 µs increment; (3) optional body+dispatch ≤ remaining ~216.7 µs. If any stage breaches 666.7 µs, **omit body or reduce active poly channels; do not falsify test/threshold data**. A 16-channel three-comb module's allocated maximum may still exceed embedded targets even when only 1–2 are active, so distinguish *prepared capacity* from *active voices*. Without actual measurements, feasibility remains **to verify**.

## 6. Subsequent-spike acceptance and handoff questions

For **every** future primitive, not for this docs-only issue: source-equivalence test compiles pinned upstream sources directly and feeds identical inputs; compare matching frame/rate/voice conditions and record any unavoidable numerical deviations. Prove deterministic seed/reset, state round-trip including unknown-version handling, allocation/lock/I/O/exception-free `process` and events, finite outputs at extremes, truthful FAST/LARGE memory requirements (per voice ×16), supported **44.1/48/96 kHz × 1/17/128/max frames × 1/4/16 channels**, and clear prepare refusal for unsupported combinations. Exercise both `windows-x64` and `linux-arm64` CI, with `.github/workflows/portability.yml` Cortex-M7 probe, and physical Pi 5 CPU per block. Report any sample-rate conversion or 32-frame buffering latency. For stereo tests, use paired L/R ports; channels mean polyphony.

**Planner questions / follow-ups (not actions in #52):** choose exact shaper enum and compatible transitive SHA set; inspect copyright on the selected files; decide whether the second port is *sst comb reference* or *CHAOS reference algorithm* (they are not interchangeable); decide policy for 16-channel ring buffers on Daisy; design Channel Expander voice refusal and 24-EDO tuning migration; define reversible legacy macro/preset→D16 mapping; identify and measure the first useful CHAOS-lite preset after M7. **No tape work is requested.**

**Study verification:** all listed legacy source files, `params.json` structure, ABI2 D1–D16 and the five named upstream repository files/directories were inspected through GitHub. No build, CTest, CI, simulator, native instrument, Windows or Pi device was run in this study. Performance and upstream reference equivalence are explicitly **to verify**.
