# Self-contained regular-chat task: standard mono mixer module

This task can be completed without access to the owner's computer. Return a ZIP of the specified files and a concise implementation/test note. Do not redesign the host or invent local hardware/benchmark results.

## Project context

Wirescape is a standalone visual modular instrument for Windows and Raspberry Pi. Its new experimental foundation has a C++17 headless engine, native Qt Quick UI, and independently compiled module folders. Existing standard modules are a sine VCO, mono ADSR, VCA and Output. We want one small additional stock Essentials module.

## Your isolated deliverable

Create only:

```text
native/modules/mixer/CMakeLists.txt
native/modules/mixer/module.cpp
native/modules/mixer/module.json
native/modules/mixer/TEST_NOTES.md
```

Use the attached `module.h` as the authoritative experimental ABI. Use the VCA example's naming/export/build conventions. The repository's CMake helper is `wirescape_add_module(ws_mixer)`; the host discovers module subdirectories. Do not change host files or the ABI.

## Behavior

- Stable module ID `essentials.mixer`, name `Mixer`.
- Two mono audio inputs, stable IDs `a` and `b`; one mono audio output, ID `audio`.
- Parameter 0 `Level A`, parameter 1 `Level B`, both range 0..2, default 0.5.
- Every sample: output = connected A * Level A + connected B * Level B. An unconnected input contributes zero. No clipping, normalization, saturation or automatic gain changes.
- Parameter events update the corresponding coefficient. Gate/note events are ignored.
- `prepare` initializes state with the configured rate/max quantum; `reset` preserves user parameter values. Processing writes every requested output sample, handles any frame count within preparation limits, and does no allocation, locking, file access, logging or interpreter work.
- Use plain C ABI descriptors and fixed numeric state. No external dependencies, GUI code, OS-specific API, or threads. Do not introduce unnecessary SIMD/thread infrastructure for this small loop.

## Acceptance evidence

Document or execute portable tests for: both inputs disconnected; only A; only B; two sources summed at default and changed gain; zero gain; negative input samples; frame partition invariance; reset preserves parameters; no changed output beyond the requested frame count. If you have a compiler, a small standalone descriptor-level harness is sufficient; identify your platform and command. Otherwise clearly label tests as proposed, not executed.

Check `module.json` against the attached example's schema/conventions. Return only the four deliverable files and your concise summary. Do not include project dependencies, native binaries, personal data, or a full application rewrite.
