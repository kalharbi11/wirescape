<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #483 Pi: one accepted main SHA for the owner's first Electron-on-Pi session

Parent #436. Done on the existing #453 branch (`task/436-pi-readiness-guide`);
no new PR. `main` at `20aabc0` contains #463, #450 and #452.

1. Merge current `main` into the #453 branch normally (no rebase, no force-push).
2. #453 body: `Closes #483`, keep `Refs #436`; labels `ci/pi` and `ci/pi-accept`.
3. When `pi5/accept` passes both `pi-run` lines on #453's exact head H, confirm
   `git diff --stat <main parent of H> H` touches only `docs/**`. Then set
   `AcceptedSha` in `docs/PI_FRESH_INSTALL.md` to that full main-parent SHA and
   replace the "no accepted SHA yet" status text with the run link. A docs-only
   follow-up push keeps the evidence (Keel ruling on #483).
4. Never pin a SHA whose Pi run is queued, old, failed or partly skipped.

The owner session after this: `docs/PI_FRESH_INSTALL.md` sections 1–6, then the
Pi part of #271. Real sound, MIDI and the display are owner-only.

Signed: Compass (Claude), 2026-11-09

```acceptance
tier: T0
size: <= 450 changed lines, <= 4 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p test_doc_links.py
  - pi-run: python3 tools/pi/preflight.py --check-only
  - pi-run: python3 tools/pi/app_smoke.py --report build_reports/pi-app-smoke
  - ci: guard, linux-arm64, windows-x64 and sanitizers green on the exact head
stops: [non-doc change on the #453 branch, pinning an unverified SHA, device or SSH access, workflow edit]
```
