<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Curated patches: cinematic studies

> **2026-11-10:** the shipped, editable versions of these eight studies are now
> `apps/studio/patches/scene-01-…` to `scene-08-…` (renamed **Scene 1–8**, revised
> with reverb, delay, quantizer and 16-step sequences), alongside **Basics 1–7**.
> See `apps/studio/patches/README.md`. The table below describes the historical
> `curated.ts` snapshots, which stay as regression fixtures only.

Eight authored patch-v2 documents are available from **Starter patches** and the
read-only **Patch browser** in Studio (desktop and the WebAssembly browser demo).
They do not overwrite user-saved patches. The browser needs its **Start with
sound** gesture first. The Macro Oscillator requires a 48 kHz engine.

The sound-design vocabulary is deliberately constrained to the modules in the
current catalogue. CHAOS-lite is Wirescape's original two-oscillator/coupled-comb
instrument, **not** the legacy CHAOS plugin. The Macro Oscillator uses the
24-model DSP port of Mutable Instruments Plaits (Emilie Gillet); the three
selected model indices are the native engine's stable 0-based indices.
These are new Wirescape patches, not copies of third-party presets.

| Patch | Start | Signal path / movement |
|---|---|---|
| **01 · Hollow Chapel** | Automatic | Two low-register CHAOS bodies with delay-bend and ring coupling retain their left/right stereo pairs. Each channel gets its own mixer, ladder and Output bus; two slow LFOs move comb tuning and opposing filter cutoffs, while internal self-modulated LFOs add irregular motion. |
| **02 · Breathing Walls** | Automatic | A bent CHAOS voice and a quiet noise floor feed a resonant ladder. A slow clock advances a smoothed random source; a separate LFO shifts feedback and roughness. |
| **03 · Black Ice Choir** | Keyboard | Four-voice CHAOS with slow ADSR/VCA articulation (2.2 s attack, 7.2 s release), two independent slow modulators and internally drifting body resonance. Hold chords and let them overlap. |
| **04 · Vowel Apparitions** | Keyboard | Macro model **7** (Chiptune), 12-semitone lower register, into 12 dB VCF, long envelope and VCA. Independent 0.082 Hz morph and 0.031 Hz cutoff LFOs prevent locked-in repetitive sweeps. |
| **05 · Glass Cathedrals** | Keyboard | Macro model **12** (Additive synthesis) with trigger/velocity; main and auxiliary outputs are blended at 0.72/0.19 before a soft ladder, and two LFOs move timbre and filter independently. |
| **06 · Dust Reactor** | Automatic | Stock VCO (55 Hz) plus low noise into a mixer, 24 dB VCF and ladder. Clocked smooth random cutoff and slow LFO movement produce a nonrepeating industrial drone. |
| **07 · Witching Hour** | Automatic | Running clock (82 BPM, two ticks/beat) → eight-step D-minor ostinato → VCO → VCF → ADSR-controlled VCA. Eight pitches, velocities, tie and rest are authored. |
| **08 · Neon Elegy** | Automatic | Running clock (94 BPM, two ticks/beat) → eight-step A-minor/F/C/G-inspired line → Macro model **2** (six-operator FM preset 1) with internal low-pass gate → ladder. Velocity and slow morph modulation add accents. |

**Performance and expectations.** Automatic patches start when their graph is
rendered, not merely when a JSON file is opened with audio disabled. The browser
must first be allowed to run its AudioWorklet. Keyboard patches require a played
note. No MIDI device is required. The sequencer is monophonic: the sequences are
melodic/arpeggiated progressions, not simultaneous polyphonic chord changes.
Audio is routed to both output buses (Hollow Chapel uses *different* filtered signals for left and right; the others intentionally duplicate a mono route). All parameter values are inside the
published module descriptor ranges; modulation amounts are deliberately modest.
For headphone listening, start with a low output volume.

**Studio layout.** The curated graphs now use tighter source → processor → output columns and modulation rows, reducing unused canvas space at the application's minimum zoom. In the Starter dropdown and Patch browser, the graph is auto-fitted after load. Large graphs may still require panning on narrow phone screens.

## Preservation and scope

- The six **Qt** bundled patches (`qt-*`) were retired on 2026-11-10. Their
  roles are covered by the library's **Basics 1–7** patches and the bundled
  desktop **MIDI · Keys, Knobs and Wheels** patch
  (`apps/studio/src/app/bundled/`). The `qt-*` IDs stay reserved; the native
  `ui_starters.json` conformance fixture is unchanged. Git history keeps the files.
- The three **Class demo** patches in `native/examples/class_demo_*.json` are
  preserved unchanged. They were created for the owner's Pi/M8 test sessions
  (see `docs/tasks/done/P0A_DEMO_PATCHES.md` and `docs/DEMO_CLASS.md`).
- **Keyboard voice** remains the default Studio patch because the browser
  audio smoke test depends on it.
- Five old generic interactive starters (**Two-source drone**, **Clocked random**,
  **Eight-step sequence**, **Macro voice**, **CHAOS-lite**) were replaced in the
  interactive list. The generated `native/document/ui_starters.json` fixture
  now mirrors the fifteen active starters, so the native runtime conformance test
  also loads and renders the new patches. User patch folders, legacy examples
  and archived material are not deleted.

The patch format, module IDs, native engine, DSP, WebAssembly audio path and
session-file semantics are unchanged. For a sound check, listen to the pads on
headphones, hold keyboard chords for the slow envelopes, and confirm that both
sequenced patches produce audible note changes without keyboard input.
