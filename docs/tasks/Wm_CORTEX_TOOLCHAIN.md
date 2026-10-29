<!-- SPDX-License-Identifier: GPL-3.0-only -->
# W-m: reliable Cortex-M7 toolchain (#66)

**Scope:** Replace the large, uncached apt install with an Arm-maintained
pinned GNU 14.2.rel1 cross-toolchain, SHA256-verified on cache miss.
Cache the extracted toolchain keyed by version and host, report cache hits,
compiler version, and both per-module compile durations. Preserve the
fail-red checks introduced by #56; no module exclusions or weaker flags.

**Read first:** `.github/workflows/portability.yml`, `docs/WORKFLOW.md` §§3–3b.
The protected workflow is authorized in the body of issue #66.
The second run must show `cache-hit: true` on the exact PR head.

```acceptance
tier: T0
budget: 2 ChatGPT CI iterations
size: <= 90 changed lines, <= 2 files
must_pass:
  - ci: cortex-m7 job green with all ABI2 modules in default and no-exceptions/no-rtti modes
  - ci: two consecutive green portability runs at exact head; second displays cache hit true
  - ci: toolchain version and per-module durations reported; linux-arm64 and guard green
stops: [new dependency, test removal, weakened compile checks, protected path authorization missing]
```
