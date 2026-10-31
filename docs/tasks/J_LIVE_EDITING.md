<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Live graph editing: park, edit, resume (#186, part 2)

Implement the planner decisions on #186 comments6537668954 and6539666147.
Transport focus is delivered separately by #201/#203. Start from merged main.

Read first: engine2 graph preparation/ingress/render/reconfigure, the internal
AppEngine facade, Studio commitTopology and its existing rollback tests.

- Build the replacement layout and every fallible allocation before parking.
- Keep the live Impl, producer registry, rings, future queue and surviving Node
  objects/DSP/ramp/pending state. Preserve unchanged feedback delays.
- A single atomic state word arbitrates request/acknowledgement/cancellation.
  Render fades out within one block, acknowledges, and returns silence while
  parked without reading graph state or draining ingress. Control waits no more
  than three blocks; a winning cancellation leaves the original graph intact.
- Commit only prepared moves/swaps while parked, without resetting processors
  or incrementing their generation. Resume fades in within one block.
- Producer enqueue reads only immutable registration/event/payload information;
  validate node/port/parameter targets during render drain and count rejections.
  Existing immediate-invalid tests may instead require counted rejection, at
  unchanged strictness. Removed targets count stale; surviving releases deliver.
- Deliver NOTE_CHOKE to each removed event edge's former destination at resume.
- Preserve explicit audio/MIDI consent, accepted-value/rollback, stable IDs and
  serialization. Invalid live candidates leave the accepted graph unchanged.

Review is T2 plus Claude, as the planner explicitly requires for engine threading.
Record the measured fake-audio gap; Pi target is <=20ms at256frames. Never use
device teardown as the live-edit mechanism. Physical live-patching remains owner-only.

Standing-v2 file allowance:12→15 (+25%) covers the thread-local allocation
instrumentation, actual ABI1 facade lifetime regression and Qt live-control
coverage; existing allocation Scope semantics and every prior assertion remain.
Standing-v2 line allowance:900→1200 (+33.4%) covers changed-range clang-format
and explicit state/lifetime regressions. No protected paths or extra feature scope.

```acceptance
tier: T2
size: <= 1200 changed lines, <= 15 files
must_pass:
  - ctest --test-dir build/native -R "^abi2_live_replan_tests$" --output-on-failure --no-tests=error
  - ci: windows-x64, linux-arm64, sanitizers including threaded live-replan regression
  - ci: Studio controller/UI live-edit and rollback regressions with devices disabled
  - pi-run: build/native/abi2_live_replan_tests --max-gap-ms 20
  - user: live patching on Windows
stops: [ABI header edit, new dependency, ID or patch-format change, removed tests, unapproved threshold change]
```
