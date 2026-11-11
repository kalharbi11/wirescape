<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Building a new module with agents (start here)

This is the one page for adding a module to Wirescape 1.x, written for the owner and for any builder agent (ChatGPT, Codex, Claude). The engine and ABI 2 are done: a new module is a self-contained folder plus **five registration edits**, with no engine, protocol or ABI change. Detail behind each step: [SDK_AUTHOR_GUIDE.md](SDK_AUTHOR_GUIDE.md) (§ numbers below point there).

## A. Owner: asking for a module
1. Open an issue titled `Module: <name>`. Describe what it does, how it sounds, the knobs and jacks you want, and any reference (another module or a prototype). That is enough; Compass (or any builder) turns it into a task file.
2. Then paste this into a fresh ChatGPT/Codex agent session:

```text
You are a Wirescape builder (repo kalharbi11/wirescape). Read AGENTS.md, then
docs/MODULE_BUILD_GUIDE.md and follow section B exactly for issue #<N>
(task file docs/tasks/<FILE>.md). Claim the issue, build on branch
task/<N>-<slug>, open one draft PR with "Closes #<N>", make CI green,
post the handoff, and ask for a T1 review from another session.
Stop and post a handoff if any Stop in AGENTS.md applies.
```

3. A different agent session reviews it (T1). You test the sound once it merges.

## B. Builder: the checklist (in order)
**0. Read only:**
- `AGENTS.md`;
- the issue and its task file;
- the SDK guide §1–§12 (§15 if you're porting a prototype);
- the closest example:

| Building | Copy from |
|---|---|
| processor (filter, gain, shaper, effect) | `native/sdk/template/` (smallest), or `native/modules2/vca/` |
| source (LFO, random, noise, S&H) | `native/sdk/examples/random_source/` |
| notes/sequencer/arpeggiator | `native/sdk/examples/step_sequencer/` |
| a folder with its own CMake file (preferred for new modules) | `native/modules2/quantizer/` |

**1. Fix the contract first.** Write `native/modules2/<name>/README.md`:
- the module ID (`essentials.<name>` for first-party modules);
- every port and parameter ID, with its hint, unit, range and default;
- what an unpatched input reads, and any normalling;
- poly behaviour (who sets the channel count);
- what goes into saved state.

Once merged, these IDs are permanent (§2). If the task file already lists IDs, use them exactly.

**2. Write the module folder** `native/modules2/<name>/`:
- `dsp.h`: the class (§3–§11). The `ws::Info{"<id>","<Name>",1}` ID must equal `module.json`'s `id`.
- `module.cpp`: SPDX line, `#include "dsp.h"`, `WS_REGISTER_MODULE(<ns>::<Class>)`.
- `module.json`: `schema_version` 2, `abi_version` 2, `id`, `name`, `library: "ws2_<name>"`. `parameters` maps each ID to its **slot index** (0, 1, 2… in `params` order), not to its default. `inputs`/`outputs` map IDs to `"signal"` or `"event"`.
- `tests.cpp`: copy the closest example's tests (§12: frame, rate and channel matrix; reset; state; CONSTANT; extremes; no allocation), then add behaviour tests with independent reference math.
- `DESIGN.md`: "original code", or the ported source with its pinned revision and licence. Porting without a licence entry is a Stop.
- `CMakeLists.txt`, as in `native/modules2/quantizer/CMakeLists.txt`:

```cmake
# SPDX-License-Identifier: GPL-3.0-only
add_library(ws2_<name> SHARED module.cpp)
target_link_libraries(ws2_<name> PRIVATE wirescape_sdk2)
add_executable(abi2_<name>_tests tests.cpp ../../tests2/allocation_watch.cpp)
target_link_libraries(abi2_<name>_tests PRIVATE wirescape_engine2)
add_test(NAME abi2_<name>_tests COMMAND abi2_<name>_tests)
```

**3. Registration edits** (all five, or the module won't appear in the app):

| # | File | Add |
|---|---|---|
| 1 | `native/engine2/CMakeLists.txt` | `add_subdirectory(../modules2/<name> <name>)` next to the quantizer line |
| 2 | `native/control/module_catalog.h` | `#include "../modules2/<name>/dsp.h"` |
| 3 | `native/control/module_catalog.inc` | `WS_APP_MODULE(<ns>::<Class>)` |
| 4 | `apps/studio/public/catalogue-presentation.json` | one entry with the same `id`, plus `name`, `category`, `ports` (`id`, `name`, `direction`, `kind`, `hint`, `poly`, `stereo_pair`) and `parameters` (`choices` for stepped menus). Copy the quantizer entry's shape. |
| 5 | `apps/studio/public/wirescape_modules.json` | **generated**: run `python tools/gen_ui_catalogue.py` and never hand-edit it. `--check` fails CI on drift. |

**4. Patch-facing docs, in the same PR:**
- a section in `docs/PATCH_DESIGNER_GUIDE.md` covering ports, ranges, normalling and one usable wiring;
- optional: a custom card layout in `apps/studio/src/panels/layouts.ts` (otherwise the generic panel is used);
- optional: a library patch in `apps/studio/patches/` (see its README).

**5. Run locally** (devices stay off):

```sh
python tools/dev.py build --headless
python tools/dev.py test
python tools/gen_ui_catalogue.py --check
cd apps/studio && npm ci && npm test
```

No local build (e.g. ChatGPT without a shell)? Push the draft and let CI build it. Read the failing job's log tail, not the whole log.

**6. PR and handoff:**
- **PR body:** `Closes #<N>`, tier T1, the test results, `Patch guide impact: updated`, and anything this guide didn't explain.
- **Mark ready** when guard, linux-arm64, windows-x64 and sanitizers are green.
- **Handoff:** post the template from `AGENTS.md`.
- **Pi CPU check:** heavy DSP (FFT, many voices, convolution) adds a `pi-run:` CPU line (§12).

## C. Reviewer (another session, T1)
Check:
- IDs match the task;
- `module.json` slots match the `params` order;
- §8 realtime rules in `process`/`reset` (no allocation, locks, I/O or logging);
- the tests use independent math and include the no-allocation test;
- all five registration edits are present, and the generated catalogue is untouched by hand;
- the patch guide is updated;
- the required checks are green on the exact head.

Then post `Approved for merge <sha7>` and a `Signed:` line.

## Stops (post a handoff, set `status/blocked`)
Any of these stops the build:
- editing `native/include/wirescape/*.h` or `native/sdk/*.h`;
- changing an existing module's IDs;
- a new dependency;
- ported code without a licence;
- loosening a test;
- engine, sidecar or protocol edits (those make it T2).
