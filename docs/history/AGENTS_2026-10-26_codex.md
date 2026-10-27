# Wirescape working agreement

## Quota-efficient execution

- Read `START_HERE_CURRENT_BUILD.md` and `docs/CURRENT_BUILD.md`, `docs/CURRENT_EVIDENCE.md`, `docs/CURRENT_PLAN.md` for the current consolidated handoff. The older foundation plan retains rationale; the user's latest steering takes precedence.
- Coordinate and implement with the user's selected Sol Medium setting. Do not automatically use Ultra or Max.
- Default to no subagent. When delegation materially helps, use at most one focused worker; a second requires genuinely independent work.
- Use Luna Medium for mechanical work and Sol Medium for substantial implementation. Escalate to Sol High only for difficult failures, concurrency/timing risks, or unresolved architectural questions.
- Before delegation state file ownership, acceptance checks, and required concise output. Avoid full-history/context copies for bounded work.
- Limit a difficult fix to two reasonable attempts, then leave evidence and a focused question for a later higher-effort review instead of repeatedly retrying.
- Use `python tools/dev.py` for deterministic builds, checks, benchmarks and concise reports. Run focused checks while developing, broad validation at milestone boundaries.
- Preserve settled decisions. Avoid repeated research/reviews and unrelated changes.
- At a checkpoint record changed behavior, actual tests, limitations, model assignments, and the next task in `docs/BUILD_CHECKPOINT.md`. Do not invent token/quota totals.

## Foundation safeguards

- Legacy source is preserved by Git tag `baseline/0.5.2-local`; native work lives under `native/`.
- Preserve `user_data/` and `modules/chaos/surgepy_bin/`. Private snapshots are in ignored `local_backups/`.
- Standalone Qt Quick UI; native headless engine independent of Qt/Python; basic Essentials before CHAOS.
- Audio stays off until the user starts it. Offline render and GUI smoke tests must not open a device stream.
- Treat the native ABI as experimental. Do not claim Pi support, live stability, polyphony, or full legacy migration until verified.
