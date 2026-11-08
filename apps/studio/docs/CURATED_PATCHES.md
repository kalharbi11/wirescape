<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Curated patches: cinematic studies

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
| **01 · Hollow Chapel** | Automatic | Two low-register CHAOS voices, using delay-bend and ring coupling, feed a mixer and ladder. Two very slow LFOs move comb-body tuning, cutoff and roughness; internal self-modulated LFOs add motion. |
| **02 · Breathing Walls** | Automatic | A bent CHAOS voice and a quiet noise floor feed a resonant ladder. A slow clock advances a smoothed random source; a separate LFO shifts feedback and roughness. |
| **03 · Black Ice Choir** | Keyboard | Four-voice CHAOS with slow ADSR/VCA articulation (2.2 s attack, 7.2 s release), two independent slow modulators and internally drifting body resonance. Hold chords and let them overlap. |
| **04 · Vowel Apparitions** | Keyboard | Macro model **7** (speech/formant), 12-semitone lower register, into 12 dB VCF, long envelope and VCA. Slow LFO shifts morph and cutoff. |
| **05 · Glass Cathedrals** | Keyboard | Macro model **12** (modal resonator) with trigger/velocity, then a soft ladder; two LFOs move timbre and filter independently. |
| **06 · Dust Reactor** | Automatic | Stock VCO (55 Hz) plus low noise into a mixer, 24 dB VCF and ladder. Clocked smooth random cutoff and slow LFO movement produce a nonrepeating industrial drone. |
| **07 · Witching Hour** | Automatic | Running clock (82 BPM, two ticks/beat) → eight-step D-minor ostinato → VCO → VCF → ADSR-controlled VCA. Eight pitches, velocities, tie and rest are authored. |
| **08 · Neon Elegy** | Automatic | Running clock (94 BPM, two ticks/beat) → eight-step A-minor/F/C/G-inspired line → Macro model **2** (FM) with internal low-pass gate → ladder. Velocity and slow morph modulation add accents. |

**Performance and expectations.** Automatic patches start when their graph is
rendered, not merely when a JSON file is opened with audio disabled. The browser
must first be allowed to run its AudioWorklet. Keyboard patches require a played
note. No MIDI device is required. The sequencer is monophonic: the sequences are
melodic/arpeggiated progressions, not simultaneous polyphonic chord changes.
Audio is routed to both output buses. All parameter values are inside the
published module descriptor ranges; modulation amounts are deliberately modest.
For headphone listening, start with a low output volume.

## Preservation and scope

- The six **Qt** bundled patches are preserved byte-for-byte in
  `apps/studio/src/app/qt-starters/`; their read-only Patch browser entries
  remain available for historical session regression checks.
- The three **Class demo** patches in `native/examples/class_demo_*.json` are
  preserved unchanged. They were created for the owner's Pi/M8 test sessions
  (see `docs/tasks/P0A_DEMO_PATCHES.md` and `docs/DEMO_CLASS.md`).
- **Keyboard voice** remains the default Studio patch because the browser
  audio smoke test depends on it.
- Five old generic interactive starters (**Two-source drone**, **Clocked random**,
  **Eight-step sequence**, **Macro voice**, **CHAOS-lite**) were replaced in the
  interactive list. The frozen `native/document/ui_starters.json` fixture
  remains unchanged for legacy engine conformance testing. User patch folders,
  legacy examples and archived material are not deleted.

The patch format, module IDs, native engine, DSP, WebAssembly audio path and
session-file semantics are unchanged. For a sound check, listen to the pads on
headphones, hold keyboard chords for the slow envelopes, and confirm that both
sequenced patches produce audible note changes without keyboard input.
