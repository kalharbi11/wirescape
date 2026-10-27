# CHAOS — a complex synth voice

**Engine prototype 0.1, extracted from GUTTR beta 0.4.** Designed as a self-contained two-generator experimental synthesis module, initially on Windows and Raspberry Pi 5, with a controlled future audio interface for other modules (including the GUTTR vocal synthesizer). This distribution is **source code**, not a prebuilt executable.

CHAOS is meant to **sound and work independently**. Connecting a vocal module is an optional future extension, never a requirement. This stage intentionally prioritizes the sound engine, modulation, routing, and integration boundary **before finalizing a patch-panel UI**.

## What is retained from GUTTR

- **Surge XT (via `surgepy`)** generators A and B. Each has three oscillators and access to their type-dependent parameters, two filters, envelopes, two native Surge LFOs, and five native Surge modulations through the raw panels. B can be activated with a separate level/pan and starting voice recipes.
- **All original non-vocal study-guide macros**, with the original Surge parameter mappings in `macros.py`: BREAK, DRIVE_TYPE, ROUGH_RATE/DEPTH, JITTER, SUB, FM_RATIO, DETUNE, VOICES, DRIFT, OFFSET, CLUSTER, QUARTER, FALL, CUTOFF, TAPE, SPACE/SPACE_MIX.
- The existing **tape + reverb effects inside Surge** and host wow/flutter, detune, pitch fall, pitch jump and 24-EDO tunings. Some techniques described in the study guide, such as full rhythmic disorder and low-frequency tactile effects, remain future development rather than falsely claimed complete.
- A **five-second stereo varispeed sound-on-sound looper** (forward/reverse, feedback, freeze), a self-modulating LFO with multiple waveforms, parameter randomization with repeat/undo, meters, and the output safety limiter.
- The source parameter catalog `params.json`, original non-vocal presets, and Windows / Pi build scripts.

## Removed — deliberately

- Singer C, five-formant insert, the full 34-section GUTTUR/Node.js bridge, all throat/vowel/crazy voice macros.
- The separate MERGE, vocoder, braid, and FRY processing chain and all of its settings.
- LAB vocal offline processing, mic-recording UI, and sample recordings. No unnecessary source WAV collection or Node.js requirement.
- The old GUTTR UI tab hierarchy. A small **temporary engineering control panel** is included only to exercise functionality while the eventual hardware-like interface is still being designed.

## New architecture groundwork — not a finished algorithm library

```text
MIDI / NOTES / TUNING
       |           |
       v           v
   SURGE A      SURGE B           future standalone voice/input
       |           |                           |
       +-----+-----+                           |
             v                                 |
      GENERATOR BUS ROUTING <--- 4-slot MOD MATRIX (control rate)
      0 parallel   1 AM   2 modulated delay    |
             |                                 |
             +------------ EXT_MIX <------------+
             |
             v
      RESONANT BODY (three damped combs, bounded feedback)
             |
             v
      ROUGHNESS AM (legacy paper macro)
             |
             v
      VARISPEED STEREO LOOPER
             |
             v
      DC BLOCK / SAFE LIMITER (-1 dBFS)  --> OUTPUT
```

Additional self-mod LFO and randomizer controls operate on macro parameters; the original native Surge modulation still operates **inside each synthesizer**. `ROUTE_MODE` is a **prototype audio-bus topology selector**, not a claim of audio-rate oscillator phase/FM cross-modulation inside Surge. The resonant body is a **generic comb structure**, not a vocal tract.

### Controls newly added

| Control | Range | Function |
|---|---|---|
| `A/B_LEVEL` `A/B_PAN` `A/B_MUTE` | stereo layer levels | independent generator buses |
| `ROUTE_MODE` | 0/1/2 | parallel, AM/ring interaction, or short-delay modulation |
| `ROUTE_DEPTH` | 0–1 | clean parallel to modified bus; zero fully bypasses routing |
| `BODY_MIX` | 0–1 | parallel comb-based resonant body mix |
| `BODY_TONE` | 0–1 | resonant delay spacing (shorter at higher tone) |
| `BODY_FEED` | 0–0.85 | damped, saturating feedback (bounded) |
| `EXT_MIX` | 0–1 | generic future stereo input, pre-body; disconnected by default |
| `MOD1..4_SRC` | off/LFO/S&H/smooth/random/output envelope | four independently assignable control-rate sources |
| `MOD1..4_DST` | named macro targets | per-slot target |
| `MOD1..4_AMT` | -1..+1 | bipolar modulation depth, normalized to target's valid range |

**Important:** routes are control-rate per 512-sample audio callback, not a fully modular audio-rate CV matrix. Feedback is strictly bounded and the master output hard-ceiled. Switching routing modes while playing can click; smoothly crossfading topology changes is a later task. The original macro targets affect the primary A Surge voice except global effects (which also go to B).

## Setup / run

### Windows 10/11 (64-bit)

Install Visual Studio C++ tools, Git, CMake and **Python 3.12 64-bit**. In the extracted directory:

```bat
build_windows.bat
run.bat
```

`build_windows.bat` clones and compiles Surge's Python binding (`surgepy`) and installs Python dependencies. Once built, `run.bat` starts the temporary UI. **No compiled binding is inside this ZIP.** Do not regenerate `params.json` unless you deliberately change the Surge version or parameter schema.

### Raspberry Pi 5, 64-bit Raspberry Pi OS Desktop

Use cooling and a graphical desktop. Build natively:

```bash
chmod +x build_pi5.sh run_pi5.sh
./build_pi5.sh
./run_pi5.sh
```

The Pi script builds `surgepy` from source; this has **not been physically tested on Pi 5**. Dependency or upstream build changes may require adjustments.

### Headless regression checks (no Surge required)

```bash
python tools/test_core.py
python tools/test_engine_mock.py
python tools/render_core_previews.py
```

The core previews use **simple test sine signals**, not the Surge voice, and exist only for assessing new routing/resonator timbres. For **real** Surge voice renders after building Surge:

```bash
python tools/render_full_demos.py
```

This generates 5 short WAVs and prints RMS, peak level, and achieved offline speed. Runtime glitches, native Surge parameter correctness, UI rendering, live audio, and Pi/Windows compilation still require on-device verification. There is no justified claim yet that this entire instrument meets a realtime or sound-quality target.

## Presets

Legacy non-vocal presets remain in `presets/` (the original five study-guide demonstrations). The `KEEP` action saves the current parameter state, B voice type, raw Surge parameters, LFO shape, and LFO target. A preset stores **parameter values**, not a separate compiled algorithm; this foundation does not yet include the eventual algorithm catalog.

## Project anatomy

| File | Role |
|---|---|
| `engine.py` | Two-Surge host, audio-thread event queue, voice timing and audio graph |
| `macros.py` | Original study-guide mapping + new bus/matrix control schema |
| `voices.py` | A/B raw oscillator/filter/envelope controls, B starting patches |
| `chaos_dsp.py` | Generic stereo oscillator-bus routing and resonant body |
| `modlab.py` | Self-mod LFO, four-slot macro modulation matrix, randomizer |
| `hostdsp.py` | Roughness AM, stereo looper, DC blocker + -1 dBFS limiter |
| `analysis.py` | Live scope, spectrum, spectrogram, modulation roughness |
| `ui.py` | **temporary** functional engineering panel, not final interface |
| `params.json` | Existing introspected Surge parameter names |
| `PORTS.md` | Explicit audio/control integration contract for later modules |
| `ARCHITECTURE.md` | Decisions, references, experimental algorithm roadmap |
| `tools/` | Headless DSP tests, mock engine test, offline render tools |

## Influences and design boundaries

Befaco [Oneiroi](https://www.befaco.org/oneiroi/): integrated oscillator/FX/looper approach, expressive self-modulation, targeted randomization with undo. The synth and project code in this archive are **not derived from Oneiroi**.

Forge TME [VHIKK X](https://forge-tme.com/vhikk-x/) and its [algorithm reference](https://forge-tme.com/vhikk-x/algos_06.html): oscillator clusters, cross-modulation, adjustable processor architectures, audio-rate feedback networks. CHAOS does **not** reproduce those proprietary algorithms or assert access to their implementation.

The user's *How Music Learned to Be Scary* study guide: linear-to-nonlinear transitions, roughness 30–150 Hz, unstable pitch/detune/microtonality, decay/tape, irregularity, uncanny sound. Existing non-vocal mappings are retained; later algorithms should address the categories not yet represented.

## Licensing

GPL-3.0; see `LICENSE`. The Surge XT dependency is GPL-3.0 and must be built from its own source. Attribution to inspirational products is architectural, not a claim of endorsement or code sharing.
