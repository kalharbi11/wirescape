<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Monitor (`monitor`)

One fan-in event input `events`, plus `signal1` and `signal2`. No outputs. Configuration
JSON is `{ "prefix": "Pico" }`; the host supplies the tap internally, never a saved
pointer. The app's log dialog provides pause/clear/counters; `play --monitor` drains
the same binary entries to stderr on the control thread.

The fixed SPSC tap holds1024 entries×128bytes. Formatting happens only in UI/control
code. MIDI keeps UMP words; notes keep a fixed summary; OSC keeps a payload prefix
with a truncation flag. Signals are sampled at up to30Hz per channel. Per-key logging
is limited to30 entries in each audio-frame one-second window, across128 active keys;
extra messages count as rate-limited and ring/key overflow increments dropped.
Pause does not stop counters. Clear advances only the consumer index. Audio reset
clears limiter history without rewinding shared ring indices while a reader is active.
No packets/devices, strings, locks or allocation are used by DSP callbacks.
