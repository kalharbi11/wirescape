<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Status and verification

October26,2026. Evidence refers to the original local workspace unless marked clean-copy below.
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
| ABI2 throughput | historical frozen acceptance snapshot | Mono graph~1.89× ABI1 time: ±5% target MISSED. Eight poly voices~0.91× eight ABI2 mono graphs in that sample. No hard realtime guarantee |

Engine-side peak and short-session counters do not prove physical output was captured or heard.
The unattended capture/alignment/analysis harness is a separate task in
[specs/AUDIO_CAPTURE_HARNESS.md](specs/AUDIO_CAPTURE_HARNESS.md).

## Known limits / remaining gates

- ABI2 is isolated/offline and awaits the M1-R planner review; the live app remains ABI1 mono.
  The slice (M1-1) was implemented before migration; follow-up fixes come from that review.
- No live MIDI/OSC/UDP services, sequencers, Monitor, MPE, arbitrary message editor or patch-v2 migration.
- No physical output recording, long-session dropout/reconnect matrix or listening-quality certification.
- No verified Pi/ARM64/Daisy/CLAP cross-build or hardware, or clean-Windows deployment result.
- No full sound-equivalent legacy migration. Inspector recognition/preservation is not playable import.
- New timing/overflow/latency semantics remain experimental; no SDK freeze or release certification.
- Allocation instrumentation covers C++ new families in static test code, not arbitrary vendor malloc.
- New raw poly render may exceed full scale; no limiter/normalization claim. Device/WAV boundary
  behavior in the current ABI1 app is unchanged.
- Dependency provisioning and public CI are pending M0-3. No SDK, fonts, backend DLLs, user files,
  reports or prior Git history are supplied by this cleaned source tree.

## Clean-copy migration acceptance

See [history/MIGRATION_REPORT_2026-10-26.md](history/MIGRATION_REPORT_2026-10-26.md)
for actual local doctor/build/test/startup results and the privacy audit. The migration does not
establish new audio/hardware/platform claims. Original reports stay in the local archive.
