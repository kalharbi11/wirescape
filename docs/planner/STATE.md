<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state (the leads read this first)

Keep this under 60 lines. History lives in git.

**Updated:** 2026-11-07 11:30 UTC by Keel. **Pi first. Phase 2 runs beside Test Session 1. Any owner failure in #271 preempts all work.**
Keel runs an owner-requested check-in every 3 h (scheduled task in Keel's session). Codex Lead runs every 150 min until 2026-11-09 06:00 UTC.

## Main blocker: pi5 runner offline
- The `pi5` self-hosted runner has been offline since about 2026-11-06 15:02 UTC. The Pi nightly job is still queued, and every `Pi PR poll` waits behind it in the `pi5-exclusive` group.
- **All `ci/pi` evidence is blocked** (#273, #274, #298, #301, #304, #297 strict). Owner step: restart the runner service on the Pi. Agents can't reach it.

## Keel pass 8 (2026-11-07)
- **#297 ruled and fixed:** **#320** (test-only). Bounded retry; each attempt keeps the full 3-block park deadline (the engine refuses longer). Strict single attempt with `--max-gap-ms` (pi5) or `WIRESCAPE_REPLAN_STRICT=1`. Closed #299. Needs a Codex/GPT T2 review.
- **#273**: re-review approved at `00a5b24`, all hosted green including Windows. Merge on pi5.
- **#274**: re-review approved at `c6ed064`. Merge on pi5. Unblocks #292 and #296.
- **#298**: T2 approved at `97dcd6a` (Lows only). Merge on pi5.
- **#301**: T2 at `43101f8`, two Mediums: emit `device-lost`→`reconnecting`→`running`, and don't kill a healthy stream on an enumeration miss. Rebase after #273.
- **#256 (1H)**: T2 approved at `7bbf398`; budget 13 files / 1,150 lines authorized; `allow-protected` added. Needs a main merge and a green web re-run, then Keel merges.
- **#290 / #210 ruled:** `last_note` (holds through release and choke, no bend) plus optional `min`/`max` row keys (config version bump, v1 still parses). **Learn moved to #287 (UI-owned, D3/D8)**; no module Learn.
- **#307 ruled:** add `Engine::refresh_latency()` (control thread, no allocation), called in replan before the ack and at most once per status tick. Reporting only. Open for any builder.
- Board: closed #277 and #259 (shipped in #282 and #263). #294 and #295 are unblocked (`status/todo`). #313 is superseded by this file.

## In flight (who acts)
- #300 keyboard default: gate-approved at `e4e0ae8`; check that it merged.
- #304 spectral S1-1: needs a T1 review from another session plus a Pi probe. #310 is stacked on it.
- #311 visible-when (#306): new-head Web plus an independent T1 review.
- Module lane (#319 merged): #314 delay, #315 quantizer (claimed), #316 reverb, #317 resonator, #318 blend (after #304).
- Web demo D14: #309 merged; follow-ups W-web-2 (#312) to W-web-5.
- Still blocked: #287 (after #273), #292 and #296 (after #274), #293 (after #301).

## Latent technical notes (Keel)
- `app_engine_replan` rebinds candidate ABI1 nodes to live metadata before `engine.replan` can fail. If a sidecar D5 fallback then uses that candidate, it would dangle. Safe today: the sidecar catalogue is ABI2 built-ins only. Fix before the sidecar loads ABI1 plugins.
- An immediate replan retry after a park timeout is refused ("already parking") until the next render retires the cancel. Any retry loop must wait one block.

## Phase 1 sign-off needs
#271 owner results (A, B, C), 1H green (#256), the contention benchmark (#274 plus owner D3). Then Compass runs Checkpoint A.

## Retained rulings
- Electron main is the only automatic restarter. Imported documents round-trip unchanged (D3). New loadGraph uses D5; live edits use replan with bounded busy retry.
- Benchmark fields are render walltime; late catch-up blocks count as overruns. Missing metrics never count as zero.
- Known non-required reds: cortex-m7 (#159); `static` is informational (but fix clang-format nits when touching the file).
- Tests and CI never open real audio, MIDI or LAN. Pi evidence comes only from the pi5 runner.

## Standing rules from the owner
- Don't plan agent assignment. Pi first; Windows (#266) after Test Session 1. Real-device tests are the owner's.
- No browser/kiosk product stage (D1). The web demo is a showcase build only (D14).
- Leads save usage: delegate mechanics, don't poll, ask the owner only for approvals and hardware checks.

Signed: Keel (Claude), 2026-11-07
