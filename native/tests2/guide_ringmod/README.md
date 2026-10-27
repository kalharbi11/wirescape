# Guide-authored Ring Mod example

This module was authored using only `docs/SDK_AUTHOR_GUIDE.md` and these template files as author instructions: `native/sdk/template/module.cpp`, `module.json`, `README.md`, `DESIGN.md`, and `tests.cpp`. At the start of this exercise, `tests.cpp` was missing; the root agent added a standalone callback example and testing-bootstrap paragraph to the guide before this module's tests were authored. No other SDK implementation, engine2 code or ported module source was read for this authoring test.

The module ID is `example.ringmod2`; its library name is `ws2_guide_ringmod`. Inputs A and B are AUDIO/POLY signals, output Audio is AUDIO/POLY, and Level is a modulatable 0–1 parameter with default 1. The host handles 1-channel broadcast. The processor multiplies `A * B * Level` independently on each voice channel. Run the `tests.cpp` wrapper test with the SDK include directory and this folder in the include path.

Authoring findings:

- The original template directory lacked `tests.cpp`, and the guide lacked a callback bootstrap. Both were added during this exercise. The example made the callback test straightforward without reading SDK implementation.
- The guide says the default parameter flags include MODULATABLE and AUTOMATABLE; this module relies on that default. A concrete flag-constant example would help authors who need non-default flags.
- The metadata template maps `"gain":0` without explaining whether the number is a legacy parameter index or value. This module mirrors `"level":0`; the guide should clarify the field's meaning.
- The first compile failed because the module stored `b.in(i).ch(c)` as `const float*`. That accessor returns a constant-aware channel view: keeping it as `auto` preserves sample-zero reads when one input is CONSTANT and another varies. The guide now clarifies this; the added mixed-signal case tests it.
