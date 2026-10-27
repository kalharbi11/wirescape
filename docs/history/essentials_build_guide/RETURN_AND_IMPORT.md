# What to return and where it goes

Return `Wirescape_<folder>_Source.zip` with exactly one top-level `<folder>/` containing module.cpp, module.json, CMakeLists.txt, tests.cpp, README.md, DESIGN.md, TEST_REPORT.json and any required original-source license/notice files. Additional small source headers are allowed if disclosed and included by CMake. Do not return build directories, DLL/.so/.pyd/.exe, fonts, caches, host code, virtual environments or user data.

README explains inputs/outputs, units, defaults, missing inputs, reset, numerical limits and integration assumptions. DESIGN explains algorithm/source/provenance, smoothing/state/time semantics, bounded work/storage and remaining concerns. TEST_REPORT.json lists environment/compiler, exact commands, actual pass/fail/skipped results and known limits; missing tools mean untested, never pass.

Agent tests use `tests.cpp` as a standalone executable whose argv[1] is the module shared-library path. Follow the working example's engine discovery and descriptor calls. Keep module CMake to wirescape_add_module(ws_<folder>) plus any source-only target additions; no kit-specific paths, engine changes or host test wiring.

## Manual handoff

1. User saves the returned ZIP in the project or extracts into `incoming_modules/<folder>/` for review. Do not overwrite native/modules/vca, adsr, mixer, scale_offset or another existing folder. An enhancement to an existing module belongs in an explicitly reviewed separate patch, not a blind folder replacement.
2. Codex checks the file list/source, runs `python essentials_build_guide/tools/verify_submission.py incoming_modules/<folder>`, compares the assigned contract and ABI snapshot, and builds offline in an isolated kit copy. This validator is not a sandbox; compiling CMake/C++ executes trusted build/code, so review first.
3. After review, copy the module source folder into `native/modules/<folder>`. Root CMake already discovers module folders; the executable looks for built ws_*.dll/.so and presents descriptor-driven controls. Copying source alone does NOT compile or install a module.
4. Codex wires the returned tests into host CTest, tests graph discovery/save/load/parameter behavior, runs `python tools/dev.py check --functional-only --package` (during Ableton use), and refreshes Desktop/Start-menu shortcuts to the accepted package. Full benchmarks and physical audio remain separate, user-cleared tests.
5. Keep existing IDs and documents, preserve user_data and Surge, save a checkpoint. Windows compilation/local integration is still required after a successful regular-ChatGPT Linux test. Pi support is not established by either.

No automatic extraction/import/activation is provided; this packet is for predictable source handoff, not a marketplace.
