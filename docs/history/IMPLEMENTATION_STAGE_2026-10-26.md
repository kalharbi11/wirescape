# Hardening stage: implemented and tested, Windows policy gate open

October 26, 2026. The user authorized continued staged implementation and model failover. This note is a recovery point, not a public release declaration.

## Accepted implementation

- N01: app-supported audio-off readiness report, captured startup logs, self-closing normal-window probe; automated UI checks at 1400×900 and 1000×650.
- N02: quiescent parameter initialization applies settings without queued performance traffic or advancing audio time. Tests cover 4096 settings, queue/state rejection, and exact 256/257-event and 64/65-span boundaries. Overload reason and stopped-session counters are exportable.
- N03/N04: destructive save continuations are consumed on failure; Retry saves only. Injected persistence and known-module preparation failures preserve accepted work. Physical canvas keys are tracked and reconciled on focus/modal/stop transitions. Real Qt control interaction tests remain enabled.
- N05: shared 5 ms sample-count ramp for VCO Frequency, VCA Gain, mixer Level A/B and ADSR Sustain. Initial settings/reset snap; repeat targets do not postpone completion; note/gate and envelope stage timing remain explicit. Rate, retarget, reset, partition and processor checks pass.
- N06/N07: bounded owned process-tree cleanup, stable-source checks, actual CMake-resolved SDK/flags and artifact hashes, flat/multiconfig inventories, current-versus-history summaries. No claim of a hermetic compiler environment.
- N08/N09: stereo format support check before opening a stream, stopped PortAudio refresh and enumeration errors, JSON diagnostics, compact minimum-window layout, explicit short-render limitations, portable inventory script. No new hardware session was opened by an agent.
- Automation: `python tools/dev.py check --package` produces JSON plus a short Markdown stage/log summary, stopping dependent stages after a failure. `startup-test`, two-size `ui-test`, and the separate read-only package inventory are reusable commands.

## Focus investigation closed without hiding the failure

The difficult remaining test was caused by a synthetic non-native file chooser retaining global window focus after programmatic acceptance. The failure popup was an item in the main window; its type and key target were correct. A hidden `QQuickPopupWindow` titled Save patch still owned process focus.

An official Qt 6.10 documentation check and a bounded High review led to one discriminating owner-window experiment. The final test restores **only window activation** after the synthetic chooser closes, confirms `QGuiApplication::focusWindow()` equals the actual owner, and retains both popup active-focus and real Escape-dismissal assertions. No popup-specific focus force or skipped assertion was introduced. The speculative production `callLater` workaround was removed.

Evidence: `modal-owner-offscreen.txt` / `modal-owner-windows.txt` demonstrate the hidden chooser, and `modal-activation-offscreen.txt` / `modal-activation-windows.txt` pass the focused original regression. These files are under `build_reports/foundation/`.

Qt's documented distinction between requested focus, active focus and the popup's rendering window was relevant: [Popup, Qt 6.10](https://doc.qt.io/qt-6.10/qml-qtquick-controls-popup.html), [Dialog, Qt 6.10](https://doc.qt.io/qt-6.10/qml-qtquick-controls-dialog.html). Test window activation is a harness precondition; ordinary interactive native-file-picker behavior is not claimed from synthetic acceptance alone.

## Actual verification and current external block

- `test-20261026T082644Z.json`: **all five CTest suites passed**, including the full Qt regression suite, engine/module tests, device mocks and controller commit/preparation failure tests.
- Focused current Python helper run: **19 tests passed** after the package inventory work. More tests may be added in later stages; do not report this as the final count forever.
- `benchmark-20261026T075153Z.json`: 45 steady/event-traffic rate/block scenarios, 256 blocks each, no offline render deadline misses observed in this bounded run. This is not device latency, sustained stability or CHAOS capacity evidence.
- Earlier local two-size/normal-startup/portable checks passed. The most recent full automatic run, `check-20261026T082644Z.json`, is intentionally **not green**: build and all tests passed, then Windows refused to create the rebuilt app for the UI smoke.
- The exact error is **WinError 4551, Application Control policy blocked this file**. Read-only Windows Code Integrity events 3033/3077 at 04:26:44 America/New_York identify `build/native/Release/wirescape_studio.exe` and signing-policy ID `{0283ac0f-fff1-49ae-ada1-8a933130cad6}`. This is a real operating-system signing-policy gate, not a QML test failure or a model quota limit.
- Do not disable the policy, install trust roots, rename/copy the blocked executable to evade it, or use another executable as a proxy to run the blocked GUI. A policy-compliant signing/environment decision belongs to the device owner. Do not repeatedly relaunch unchanged blocked bytes.
- The old staged development package `build/packages/wirescape-release-20261026T075153Z` has **1405 expected/actual files with no inventory drift**. It is an older accepted staging artifact, not proof that the newly rebuilt app can launch under the current policy.
- A five-second software/offscreen idle probe accumulated no process CPU time at the measurement's available resolution. That does **not** establish zero CPU usage or justify a polling rewrite. No idle optimization was made from it.

## Further independent preparation

F01 completed: six synthetic legacy-document fixtures plus source provenance in `docs/LEGACY_FORMAT_FIXTURES.md`. Current v3 producer and older reader/test evidence are distinguished; no historical v1/v2 serializer was invented. Fixtures contain no user presets or Surge data and do not establish completed migrations.

F03 prepared: `tools/foundation/package_inventory.py` checks hash/path drift without launching binaries, and `docs/PORTABLE_DEPENDENCY_INVENTORY.md` records evidenced dependency versions/notices and clean-machine/Pi limits. Unknown versions remain unknown. No license-compliance determination is implied.

Read `docs/NEXT_TASK_QUEUE.md` before selecting later work. Independent pure-source/reference experiments may proceed while the signing-policy gate is open; a public/native deployment acceptance claim may not.

## Recovery and scheduling

- Current work is uncommitted; preserve all existing edits and personal data. No physical audio stream is opened by automated tests. User's earlier successful sound/meter report remains the hardware smoke evidence.
- One future continuation in this chat is scheduled for **October 26, 2026, 10:20 AM America/New_York**, automation ID `one-time-wirescape-continuation`. Keep the computer awake and the app running. It cannot bypass account-wide limits or the signing policy.
- Workers used bounded ownership: Luna Medium for tools/fixtures/inventory, Sol Medium for UI work. Focused High reviews were used only after evidenced dialog failures; they did not restart architecture decisions. No Ultra/Max worker was used.
- Latest observed shared account usage before the later continuation work was 65% primary/300 minutes and 10% secondary/10080 minutes. These are historical shared-window readings, not task-specific token totals or a promise of remaining capacity.
- Resume by reading this note, BUILD_CHECKPOINT and NEXT_TASK_QUEUE; inspect active workers and the current diff to avoid duplicate work. Do not repeat the already-solved modal experiments or repeatedly retry a model-specific limit.
