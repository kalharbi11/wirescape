<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Status and verification

Updated October 27, 2026 (M1 complete except the Windows mono gap). Rows dated October 26 refer to the original local workspace. Rows marked **CI** or **Pi** come from this repository's GitHub Actions runs and Codex's Pi 5 runs, linked in the issues and PRs named.
Original build/report files were local ignored artifacts and are **not included in this repository**.
Historical review snapshots preserve summarized numbers; they are not clean-checkout certification.

| Scope | Recorded evidence | What it establishes |
|---|---|---|
| ABI1 six-module checkpoint | check-20261026T172345Z |27 Python tests,16 CTest suites, two UI sizes, normal startup, offline render,45 benchmark scenarios and package checks passed |
| Filter numerical review | history/SIX_ESSENTIALS_IMPORT_2026-10-26.md/.json | Quiet-tail, high-resonance and1/17-frame modulation checks at44.1/48/96k; no demonstrated blocker in that tested scope |
| App/document integration | native/ui/tests/controller_failure_tests.cpp | Module integration, rejected-input preservation, combined save/load and identical offline WAVs; example loads/renders |
| Original portable package | package-20261026T172345Z |1,421-file inventory and13 Essentials libraries; offline/normal startup; not a clean-machine result |
| Explicit real-device probe | audio-session-six-20261026.json, local only | Three-second MME session,48k/256/stereo,555 blocks; engine peak~0.00256, zero reported underruns/faults/overflows/late events/overloads; clean stop |
| Probe guard | audio-probe-no-confirm.log, local only | Missing confirmation exits2 before device/controller construction |
| ABI2 foundation plus ABI1 regression | check-20261026T230043Z |27 Python and28 CTest suites, UI/startup/offline render/benchmark/package passed; no device stream |
| Five SDK ports | reviews/ABI2_ACCEPTANCE_SNAPSHOT.json | Default mono max absolute error0 vs ABI1 at3 rates × blocks1/17/256;135 rate/block/channel configurations finite, deterministic and reset-reproducible |
| ABI1 shim / poly proof | reviews/M1_FOUNDATION_SLICE_REVIEW.md | Raw steady/scripted Six Essentials graph bit-identical; offline three→eight-note synth with stealing/releases, stereo, zero counted render allocations and pitch error<0.001 cent |
| Concurrency/shutdown | historical ABI2 acceptance logs, local only |100k events over10 simulated seconds; actual producer/render concurrency; Debug CRT heap integrity/balance; not ASan or device jitter proof |
| ABI2 throughput (before M1-3) | historical frozen acceptance snapshot | Mono graph~1.89× ABI1 time: ±5% target MISSED. Superseded by the M1-3 rows below |
| **CI** on every PR | `linux-arm64`, `windows-x64`, `guard`, `sanitizers` (all required) | Release build and all CTest/Python suites on Linux ARM64 and Windows MSVC; protected-path, test-retention and binary guard; ASan+UBSan and TSan on the ABI2 suites; callgrind Ir per block |
| **CI/Pi** ABI2 throughput after M1-3 | `docs/reviews/M1-3_REVIEW.md`, PR #22 | Mono ABI2/ABI1: 0.865× linux-arm64 runner, **1.097× windows-x64 runner (target ≤1.05×, open: #32)**, 0.860× by callgrind Ir, 0.711× on the Pi 5. Poly8/eight-mono ≤0.974× everywhere. Modulated parameter-pipeline cost halved. Equivalence, shim and poly WAV hash unchanged |
| **Pi** soak | PR #31 comment (`main` at `6cc87f9`, before M1-3) | 10-minute paced, device-free soaks at 48k, blocks 64/128/256, poly8 and the Six Essentials shim graph: zero render overruns, allocations and faults; four scheduler deadline misses reported separately. A 16-voice poly graph uses 12% of a 128-frame period at p99. One 4 ms outlier in a 10 s budget run. Re-run on post-M1-3 `main` pending (#42) |
| **CI** Pi TSan | PR #22 comment | ABI2 panic/ingress/egress/render stress passes TSan on the Pi 5 (LLVM 24 runtime; GCC 14's runtime doesn't support the Pi's 47-bit VMA) |
| **CI** capture harness | PR #13 | Offline analysis and 31 self-tests with a fake backend; Windows helper compiles. **Never run on a real device yet** (#43) |

Engine-side peak and short-session counters do not prove physical output was captured or heard.
The unattended capture/alignment/analysis harness is a separate task in
[specs/AUDIO_CAPTURE_HARNESS.md](specs/AUDIO_CAPTURE_HARNESS.md).

## Known limits / remaining gates

- ABI2 is tested offline only. Studio, the CLI and the renderer still use the ABI1 mono engine until M2-6.
  All 13 Essentials are on ABI2 in `main` (M2-2 #39 and M2-3 #40 merged 2026-10-28); the M2-4 regressions are #41. The app still runs the ABI 1 engine until J1 (#29).
- Windows MSVC mono throughput misses the 1.05× target (1.097× in the M1-3 review, 1.147× in a later CI run); #32.
- No live MIDI/OSC/UDP services, sequencers, Monitor, MPE, arbitrary message editor or patch-v2 migration.
- No physical output recording, long-session dropout/reconnect matrix or listening-quality certification.
  No real-device capture run yet (#43) and no live musical session on the new engine (#45, after M2-6).
- ARM64 builds and tests pass in CI and on a Pi 5. No Daisy/CLAP cross-build yet (probe: #44), and no clean-Windows deployment result.
- No full sound-equivalent legacy migration. Inspector recognition/preservation is not playable import.
- New timing/overflow/latency semantics remain experimental; no SDK freeze or release certification.
- Allocation instrumentation covers C++ new families in static test code, not arbitrary vendor malloc.
- New raw poly render may exceed full scale; no limiter/normalization claim. Device/WAV boundary
  behavior in the current ABI1 app is unchanged.
- No SDK, fonts, backend DLLs, user files, reports or prior Git history are supplied by this cleaned source tree.

## Clean-copy migration acceptance

See [history/MIGRATION_REPORT_2026-10-26.md](history/MIGRATION_REPORT_2026-10-26.md)
for actual local doctor/build/test/startup results and the privacy audit. The migration does not
establish new audio/hardware/platform claims. Original reports stay in the local archive.
