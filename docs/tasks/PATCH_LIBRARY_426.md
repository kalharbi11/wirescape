<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #426 Patch library publishing and designer fast lane

Owner explicitly authorizes patch-only designers to merge without lead approval.
Implement build-time folder packaging, existing starter/browser integration,
migrate public cinematic entries to editable JSON, validate/import helper and guide.
No live GitHub credentials, DSP/ABI/protocol or new dependency. Existing curated
and native fixtures remain regression snapshots. Folder commits trigger the existing
apps/studio/** web-demo paths; no workflow edits needed.
Later patch-only issues may reference this standing task without editing it.

```acceptance
tier: T1
size: <= 5000 changed lines, <= 24 files
must_pass:
  - python tools/patch_library.py check apps/studio/patches --shared
  - python -m unittest discover -s tools/foundation/tests -p test_patch_library.py
  - node --test apps/studio/tests-node/patch-library.test.mjs
  - bash -lc 'cd apps/studio && npm ci && npm run build && WIRESCAPE_CHROMIUM="$(command -v chromium || command -v chromium-browser || command -v google-chrome)" npx playwright test tests/patch-browser.spec.ts tests/curated.spec.ts tests/codec-roundtrip.spec.cjs'
  - ci: guard, linux-arm64, windows-x64, sanitizers, Web and Web demo green on exact head
stops: [DSP or ABI change, external credentials, new dependency, threshold weakening]
```

Infrastructure implementation requires ordinary independent T1 review.
Subsequent JSON/patch-README-only changes use the owner's explicit self-review/
self-merge exception in docs/PATCH_DESIGNER_GUIDE.md. It never bypasses checks.

