<!-- SPDX-License-Identifier: GPL-3.0-only -->
# S1: Conjunctions modules (Track S, parallel; one issue + one PR per module)

**What.** The owner's web app *Conjunctions* joins two sources with twelve processes, rendered offline in a worker. Track S1 turns them into live Wirescape modules with two signal inputs (`A`, `B`) and CV on the musical controls. The owner gives you the app's source (one `index.html`; the DSP sits in `<script id="dsp">`, the controls and their ranges in the `PROCS` table). Keep its sound and its controls; rebuild the code to Wirescape's rules. Do your own research on the DSP; this file only says what must change and in what order.

**Read first:** `AGENTS.md`; `docs/SDK_AUTHOR_GUIDE.md` in full, **especially §15 (porting a prototype)**, §5 (channels are voices), §7–§8 (memory, realtime) and §12 (tests); `native/sdk/template/` and `native/sdk/examples/random_source/`; one two-input module for the port pattern, `native/modules2/vca/`; `native/control/module_catalog.inc` (how a module reaches the app); `docs/PI_RUNNER.md` (`pi-run:` lines). Claude Code: use the `new-module` skill. Don't read `legacy/`, `native/ui/` or the Qt code.

**Order (start at 1; one module per PR; each is mergeable on its own):**
| # | Module ID (proposed) | Covers | Why this order |
|---|---|---|---|
| 1 | `sound.blend` | Mix/crossfade + Splice (mode param). A/B balance CV, splice segment lengths, fade, jitter, a trigger input that forces a switch | Easiest; proves the two-input, mode-param, equal-power pattern the rest reuse |
| 2 | `sound.cross_mod` | Phase modulation by delay + AM/ring (mode) | Small; needs one bounded modulated delay line |
| 3 | `sound.waveset` | Waveset interleave, **"keep pace" only**, plus repeat | Time domain, no FFT, no latency setting; the hard part is bounded per-input cycle buffers that stay in time. Easier than Spectral |
| 4 | `sound.shaper` | Waveshaping with a slice of B as the curve (follow / freeze) | Rolling buffer of B, smoothed table swaps |
| 5 | `sound.spectral` | Spectral interleave, cross-synthesis, morph, max/mean/min/difference (mode); **morph position as CV** | One shared STFT engine; needs an original FFT in the module folder, `latency_frames()` = FFT size, hops on an absolute sample counter |
| 6 | `sound.vocoder` | Vocoder (bands, release, flatten carrier, noise for consonants) | Reuses #5's STFT code (copy or a shared header inside `native/modules2/`) |
| 7 | `sound.grains` | Granular brassage from a few seconds of live A/B | Fixed grain pool, stereo pair out |
| — | `sound.convolver` | Convolution | **Wait** for Track S3 (sample buffer service); a capture-from-B version may come first if the S3 ruling allows |

**What must change from the prototype (the full table is guide §15):** streaming instead of whole-file renders; Waveset's "concatenate" mode and convolution's "keep tail" are dropped (outputs can't be longer than inputs); no end-of-render normalising (`levelise`/`finish`); all buffers sized in `prepare` for 96 kHz; no fixed 44.1 kHz; `ws::Random` instead of the fixed render seeds; mode menus → stepped params; the provenance/spectrogram/scope display stays out (Phase 3A); the built-in sources (pads, chant, waves, samples) are **not** ported: patch Wirescape's VCO, Noise, Macro Oscillator or any other module into `A` and `B`.

**Licence and provenance.** The owner wrote Conjunctions; the ports are GPL-3.0 under `native/modules2/`. It names CDP processes as inspiration: don't copy CDP code (LGPL). Record the prototype, what changed and why, and a listening note in each module's `DESIGN.md`.

**IDs.** The IDs above are proposals: confirm them in the issue before the first PR merges; after that they never change. Each module also adds a starter patch in `apps/studio` only if the task's issue says so (default: no).

**Stops (in addition to `AGENTS.md`):** any change to `native/include/wirescape/*.h` or `native/sdk/` outside a new example; a new dependency (an FFT library included); file I/O in a module; a module that needs more than 32 params or ports.

**Per-module acceptance** (copy into each issue, filling `<name>`; `pi-run:` only for #5, #6 and #7):
```acceptance
tier: T1
size: <= 900 changed lines, <= 8 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "abi2_<name>_tests" --output-on-failure
  - python tools/dev.py test
  - pi-run: python tools/module_cpu_probe.py <name>   # two instances, 48 kHz / 128 frames, worst block < 50% of the period (only #5-#7; add the probe in #5 if it doesn't exist)
stops: [ABI header edit, new dependency, file I/O in a module, test loosening]
```
The tests are the guide §12 list (frame/rate/channel matrix, reset, state, CONSTANT, extremes, no allocation) plus behaviour tests per process with independent math (for example: Blend at balance 0.5 is −3 dB per side; Waveset with two sines of known period yields the composite period; Spectral morph at 0 and 1 matches A's and B's magnitude spectra within a tolerance after the latency).
