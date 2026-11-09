<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #471 — Engine-refused patch draft (Studio)

**Tier:** T1. **Owner:** issue #471. **Board:** #473 row 21.
**Dependency:** #463 merged on main (3fa0d3a). No engine or protocol edits.

## Read first
- `AGENTS.md`, `docs/WORKFLOW.md`, `apps/studio/AGENTS.md`
- `apps/studio/src/app/App.ts`, `apps/studio/src/engine/RealEngine.ts`, `apps/studio/src/engine/FakeEngine.ts`
- `apps/studio/tests/mock-bridge/bridge.ts`, `apps/studio/tests/mock-bridge/real-engine.spec.ts`
- #471 and latest Codex Lead handoff; coordinate App.ts with #478.

## Behavior
On a real engine `loadGraph` refusal with `unavailable` or `busy`, keep the previous running patch and its editable history on screen. Preserve the refused document, including unknown fields, as an accessible draft. The user can open and edit that draft without sending edits to the still-playing graph; show an explicit persistent **NOT RUNNING** label even after Save. Reapply submits the current draft to the real engine. On another refusal retain the draft and the playing graph; on success clear draft mode. Ordinary content refusals keep existing editable-document semantics.

No realtime, protocol, device, or patch-format changes. Do not replace `selectAudioDevice` or #463's graph-rollback semantics. Preserve #478's ingress validation when integrating main.

## Acceptance
```acceptance
tier: T1
size: <= 450 changed lines, <= 5 files
must_pass:
  - sh -c 'cd apps/studio && npx playwright test tests/mock-bridge/refused-draft.spec.ts'
  - sh -c 'cd apps/studio && npm test'
stops: [new dependency, engine/protocol edit, format change, test weakening, size budget exceeded]
```

Owner-only listening and device tests are not required for this UI-only mock-bridge change. Exact-head hosted CI and independent T1 COMMENT review remain required before merge.
