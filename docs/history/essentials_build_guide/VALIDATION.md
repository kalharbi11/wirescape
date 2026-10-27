# Actual kit validation

October 26, 2026, Windows x64 / MSVC 19.44 / Visual Studio 2022 / CMake 4.4.4.

- tools/run_checks.py configured/built the standalone example and passed its CTest, including actual dynamic module loading, independent ramp calculations, partition/reset/null-input and adjacent guard checks. Supported-rate ramp checks include 44.1/48/96 kHz.
- A freshly extracted copy of the share ZIP also configured, compiled and passed CTest with no references back to the Wirescape repository.
- Four Python submission-verifier tests passed, including malformed/missing source, forbidden binaries/build trees, and duplicate parameter IDs. The example passes structural validation.
- Main app `check-20261026T163643Z.json` passed 27 foundation Python tests, nine CTest suites, both UI sizes, audio-off startup/render and package inventory. Performance benchmarks were explicitly deferred while the user was working in Ableton.
- Final ZIP builder checks reference snapshots, source-only file types, CRC and byte-for-byte archive contents; SHA-256 sidecar and internal file manifest are provided.

These results validate the KIT/EXAMPLE and current app. They do not claim the six assigned modules are built, Linux/ARM/Pi or hardware audio support, any performance winner, or the correctness of future third-party submissions. Agents must run their own task-specific tests and state when compiler tooling is unavailable.
