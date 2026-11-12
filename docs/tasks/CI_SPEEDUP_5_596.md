<!-- SPDX-License-Identifier: GPL-3.0-only -->
# CI speedup 5: docs-only fast path (#596)

Owner: Keel. Full brief and ruling: issue #596; design and evidence: PR #617.

- A `changes` job classifies the PR merge-ref diff (`HEAD^1..HEAD`). Docs-only =
  every changed path ends in `.md` (anywhere). Other files under `docs/` count
  as code: `docs/specs/patch_v2_draft.example.json` feeds `patch_document_tests`.
  Push/dispatch runs, an empty diff or any classification error count as code.
- Docs-only PRs skip native builds/tests (required `linux-arm64`/`windows-x64`
  still report and run the full foundation Python suite) and both sanitizer
  lanes. The required `sanitizers` aggregate accepts skipped lanes only when
  `changes` succeeded with `code=false`; otherwise both lanes must be `success`.
- The acceptance ffmpeg/timeout and apt-retry fixes landed separately in #638.
- Evidence: docs-only green in 38 s (#622), mixed takes the full path (#623),
  broken classifier fails closed (#624); a
  fixture-only change under `docs/` must take the full path (probe 4).

```acceptance
tier: T2
size: <= 140 changed lines, <= 2 files
must_pass:
  - actionlint
  - ci: guard, linux-arm64, sanitizers green
stops: [test selection change on code PRs, required-check rename, skipping guard or acceptance]
```
