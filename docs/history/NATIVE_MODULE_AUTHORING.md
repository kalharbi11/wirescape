# Experimental native module authoring

This describes the standalone native build, separately from the legacy Python/browser API in MODULE_API.md. ABI version 1 is experimental; rebuild the host and modules together. Windows x64 is locally verified; ARM64/Pi is not yet verified.

## Small module packet

An ordinary module has `native/modules/<folder>/module.cpp`, `module.json`, and a CMakeLists containing `wirescape_add_module(ws_<library_name>)`. Root CMake discovers these directories. The DLL exports `ws_module_entry` returning an immutable `ws_module_descriptor` from `native/include/wirescape/module.h`. Stable module IDs, numeric parameter IDs and port IDs are document identities; changing them can break saved patches.

The app discovers `ws_*.dll` beside its executable and builds generic controls from descriptors. Packaging copies those DLLs and manifests generically. Manifest metadata does not yet enforce trust or validate agreement with the binary descriptor. Installing a trusted native DLL executes native code; there is no isolation, automatic installer or hot reload. Use only reviewed local modules and restart after replacement.

Use Scale/Offset as a compact reference: `native/modules/scale_offset/` plus `native/tests/scale_offset_tests.cpp`. Its contract is one CONTROL input/output, `out = input * scale + offset`, missing input zero, no clipping. Scale is -2..2/default 1; Offset -1..1/default 0. Both use the existing allocation-free 5 ms ramp, initial/reset targets snap, and repeated values do not prolong a transition. It deliberately adds no host branch or new event interface.

## Execution contract

- `create`, `prepare` and `destroy` run while processing is quiescent. Return allocation/preparation failures rather than allowing exceptions across the C ABI. Descriptor data must outlive all processors that reference it.
- `process`, `event` and `reset` must finish bounded work without allocation, locks, file/device access, Qt, Python or exceptions crossing the ABI. Prepare storage first. Processing receives the actual span length, which can be smaller than the configured maximum.
- AUDIO and CONTROL currently use mono float buffers, one source per input, and a prepared acyclic graph. Define unpatched input behavior explicitly. Output every requested sample, respect buffer bounds, and avoid block-size-dependent state updates.
- Parameters are scalar values with IDs, bounds/defaults and units. Use the shared ramp where smoothing is appropriate. Existing timing events are delivered by the host; this ABI does not yet offer a general event-output/scheduler/provider interface or arbitrary module-state serialization. Return a contract question when a design needs these capabilities.
- Output modules produce raw engine samples. The host applies its shared output gain/clamp once at the device/file boundary. Utility modules should not quietly duplicate that output policy.

## Acceptance before integration

Specify signal kinds, units, ranges, defaults, output clipping, missing-input behavior, reset and sample-rate semantics before implementation. Tests should check actual behavior, guarded buffers, supported sample rates, partition equivalence and a useful graph. Test DLL discovery by stable identity as well as direct processor calls. Add the test target in root CMake; keep module-specific knowledge out of the host. Code-inspect the processing path for allocations/locks; a fast offline result alone does not establish realtime safety.

From the repository root, `python tools/dev.py check --package` builds and runs the bounded acceptance pipeline, writes JSON/Markdown summaries, and keeps physical audio off. A successful local package test does not establish clean-Windows deployment, sustained device stability, Pi support or a stable SDK. Hardware audio remains an explicit user action.
