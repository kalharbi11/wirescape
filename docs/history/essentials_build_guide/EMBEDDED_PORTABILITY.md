# Embedded portability — reusable DSP, target-owned integration

User requirement, October 26, 2026: a person should be able to save a Wirescape patch and hand its module sources/data to another agent, which builds a Daisy Seed or other hardware project primarily by integration rather than rewriting algorithms. Wirescape itself need not generate/build/flash target firmware. No Daisy-specific implementation or verified support is claimed by this note.

## Preserve this boundary

A module has a platform-neutral DSP implementation, a thin Wirescape descriptor/lifecycle adapter, and optional separately owned UI/provider integration. Prefer a small C++17 DSP class/header/source with explicit initialization, state, parameters/events and process spans; no Qt, Python, OS handles, filesystem, sockets, device APIs or desktop host dependency inside it. Modules may retain the current ABI wrapper; DO NOT change ABI headers or current DSP results to implement this policy.

For new external modules, put reusable DSP in a source/header such as dsp.h when practical, with module.cpp as the thin wrapper. An already-small self-contained module.cpp can be reused through the existing callbacks or entry-point renaming; splitting it is not mandatory churn. The objective is unchanged processing code under a different adapter, not compulsory rewrites or a universal hardware framework.

- Explicit sample rate, actual frame count (including short/uneven spans), mono/channel layouts, units, smoothing, gate/event ordering and reset. No implicit desktop time, block512 requirement or mutable global state.
- Default audio/control buffers float32; justify double precision where useful. No x86-only intrinsics or platform-specific code without a portable equivalent. Expensive math and oversampling need target measurements, not automatic approximation/quality loss.
- No allocation/locks/exceptions/I/O during rendering/events/reset. Prefer caller-owned fixed DSP state and explicitly supplied large buffers; boot-time wrapper allocation is separate from processing. Record stack, state, scratch/delay/sample memory needs and lifetime. Avoid huge automatic arrays and hidden per-instance heap requirements.
- Processing should compile without RTTI/exceptions when practical. Desktop tests may use exceptions; the reusable DSP need not. Hardware memory placement/allocator and SDK headers belong in a target adapter, not generic DSP.
- Report external source/runtime dependencies and licenses. Python/Numba providers, desktop GUI, generic OS services and a desktop synthesizer runtime are not automatically MCU-compatible because a function pointer/C ABI exists.
- Keep accurate offline reference tests and fixed-seed/state reset tests. Cross-compilation/target performance tests are later proof steps. A desktop CTest pass does not imply firmware fit, hardware behavior or sufficient callback headroom.

## Current gaps, honestly

Current native/src/engine.cpp directly uses Windows LoadLibrary or POSIX dlopen, filesystem/vector/string setup, and static_assert for lock-free uint64 atomics targeting 64-bit Windows/Linux ARM64. This is a desktop host, not a bare-metal MCU runtime. Current module create callbacks typically allocate small state at setup. Many current DSP paths use double math. None of these justify claiming Daisy support or guarantee that an arbitrary patch fits unchanged.

A hardware integrator can statically compile selected processor sources and give their entry points unique names (the existing WS_MODULE_ENTRY_NAME pattern), or instantiate extracted DSP classes. It can use a fixed execution order/buffer plan rather than port desktop discovery, JSON parsing, UI and event services into firmware. Per-board glue handles audio callback, ADC/pots/buttons/CV calibration, MIDI/network hardware, clocking, memory placement and persistence. Do not silently substitute a different algorithm for a missing resource/service.

## Patch handoff needed for integration

A saved editable patch alone is not yet a reproducible hardware build bundle. A manual handoff should include the patch, exact module/dependency versions or source hashes, parameter/port/unit definitions and values, graph ordering and feedback/latency rules, reset/time/event semantics, target sample/block configuration, voice count and state/scratch requirements. Separately supply hardware control/I/O mappings and any preset/sample assets with provenance. Unsupported UI/provider nodes must be explicitly mapped or reported; never silently drop them.

Target project assembly: inspect patch -> verify supported modules/resources -> precompute routing/order and allocate buffers -> compile unchanged portable DSP -> add board I/O/control adapters -> compare offline output/reference vectors -> measure target deadlines, stack/memory and actual hardware behavior. Wirescape does not need an automatic exporter for this process.

## Daisy Seed example, not a universal profile

The classic STM32H750-based Seed uses a 480MHz Cortex-M7 and float audio callbacks. The MCU has limited internal memory; Seed memory variants differ (the 65MB option adds 64MB SDRAM, the 1MB option does not). Large buffers need explicit placement/initialization and flash/link layout depends on the target toolchain. C++ support does not imply desktop shared-library loading or identical standard-library/atomic/runtime behavior.

Compute/memory, polyphony, oversampling, large instruments, reverbs/samplers and analysis are patch-level resource questions. MIDI/network/log/UI services require available peripherals and adapter code. No fixed module-count limit can be inferred from a PC benchmark. Use the target SDK's actual callback sizes/rates and measure worst-case execution with headroom, not average desktop timing.

Primary references checked October 26, 2026:
- https://github.com/electro-smith/libDaisy/blob/master/doc/md/_a3_Getting-Started-Audio.md
- https://www.st.com/en/microcontrollers-microprocessors/stm32h750vb.html
- https://daisy.audio/pages/memory-what-is-the-difference
- https://docs.daisy.audio/libDaisy/sdram_8h/

Keep desktop capability unrestricted: modules may explicitly declare desktop-only dependencies rather than pretending every module/patch is an embedded candidate. The portable DSP subset should be easy to identify and reuse. Do not rebuild current processors or introduce a platform abstraction framework merely to satisfy a hypothetical board.
