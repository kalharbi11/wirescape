<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Groove foundation provenance and review

Original portable C++17 module. No third-party source imported.
Westlicht Performer is a behavioral inspiration only, not a dependency.

Proposed v1 JSON structure is a T2 config/format contract requiring Keel
review (#430). The existing quiescent runtime_module_config adapter serializes
to a transient bounded host structure; only JSON is persisted. No ABI/SDK
header or host protocol changes. Fixed-capacity render-owned state has no
allocations, locks, JSON or I/O in process/reset. S0 is an eight-step proof
that S1 can extend to 32 steps per track. Transport semantics and live
editing require separately reviewed later phases.
