<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pi: dev.py launch --confirm-lan + ESP32 OSC controller sketch (#746)

**Owner fix for v1.0 (Keel, 2026-11-13).** Read first: `tools/dev.py` (`command_launch`, `launch` parser), `apps/desktop/main.ts:25-36`, `tools/foundation/tests/test_launch_freshness.py`, `docs/CONTROLLERS.md`, `docs/PI_RUNNER.md` (grep "launch").

## 1. `--confirm-lan` on `dev.py launch`
- Add `--confirm-lan` (store_true) next to `--confirm-audio/--confirm-midi`, include it in `consent`, and record it in `device_confirmations`. Without any consent flag, keep exactly today's `--null-audio` behaviour.
- `--confirm-lan` alone must **not** remove `--null-audio`: LAN consent is not audio consent. Build the command as audio/midi consent (or `--null-audio` when neither) plus `--confirm-lan` when given.
- Help text: "allow OSC In rows with LAN ticked to listen on the network (owner only)".
- Never pass it in CI, tests or agent runs (AGENTS.md "Devices off"). Tests check only the command that would be built; they never launch.

## 2. Controller sketch
Bring in `controllers/WirescapeOSC/WirescapeOSC.ino` unchanged from commit `e984c96` on branch `claude/busy-pasteur-ywx96r` (`git checkout e984c96 -- controllers/WirescapeOSC`). It sends `/pico1/knob/N <float 0..1> <int raw>` and `/pico1/button/N` over UDP and needs no libraries. Its encoder was round-tripped against `native/io/osc/codec.h` on the host, but it has not been compiled for ESP32: say so, and don't claim otherwise.

## 3. Docs
`docs/CONTROLLERS.md`, new section "ESP32 / Pico W board (WiFi)":
- the sketch path; breadboard (pot wiper → GPIO 4/5/6, button GPIO 15 → GND, 3V3 only);
- WiFi/IP fields; args 0 and 1 (Argument 1 + Set max/min from #742);
- launch: Windows `tools\windows\Wirescape.cmd --confirm-lan`, Pi `python tools/dev.py launch --confirm-audio --confirm-midi --confirm-lan`;
- tick LAN in OSC In; Monitor check;
- troubleshooting: "LAN not confirmed", `ss -ulpn | grep 9000` shows `0.0.0.0:9000`, Pi IP via `hostname -I`, same network (guest WiFi blocks device-to-device), `ufw allow 9000/udp` if a firewall is on, Windows Firewall "Private".

Mention the flag in `docs/PI_RUNNER.md` only if it lists launch flags. Patch guide impact: none, since no module, port or range changes (say so in the self-review).

## Tests
`tools/foundation/tests/test_dev_launch_consent.py` (new; unittest; mock out the Electron/engine lookup, freshness check and subprocess, so nothing runs):
1. no flags → command has `--null-audio`, no `--confirm-lan`;
2. `--confirm-lan` only → `--null-audio` + `--confirm-lan`;
3. all three → `--confirm-audio --confirm-midi --confirm-lan`, no `--null-audio`;
4. the record's `device_confirmations` lists exactly the flags given.

```acceptance
tier: T1
budget: 1 Codex session
size: <= 400 changed lines, <= 6 files
must_pass:
  - python -m unittest discover -s tools/foundation/tests -p "test_dev_launch_consent.py" -v
  - python -m unittest discover -s tools/foundation/tests -p "test_launch_freshness.py" -v
  - ci: guard green
stops: [engine/sidecar or Electron change, default LAN behaviour change, any test or CI step passing --confirm-lan, new dependency]
```

**Owner check (manual, Pi):** flash the sketch, launch with `--confirm-lan`, tick LAN, patch messages → Monitor, and see `/pico1/knob/1` move.
