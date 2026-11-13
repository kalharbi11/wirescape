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
with the Arduino IDE and your board's WiFi-enabled core (no extra libraries).
The sketch targets ESP32-S3; Pico W requires the arduino-pico core.
**Firmware is not yet compiled or tested on ESP32 hardware.** Its OSC encoder
was round-tripped against `native/io/osc/codec.h` in a host-side check only.

On an ESP32-S3 breadboard, wire each potentiometer's outer legs to **3V3**
and GND, and its wiper to GPIO **4**, **5**, or **6**. Wire a button from
GPIO **15** to GND (internal pull-up). **Never connect 5V to an ADC pin.**
Set `WIFI_SSID`, `WIFI_PASSWORD`, `WIRESCAPE_IP`, and `WIRESCAPE_PORT`
(default **9000**) in the sketch. On the Pi, find its IP with `hostname -I`.
Put the board and Pi on the same trusted network.

The sketch sends `/pico1/knob/N` with **argument 0** as a float 0…1 and
**argument 1** as the raw ADC integer, plus `/pico1/button/N` for buttons.
To calibrate raw readings, use **Argument index 1** in the OSC In row's
Advanced editor, then **Set max / Set min** from #742 (or enter Raw min/max).

Launch on Windows: `tools\windows\Wirescape.cmd --confirm-lan`.
Launch on Pi: `python tools/dev.py launch --confirm-audio --confirm-midi --confirm-lan`.
`--confirm-lan` is owner-only network consent; by itself it does **not**
enable audio or MIDI. In OSC In **Edit…**, tick **LAN**, then cable
`osc.in.messages` → `monitor.events` and turn a knob to inspect traffic.

If the card says **LAN not confirmed**, check both launch flag and LAN tick.
On Pi, `ss -ulpn | grep 9000` should show `0.0.0.0:9000` while listening.
Check `hostname -I`, matching port/IP, and that guest WiFi is not isolating
devices. If UFW is enabled, use `sudo ufw allow 9000/udp` (owner action).
On Windows allow Wirescape on Windows Firewall's **Private** network only.
