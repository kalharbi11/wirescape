<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Quad VCA / Low-pass gate (#490: Q1 #601, Q2 #602)

This is an owner request, unshelved on 2026-11-11. It adds a new module with the new ID `essentials.quad_vca`, named **Quad VCA**. The existing `essentials.vca` is not touched. The DSP is original: if anything is ported, the source and licence go in `DESIGN.md` first, and that is a Stop. The tier is T1: no SDK, ABI or protocol change.

## Contract (IDs are final once merged)
- **Inputs:** `in1`…`in4` (`WS2_AUDIO`, poly) and `cv1`…`cv4` (`WS2_CV_UNI`, poly, default 1).
- **Outputs:** `out1`…`out4` (`WS2_AUDIO`) and `mix` (`WS2_AUDIO`, the sum of `out1`–`out4`, not normalised; clamp to ±4).
- **Params:**
  - `level1`…`level4`: 0–1, default 1, modulatable and per-voice.
  - `mode1`…`mode4`: stepped, 0 = VCA, 1 = LPG, default 0.
  - `response`: 0–1, default 0.4. It sets the LPG decay, shared by all four channels.
- **Normalling:** an unpatched `inN` (N > 1) takes the *input* signal of channel N−1, so one source can feed four gates. An unpatched `cvN` means the level knob alone sets the channel.
- **Control:** `c = clamp(levelN · cvN, 0, 1)` per sample and per voice channel.
- **VCA mode:** `out = in · c`, linear, so it matches `essentials.vca` at gain 1.
- **LPG mode (vactrol-like):**
  - A one-pole follower `v` tracks `c` with a fast attack (≈ 2 ms) and a slow release that `response` maps exponentially from ≈ 60 ms to ≈ 1.5 s.
  - `v` drives two things together:
    - **Amplitude:** `v` (or `v^1.5`).
    - **2-pole low-pass cutoff (TPT/SVF, fixed mild Q ≈ 0.6):** exponential from ≈ 40 Hz at `v = 0` to ≈ min(18 kHz, 0.45·fs) at `v = 1`.
  - The result is that a pluck blooms and then closes darker as well as quieter.
- **Poly:** channels follow `in1` (or the widest patched input), up to 16. State is per voice channel in fixed arrays.
- **State:** none is saved beyond the parameters. `reset()` zeroes the followers and filters.

## Q1 #601: native module (builder: any independent agent)
- Add `native/modules2/quad_vca/`: `module.json`, `dsp.h`, `module.cpp`, `README.md` (the contract above), `DESIGN.md` and `tests.cpp`.
- Follow `docs/MODULE_BUILD_GUIDE.md` §B step by step, including all five registration edits.
- **Tests** (independent reference math):
  - the frame/rate/channel matrix and CONSTANT input;
  - VCA attenuation is exact (level 0.5 → −6.02 dB ±0.05);
  - normalling (only `in1` patched → all four outputs carry it);
  - `mix` equals the sum of the four outputs;
  - LPG: a gate held at 1 and then dropped to 0 decays monotonically, and `response` 1 decays more slowly than `response` 0;
  - LPG closing: the RMS above 2 kHz falls faster than the RMS below 200 Hz while it decays;
  - output stays finite at extremes, with 16 voices at 32 kHz and 96 kHz;
  - no allocation in `process`.
- **Patch guide impact:** add a short Quad VCA entry (ports, modes, normalling, a usable wiring) to `docs/PATCH_DESIGNER_GUIDE.md`.

## Q2 #602: Studio card and patches (after Q1 merges)
- Studio card: four columns (level knob, VCA/LPG switch, `in`/`cv`/`out` jacks) and one shared Response knob. Use the generic panel unless a layout entry is needed in `apps/studio/src/panels/layouts.ts`. Playwright: the card renders, the mode switch sends `setParam`, and the result passes with FakeEngine.
- Two library patches in `apps/studio/patches/`:
  - *LPG plucks*: Groove or keyboard → ADSR → `cv1` (LPG mode, a VCO in `in1`);
  - *Four-voice gate mix*: one source normalled across four channels, each with its own LFO on `cvN`, into `mix`.
  Both pass the offline library smoke (the render test that flags silent or invalid patches).
- Update the guide with the patch descriptions.

```acceptance
tier: T1
size: <= 700 changed lines, <= 12 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [ABI/SDK header edit, change to essentials.vca, an ID differing from this file, ported code without a licence entry, allocation or locks in process, new dependency]
```

Signed: Compass (Claude), 2026-11-11
