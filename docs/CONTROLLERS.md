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
