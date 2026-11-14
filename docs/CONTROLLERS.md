<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC controller setup

Use the **native desktop** Wirescape app; the browser demo cannot receive UDP. Add `osc.in` and cable its `row1`…`row8` CV outputs to the target inputs. To inspect messages, cable `osc.in.messages` → `monitor.events`.

## Point the controller at Wirescape

Configure **OSC 1.0 over UDP**, host `127.0.0.1`, port `9000` (or your OSC In editor's port), and send float values from 0 to 1. For a Pico/TouchOSC on another device, enter the computer's LAN IP; LAN ingress requires **both** `lan:true` in OSC In and a separately confirmed native launch with `--confirm-lan`. The toggle alone cannot authorize a LAN socket. Use only a trusted local network.

## Pico defaults

| Address | Output | Address | Output |
| --- | --- | --- | --- |
| `/pico1/knob/1` | `row1` | `/pico1/knob/5` | `row5` |
| `/pico1/knob/2` | `row2` | `/pico1/knob/6` | `row6` |
| `/pico1/knob/3` | `row3` | `/pico1/knob/7` | `row7` |
| `/pico1/knob/4` | `row4` | `/pico1/knob/8` | `row8` |

**Edit…** sets port, address, argument index, range and CV/gate/trigger mode. **Reset to Pico defaults** enables all eight original rows. **Clear** disables all eight but retains their addresses; with no active rows, no socket binds. Disabled-only messages never reach Monitor; unmatched addresses do.

## Verify and limits

The OSC card reports actual bind state, received count and active rows. **Port busy**, **LAN not confirmed** and **All rows off · not listening** describe different states, not guaranteed remote reception. The Monitor displays at most 32 recent messages (address and up to four numeric arguments) when directly wired; Pause/Clear change the view only.

Only native OSC In is supported: no OSC Out, browser UDP or automatic controller discovery. Without explicit owner LAN consent, the listener stays on localhost. Omitted `config.version` means 1; unknown future versions are refused by older engines.

## ESP32 / Pico W board (WiFi)

Flash [`controllers/WirescapeOSC/WirescapeOSC.ino`](../controllers/WirescapeOSC/WirescapeOSC.ino)
with the Arduino IDE and board-core WiFi/WiFiUdp (no extra libraries).
ESP32-S3 is the main target; Pico W needs arduino-pico. **Not compiled on ESP32 yet**;
only the OSC encoder was host-round-tripped against `native/io/osc/codec.h`.

**Wiring:** Pot ends to 3V3/GND, wipers to GPIO 4/5/6. Button GPIO 15 to GND (internal pull-up). **Never connect 5V to an ESP32 ADC pin.**
**Network:** Set `WIFI_SSID`, `WIFI_PASSWORD`, `WIRESCAPE_IP`, `WIRESCAPE_PORT`
(default 9000) in the sketch. Get the Pi IP with `hostname -I`; use trusted WiFi.
**Messages:** `/pico1/knob/N` sends float 0…1 (argument 0) and raw ADC integer
(argument 1); `/pico1/button/N` sends press/release. Select **Argument index 1**
under Advanced and **Set max / Set min** (#742) to scale raw readings.
**Launch:** Windows `tools\windows\Wirescape.cmd --confirm-lan`; Pi `python tools/dev.py launch --confirm-audio --confirm-midi --confirm-lan`.
LAN consent alone does **not** enable audio/MIDI; null audio remains the default.
**Verify:** Tick LAN in OSC In Edit…; cable `osc.in.messages` → `monitor.events`. Turn a control and inspect both arguments in Monitor.
**Troubleshooting:** "LAN not confirmed" needs both tick and launch flag. On Pi,
`ss -ulpn | grep 9000` should show `0.0.0.0:9000`; check `hostname -I`,
port and WiFi (guest networks may isolate devices). If UFW is enabled, the owner
may run `sudo ufw allow 9000/udp`; use Windows Firewall **Private** on Windows.
