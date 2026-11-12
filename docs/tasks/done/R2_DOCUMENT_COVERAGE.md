<!-- SPDX-License-Identifier: GPL-3.0-only -->
# #465: Qt-free document fixture coverage and soak

R2 implements D16. No Qt source/test or legacy fixture is removed. The original
six fixtures are read in place by Studio and `abi2_runtime_patch_tests`.

## Contract reconciliation (Codex Lead ruling on #465, 2026-11-09)

The fixtures named `patch_v1/v2/v3` are **Python** `wirescape.patch` documents
with string IDs, not `wirescape.native.patch` v1/v2. They were inspection-only
in Qt: `patch_document_tests` and `app_patch_tests` refuse all six for execution.
Do not invent an executable migration, fake classifications, or a second C++
document owner to make a coverage count look equivalent. Studio owns native-v1
migration; the runtime owns a disposable execution copy. Legacy/visual/future
imports must refuse before replacing the current document or rebinding Save.

An existing ingress bug was found: Studio cast JSON to PatchV2 and could turn a
Python/future document into an empty native graph, and silently drop malformed
native modules/connections or overwrite duplicate module IDs. The pre-mutation envelope/shape guard
fixes it. Electron also validates native envelopes, duplicate keys and read size
and readable module/cable shapes before binding dialog/library Open paths, and
revalidates existing Save targets. Native-v1 requires its migration arrays; native-v2
may omit them. Known UI containers must be readable objects; missing v2 module arrays are handled
by the app loader. Unknown module types, dangling endpoints and opaque fields remain
editable; these ingress checks do not validate graph execution. Exact original
v1 bytes are backed up once using atomic no-clobber publication; the v2 document
is still written to an exclusive temporary file and atomically renamed.

## Coverage table and R3 deletion gate

Assertion families below enumerate the old Qt contracts; these are not claims
that Qt's implementation-specific APIs survive. Independent review must inspect
this table before R3 may use this packet as its deletion prerequisite.

| Old Qt assertions (`document_reader_tests` / `patch_document_tests`) | Product assertion or explicit retirement |
|---|---|
| Six fixture classifications, recognizedStructure, sequence/module_state inventory, conflicting states, explicit state null | Qt-only inspection categories retired by D16; all six original files refuse native import/execution, with source bytes, current document and catalogue unchanged (`legacy-documents.spec.cjs`, runtime fixture loop). No sequence/state translation is invented. |
| Exact original bytes, opaque object, diagnostics, staging copy isolation | Studio fixtures assert original source object/file bytes unchanged, current JSON identical and clean after refusal; runtime asserts source bytes/catalogue unchanged and nonempty diagnostic. |
| Native v1 classification without descriptor loading | Studio accepts/migrates actual native v1; unknown module stays inactive and preserved; runtime execution separately refuses unavailable modules. |
| Wrong format/version, unsupported native/visual future versions, malformed roots/JSON/trailing/empty/oversize | Studio pre-mutation envelope/shape tests plus real UI unchanged-document/title regressions; real Electron unsupported-open/overwrite cases; native runtime negative/root/size tests. JSON lexical errors remain JSON.parse/runtime parser failures. |
| Native/legacy duplicate IDs, bad IDs/elements/endpoints, dangling cable source/destination/output | Studio/real Electron reject malformed native IDs, duplicate module IDs and unreadable module/cable/UI shapes before mutation/binding. Existing runtime ID/cable validation plus added dangling-destination/output tests reject executable graph errors; editable unknown/dangling data round-trips intact. Legacy files never reach either graph. |
| Manifest map, duplicate catalog IDs/numeric mappings, fractional mapping | Qt-only ModuleCatalog migration-map API retires; Studio's generated catalogue and existing `ui_catalogue_sync`/codec export checks remain authoritative. No descriptor loading or new manifest API is added. |
| Finalized native example loads | Existing `ui_patch_conformance`, runtime demo load and all native-example Studio round trips. |
| v1 stable parameter IDs, original version/bytes, complete module/top envelope collisions, unknown vendor fields, no invented DSP state | New native-v1 Studio regression asserts mapped frequency, whole preserved source/module including collisions, vendor payload and absent state; existing native-example tests cover all v1 fixtures; runtime v1/v2 render equality is retained. |
| Unknown v1/v2 module/cable round trips, inactive validation | Existing editable round-trip tests plus new unknown-v1 regression; runtime inactive/unknown refusal remains separate. Source preservation never implies the engine can run it. |
| Structural settings/config/state/endpoint/cable collisions and escaped keys/sibling independence | Existing `codec-roundtrip.spec.cjs` unknown native fields/absent metadata/endpoint/output tests; new v1 collision regression. Runtime uses strict duplicate-key decoding; Electron rejects escaped/nested duplicates before binding or overwrite. |
| Duplicate raw/escaped/nested keys | Runtime root/escaped/nested rejection; actual Electron file I/O regression includes escaped and nested duplicates. |
| Invalid settings, numeric parameters, channel/feedback/output limits | Existing runtime validation tests retain channel/sample-rate/config/parameter checks and added dangling output; executable validation remains engine-owned. |
| Atomic v2 save, serialized value round trip | Existing real-sidecar Save/quit/reopen byte test; Electron retains exclusive temporary write + rename; new real-I/O v1 regression asserts exact saved bytes and no leftover temporary files. |
| Exact first v1 backup, no staging residue, no overwrite of existing backup | New real Electron v1 Save/Save As regression: original whitespace bytes match backup, existing backup marker survives, no temp files remain. Publication uses hard-link no-clobber after staging. |
| Invalid/future/malformed/duplicate target cannot be overwritten | New real Electron I/O regression tests all six fixtures plus malformed native shapes/UI/future/escaped/nested duplicates/oversize; target bytes remain exact and rejected Open cannot redirect subsequent plain Save. |
| Save As backs up original v1; v2 reread creates no v1 backup | Same real Electron v1 regression proves exact Save As backup and no backup after opening v2. |
| Failed-load save refusal | Unsupported Open leaves prior save binding unchanged; renderer document refusal leaves current bytes unchanged. IPC independently rejects unsupported outgoing envelopes. |

The soak now links `wirescape_runtime_document` unconditionally. Bounded std::ifstream
input replaces QFile; graph ownership, rendering, allocation/fault assertions,
finite/positive peak checks, pacing and timing thresholds are unchanged. A fresh
Qt-disabled configure must expose and pass `app_document_soak`.

```acceptance
tier: T2
size: <= 750 changed lines, <= 12 files
must_pass:
  - python tools/dev.py build --headless
  - python tools/dev.py test
  - ctest --test-dir build/native --output-on-failure --no-tests=error -R '^(abi2_runtime_patch_tests|app_document_soak)$'
  - bash -lc 'cd apps/studio && npm ci && npx playwright install ffmpeg && browser=$(command -v chromium || command -v chromium-browser || command -v google-chrome) && test -x "$browser" && WIRESCAPE_CHROMIUM="$browser" npm test'
  - bash -lc 'cd apps/desktop && npm ci && npm run build && xvfb-run -a npm test'
  - ci: guard
  - ci: linux-arm64
  - ci: windows-x64
  - ci: sanitizers
  - pi-run: ctest --test-dir build/native --output-on-failure --no-tests=error -R '^(abi2_runtime_patch_tests|app_document_soak)$'
stops: [Qt source/test removal, fixture edit, unknown-field loss, threshold change]
```

Signed: Codex (R2 document coverage builder), 2026-11-09 UTC.
