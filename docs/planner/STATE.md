<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Planner state
**Updated:** 2026-11-12, Codex Lead followup (America/New_York, Eastern time).
Board#473 BODY is live; named agents retain branches. Current state uses actual heads.
Only completed source-matched checks/runs count; queued, skipped and stale are distinct.
## Priority and authority
- Owner order: OSC → Quad VCA/LPG → ASIO, then broad Qt retirement#661.
- T1 needs another session; T2 needs the other family. Keel owns ABI/spec/CI.
- No weakened tests, force/main pushes, devices, LAN or SSH. Owner alone auditions.
## Completed software
- OSC row-enable#645 and compatibility#659 (e82868a) are merged.
- Quad native#611 (62fa9ec) and Studio/patch#675 (0cc86f76) merged;
  parent#490/#602 closed after actual acceptance/required CI and production renders.
- Groove#619 merged91ffa3bd: native/Studio acceptance and required gates complete.
- OSC fuzz#674 merged76b8a416: actual Clang18 build and60s/target plus OSC
  CTests passed(run),99/250lines5/6files; exact guard/Linux/sanitizers/Static.
- ASIO#631 source28b25b7 mergedf0f14bda after Keel Claude approval,
  Codex Claude-retry delta and unchanged spec disposition. Actual nativeaccept627
  passed(run),543/600lines8/10files, all required/Static/Web green. #627/#610 closed.
- #670/#673/#677/#678 and setup/visual#605/#608/#607/#617/#647/#654 merged.
## Final OSC priority
- gpt-builder1 owns#672/#684 sourcef0a1879. Linux/sanitizers/Static/Web and
  actual selectedaccept672 pass; Windows fails1/111 at native document fixture.
- Keel/reviewer found missing version+parameters on both fixture modules;
  add valid fields per existing contract, retain every save/reload assertion.
  Fresh exact Windows/acceptance/gates then independent T1 approval/merge;
  close parent#72 only after actual merge. No parallel implementation.
## D5 and runner
- #691 narrow generatedmoc cppcheck fix source7ea1627 MERGEDc6fa02c.
  Codex cross-family review5876075609/approval and15unit tests pass;
  required Windows/Linux/sanitizers/Static green. Authored errors stay gated.
- gpt-builder2 #487/#648 source99f842b: prior required/nativeacceptance green,
  old Static generatedmoc red. Integrate#691 normally, rerun fresh exact gates,
  link completed source-matched private device-off Pi protocol/recovery proof,
  then Claude approval. No duplicated poll daemon, timeout or threshold changes.
- #296 stays blocked: no accepted/executed3600s evidence supplied publicly.
  Keel rules on pending runner integration; gpt-builder2 continues existing work.
## Assigned supporting queue
- claude-builder1 #676 is unblocked by#674; non-required fuzz CI/Pi nightly
  per Keel6823484751, Codex T2 then Keel CI-policy approval. No Pi claim yet.
- claude-builder2 #686/#690 aee6c55: senior Medium5876089384 proves escaped
  quotes cause strip_comment to truncate a quoted hash; repair and test.
- claude-builder2 #687/#692 a84bf37: known-ID tests, final shared Qt fixture
  waits#684/#648; gpt-builder3 reviews. Preserve existing writers.
- gpt-builder3 #685/#693 de621f8: integrate updated220/8task PartB from#689,
  prove applyable bounded format patch; Keel allow-protected and Claude T2.
## Lead decisions and release
- #689 board closed-status cleanup merged0131d8d; labels/roster updated.
- Keel#6791974969 remains blocked by durable postmerge-wakeup High.
  #681 raw Markdown closing-directive Medium and generated UTC header remain.
- Compass reconciles stale PLAN Now/ownership with completed priorities.
- #593 brief now follows accepted Keel6816704664: candidateversions1.0.0
  after OSC closeout → owner#581 tests mergedcandidateSHA → tag exactly thatSHA.
- Physical ASIO clicks/sound#597, Pi#271/#274,3600s#296 and release are distinct.
- Broad Qt#482→#468/archive→#481/#469 and ABI1 stay deferred behind priorities.
Signed: Codex Lead, 2026-11-12
