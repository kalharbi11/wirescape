# ARM64 headless validation guide

Updated October 26, 2026. This is a reproducible validation recipe, not evidence of ARM64 or Raspberry Pi support. No ARM64 build or hardware test has been run as part of preparing this guide.

## Local tool availability

Read-only command discovery on the Windows development machine found:

- `cmake.exe` at `<drive>/Program Files\CMake\bin\cmake.exe`.
- `wsl.exe` at `<drive>/Windows\system32\wsl.exe`; only the launcher was identified. WSL was not started, and no distribution availability is claimed.
- `clang++`, `clang` and `aarch64-linux-gnu-g++` were not found through `Get-Command`.
- Visual Studio 2022 Community and Build Tools installations were found. In both, MSVC 14.44.35207 exists, but `VC\Tools\MSVC\14.44.35207\bin\Hostx64\arm64\cl.exe` does not.

This is an inventory of the requested commands and known MSVC compiler paths, not an exhaustive search for other toolchains. Windows ARM64 compiler availability is separate from whether this project builds or runs on Linux ARM64 or a Raspberry Pi.

## Build and test on a Linux ARM64 host

Use a Linux ARM64 machine (for example, a Raspberry Pi) with CMake 3.20+, a C++17 compiler, and standard build tools already installed. Start from the repository root. The commands below disable the optional UI and Qt package lookup, so the native engine/tests are independent of Qt; they do not install dependencies.

```sh
uname -a
uname -m                         # expect aarch64 on a 64-bit ARM Linux host
cmake --version
c++ --version
git status --short

cmake -S native -B build/native-arm64 \
  -DWIRESCAPE_BUILD_UI=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE
cmake --build build/native-arm64 --parallel 2
ctest --test-dir build/native-arm64 --output-on-failure
```

The configured CMake project builds the shared engine, CLI, Essentials shared modules, and native test executables. CTest runs `native_engine_tests`, `native_module_tests` and `native_scale_offset_tests`. `native/document` uses QtCore optionally; disabling Qt package discovery omits `document_reader_tests`. With Qt 6.5+ Core already available, leave `CMAKE_DISABLE_FIND_PACKAGE_Qt6` unset to also configure and run the document reader test. UI targets remain disabled either way.

For a quick artifact and CLI check after a successful build:

```sh
file build/native-arm64/wirescape_cli build/native-arm64/libwirescape_engine.so
find build/native-arm64 -maxdepth 1 -type f \( -name 'libws_*.so' -o -name '*.module.json' \) -print
build/native-arm64/wirescape_cli
```

The engine's Essentials loader expects Linux module filenames `libws_vco.so`, `libws_adsr.so`, `libws_vca.so` and `libws_output.so` in the directory supplied to it; CTest passes the configured module output directory. Check that these are actual ARM64 ELF shared objects (`file`), not host-built or cross-built binaries for another architecture. Module manifests are copied beside module binaries, but the experimental loader currently loads by filename/export; manifests are not a trust or compatibility gate. Do not mix binaries built for different targets or assume the experimental native ABI is stable across builds.

## Evidence to record from a real ARM64/Pi run

Attach the exact command output and record the board model/revision, OS image and version, kernel, `uname -m`, compiler identity/version/target, CMake version/generator, repository revision and dirty state, configure/build logs, CTest summary, and `file` output for the engine, CLI, test programs and each Essentials module. Record whether QtCore was found and whether the document reader test ran. A result on a generic ARM64 host does not establish compatibility with every Pi model or OS image.

These headless checks must not open an audio device. They provide no physical audio or MIDI evidence. If a later UI/device validation is performed, separately capture a self-closing startup readiness report showing `audio_started: false` and `normal_window: true` before any explicitly authorized live-audio test; record the device and exact user action for that separate test. Do not infer live stability, polyphony, ABI compatibility, full legacy migration, or release readiness from compilation and offline tests.
