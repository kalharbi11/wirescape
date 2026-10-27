---
name: Task
about: A bounded unit of work for an agent or contributor
labels: status/todo
---

**Spec file:** `docs/tasks/<file>.md` (if there is one, it is authoritative; this issue summarizes it)
**Milestone:** M?   **Agent:** agent/codex | agent/copilot | agent/claude   **Size:** size/s | m | l

## Goal
One or two sentences.

## Read first
- AGENTS.md
- …

## Boundaries (don't touch)
- …

## Deliverables
- …

## Acceptance tests
- …

## Stop condition / review package
- Draft PR with `Closes #<this issue>`, CI green, a handoff comment, then label `status/review`.
