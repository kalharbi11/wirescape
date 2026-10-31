<!-- SPDX-License-Identifier: GPL-3.0-only -->
# J5: OSC In over UDP and Monitor log (#72, PR #82, stacked on J4 #81)

Spec: `docs/tasks/J_JAM_CHECKPOINT.md` § "B4 / J5"; reference `docs/specs/COMMUNICATION_MODULES_PLAN.md` §4.4, §4.7.

## P0 answers that apply here (planner, final)
- **IDs:** `osc.in` (config: `port` default 9000, `lan` toggle; outputs `row1`…`row8` signals + `messages` events), `monitor` (one event input `events`, signal inputs `in1`, `in2`).
- **Minimum subset before M5:** own bounded OSC 1.0 codec (i, f, s, b, T, F, N, I; arrays rejected safely; bundles applied immediately, time tags ignored); one UDP socket per port shared by all `osc.in` modules; network thread with 10 ms poll; **127.0.0.1 by default**, LAN only with the module's `lan` toggle **and** `--confirm-lan` (runner) or the app's first-time dialog. No OSC Out, UDP Send, Learn, Seen list or Monitor table mode.
- **Ingress:** same D13 SPSC rings and counters as J4; stale timeout per row (hold or zero).
- **Wildcard rows** (`/pico1/knob/*`) output a poly signal, one channel per matched index, sorted numerically.
- Fuzzing: a corpus test now; the libFuzzer job is #94 after this merges.

Planner authorization on #29 comment6536318466 permits2600changed lines/52files,
including both catalogue-count updates and changed-line formatting. The host regression tests refusal
of an unconfirmed LAN request using fake/offline routing; no LAN socket opens.

Draft Monitor port IDs are aligned to the approved `in1`/`in2` names above;
no existing module or port ID on main changes.

```acceptance
tier: T2
budget: 2 Codex sessions (remaining)
size: <= 2600 changed lines, <= 52 files
must_pass:
  - python tools/dev.py build --headless
  - ctest --test-dir build/native -R "osc|monitor_log|abi2_monitor_tests|app_engine_lifetime" --output-on-failure
  - ctest --test-dir build/native -R "^osc_host_tests$" --output-on-failure --no-tests=error
  - ci: linux-arm64, windows-x64, sanitizers (TSan on loopback tests), guard green on the exact head
  - pi: ctest -R "osc|monitor_log"
stops: [ABI edit, new dependency, non-localhost socket in a test, unspecified design choice]
```
