# Claude ↔ Codex project communication

Claude is the top-level project reviewer/manager. Codex implements bounded agreed slices and resolves routine issues locally. The user manually asks Claude to review checkpoints; this file does not send messages or authorize another chat. Keep entries concise, evidence-based, and chronological. Don't overwrite reviewer notes; resolve them in a new entry. Latest user instructions take precedence.

## Entry 001 — ABI2 foundation start (October26,2026, Codex)

**Request:** implement the pasted isolated ABI2 foundation slice, then hard-stop with docs/FOUNDATION_SLICE_REVIEW.md. TrackC, all other module ports, app integration and patch-format migration are out of this slice. TrackA audio-capture remains independent and is not started by this task.

**Preservation:** protected ABI1 headers/engine/modules/UI byte hashes are saved in ABI2_BASELINE_MANIFEST.json. Claude's added plans and START_HERE edit are preserved. Minimal root CMake wiring will be necessary for new CTest targets; protected app logic stays untouched. New SDK/header/voice code is original MIT; GPL port code remains GPL.

**Decisions adopted from user's pasted Claude note:** MIT SDK/header/voice with GPL app/modules; own SIMD wrapper, no external SIMD dependency; feedback32samples; future network loopback-only default with per-module LAN toggle; hard stop at foundation review.

**Implementation clarification for review:** finite event capacities cannot promise delivery of an unbounded release flood. Reserve release capacity; retain release intent through a bounded emergency CHOKE/panic/reset path and visible counters if the release lane itself fills. Never silently discard a release and leave voices active. Document exact limits/tests rather than claiming mathematical impossibilities. Timestamp scheduling tests prove fake-clock mapping under a defined lookahead, not jitter-free real drivers.

**Status:** in progress; no approval requested now. Final entry will link actual tests, deviations and commit for Claude review.


## Entry002 — foundation ready for review (October26,2026, Codex)

**Status:** hard stop. Implementation `34b948e1d1404e6108ea3ceeec3ee951959e1c4a`; [review packet](FOUNDATION_SLICE_REVIEW.md). Full app/package check green,12 ABI2 suites and Debug CRT pass,92 protected source hashes unchanged. Every default mono port max error0; legacy graph steady/scripted bit-identical; offline eight-voice synth passes. Read exact evidence/limits in the packet.

**Review blockers:** mono mean=1.890× ABI1 (±5% target missed); approve finite-release emergency behavior, max-block timestamp lookahead, explicit-channel modulation policy and latency path semantics before Track C. Further vectorization/LTO/profiling and mixed event-cycle adversarial tests remain. No app migration, audio devices, external I/O, adapters, new modules or automatic continuation.

**To Claude:** please return prioritized source-evidenced findings and bounded next tasks here or through the user. Do not overwrite these historical entries. No reply sent to another chat.
