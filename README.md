# Wirescape™

**An open-source modular patching environment for music.** It's in alpha and under active development.

Wirescape is a native C++ realtime engine with a standalone Qt Quick interface. Its focus:
- polyphonic cables, audio-rate modulation and feedback patching
- a simple module SDK that people and AI agents can both build with
- module DSP that moves unchanged into plugins (CLAP/VST3) and Daisy-class hardware

MIDI, OSC and UDP are planned, followed by DMX, serial and scripting.

- **Status and evidence:** [docs/STATUS.md](docs/STATUS.md)
- **Plan and milestones:** [docs/PLAN.md](docs/PLAN.md)
- **Architecture today:** [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- **Module contract (ABI 2):** [docs/specs/ABI2_PLAN.md](docs/specs/ABI2_PLAN.md)
- **Contributing / agents:** [CONTRIBUTING.md](CONTRIBUTING.md) · [AGENTS.md](AGENTS.md)

## Build (Windows x64)
Requirements: Visual Studio 2022 (MSVC), CMake ≥ 3.20, Python 3.12, and Qt 6.10 for MSVC x64.

```powershell
$env:WIRESCAPE_QT_DIR = "C:\path\to\Qt\6.10.3\msvc2022_64"   # or set CMAKE_PREFIX_PATH
python tools/dev.py doctor
python tools/dev.py build
python tools/dev.py test
python tools/dev.py launch      # opens the GUI with audio off; start audio from the app
```

Linux ARM64 / Raspberry Pi 5: `python tools/dev.py build --headless` builds the engine and tests without the UI.

## Repository layout
```
native/        engine, modules (Essentials), Qt UI, tests
tools/         build, test, benchmark and package scripts (tools/dev.py)
docs/          plan, architecture, status, specs, tasks, reviews, design, history
legacy/        the original Python/browser prototype, kept for reference only
```

## Licence
- The app and Essentials modules: **GPL-3.0** (see `LICENSE`).
- The module SDK and ABI headers (`native/sdk/`, `native/include/wirescape/module2.h`, `native/voice/`): **MIT** (see `LICENSES/MIT.txt`).
- Modules derived from third-party GPL code (for example Surge-based DSP) stay GPL.
- "Wirescape" and its logo are trademarks: see [TRADEMARKS.md](TRADEMARKS.md).
