<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Controller quick start

Wirescape's OSC In listens on UDP port 9000 by default and binds to `127.0.0.1` by default. Use OSC addresses such as `/pico1/knob/3` and send the knob value as a float from `0.0` to `1.0`. For Pico 2 W and ESP32 Wi-Fi controllers, send when the value changes by a small deadband (for example, `0.005`), limit each control to at most 200 messages per second, and send a 1 Hz heartbeat if the controller needs to show that it is still connected. A wildcard row such as `/pico1/knob/*` can collect numbered knobs into a poly signal.

For a laptop test, this short Python 3 script uses only the standard library and sends one OSC float. The destination defaults to localhost. A non-local destination requires an explicit `--confirm-lan` acknowledgement:

```python
import argparse, socket, struct

p = argparse.ArgumentParser()
p.add_argument("--host", default="127.0.0.1")
p.add_argument("--port", type=int, default=9000)
p.add_argument("--value", type=float, default=0.5)
p.add_argument("--confirm-lan", action="store_true")
a = p.parse_args()
if a.host not in ("127.0.0.1", "localhost") and not a.confirm_lan:
    p.error("LAN sending requires --confirm-lan")
def osc_string(s):
    b = s.encode() + b"\0"
    return b + b"\0" * (-len(b) % 4)
packet = osc_string("/pico1/knob/3") + osc_string(",f") + struct.pack(">f", a.value)
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.sendto(packet, (a.host, a.port))
```

For a Wirescape OSC receiver reachable over the LAN, enable the OSC In module's `lan` toggle **and** start the app or runner with `--confirm-lan`. Both are required; localhost remains the default. The headless runner's documented OSC hook is `wirescape play --osc-port <n>` when J5 lands. Do not send to a real LAN receiver unless you intend to test it.

For USB-MIDI, suitable DIY controller targets include a Raspberry Pi Pico with TinyUSB, an ESP32-S2 or ESP32-S3 with USB device support, and an ATmega32u4 board such as a Leonardo or Pro Micro. J4 covers MIDI In and MIDI-to-CV. The first real MIDI device access requires the runner's `--confirm-midi` flag; automated tests use a fake backend and do not open devices.
