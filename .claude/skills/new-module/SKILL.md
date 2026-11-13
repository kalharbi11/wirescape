---
name: new-module
description: Build or change a Wirescape ABI 2 module (a sequencer, random source, oscillator, filter, effect, MIDI/OSC tool). Use whenever the user asks to create, port or modify a module in native/modules2/ or native/sdk/examples/.
---
<!-- SPDX-License-Identifier: GPL-3.0-only -->
# New Wirescape module

1. Read `AGENTS.md` (engineering rules) and `docs/SDK_AUTHOR_GUIDE.md` in full. Read nothing else broad.
2. Pick the starting point from the guide's table and copy that folder to `native/modules2/<name>/` (or the user's chosen folder):
   - processor → `native/sdk/template/`
   - source (random, LFO, S&H) → `native/sdk/examples/random_source/`
   - notes / sequencer → `native/sdk/examples/step_sequencer/`
3. Before writing code, write down in `README.md`: module ID, every port and parameter ID with units and ranges, poly behaviour (who sets the channel count), and state contents. IDs are final once merged.
4. Implement `dsp.h`. Check it against the guide's §8 realtime checklist line by line. Porting a prototype (Web Audio, offline renderer)? Apply the guide's §15 table first.
5. Adapt the example's `tests.cpp`: keep the frame/rate/channel matrix, reset, state round trip, CONSTANT, extremes and no-allocation tests; add behaviour tests with independent reference math.
6. Do the five registration edits in `docs/MODULE_BUILD_GUIDE.md` §B.3: the module `CMakeLists.txt` + `add_subdirectory` in `native/engine2/CMakeLists.txt`, the include in `native/control/module_catalog.h`, `WS_APP_MODULE` in `native/control/module_catalog.inc`, the entry in `apps/studio/public/catalogue-presentation.json`, then `python tools/gen_ui_catalogue.py` (never hand-edit `wirescape_modules.json`).
7. Run `python tools/dev.py build --headless` and `python tools/dev.py test`. Both must pass.
8. Never open audio/MIDI devices or LAN sockets. Never edit `native/include/wirescape/*.h`. Never weaken a test to pass.
9. Finish with the guide's "Definition of done" checklist, and list anything the guide didn't explain in your final message (it improves the guide).
