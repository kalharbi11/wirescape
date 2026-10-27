# Building and checking the example submission

This folder is a self-contained offline C++17 kit. It includes the native engine ABI and host implementation, two small source references, and `submissions/example_gain` as a working example. It does not require Qt, Python DSP, Surge, a device, or network access.

From this directory, configure and build with:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build --output-on-failure -C Release
```

To run the bounded wrapper (configure, build, then CTest), use `python tools/run_checks.py`. It writes a JSON report under the selected build directory. Missing CMake/CTest is reported as skipped and exits with status 2; a skipped step is never reported as a pass. Each subprocess has a finite timeout (120 seconds by default). Set a different positive timeout with `--timeout SECONDS` or select a build directory with `--build-dir PATH`.

To check a submitted folder without compiling or running its code:

```text
python tools/verify_submission.py submissions/example_gain
```

This performs structural checks only. It requires regular `module.cpp`, `module.json`, `CMakeLists.txt`, and `tests.cpp` files; validates basic manifest fields; and rejects symlinks, generated build folders, and common prebuilt executable/library/archive/font/zip files. It does not certify safety, ABI correctness, or DSP behavior. Do not execute untrusted submissions outside an appropriately isolated review environment.

## Authoring a module

Copy `submissions/example_gain` into a new folder below `submissions/`, then change the module ID, library target, code, and checks. The module's `CMakeLists.txt` must call only `wirescape_add_module(target)`. This helper is deliberately compatible with the host repository's existing helper of the same name, so a returned module folder can be copied unchanged into `native/modules/<folder>`.

The kit's root CMake discovers those module folders and creates a test executable from each folder's `tests.cpp`, linked to `wirescape_engine`, then runs it with the built module library path as its only argument. In the main repository, tests are integrated separately; the submission CMake file must not assume any extra kit-only CMake helper exists.

The manifest uses `parameters` as parameter-name-to-numeric-ID mappings, matching the native module conventions. Keep it consistent with the descriptors in `module.cpp`. The engine ABI supports mono audio/control buffer ports and parameter/note/gate input events. It has no polyphonic ports, event outputs, or generic structured-message ports. All DSP callbacks must remain bounded, allocation-free, nonblocking, and exception-contained.

`smoke_tests/` contains a checklist for reviewing the built example and checking that the host remains offline. These checks do not open an audio device.
