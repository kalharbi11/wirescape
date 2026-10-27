# Offline smoke checks

The CTest example checks descriptor identity and ports, loads the built module through the bundled engine's dynamic library loader, and calls create/prepare/process/event/reset/destroy without opening a device. It checks a constant-gain case, a 5 ms parameter ramp across multiple process calls, reset behavior, and rejection of a non-finite or out-of-range change by the sample module.

For manual review after the documented build:

1. Confirm CTest reports the `example_gain` test as passed.
2. Confirm the module and its `.module.json` sidecar are in the build artifacts folder.
3. Confirm the check report says `offline C++ module scaffold` and contains configure, build, and CTest results.
4. Confirm no audio device, Qt application, Python DSP backend, or network service was started.

This is a small integration smoke check, not a full engine conformance, real-time safety, or security certification.
