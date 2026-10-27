# Wirescape communication modules plan: MIDI, OSC, UDP, Monitor

**Status:** approved direction from the user, October 26, 2026. This is Track C, scheduled in `docs/PLAN.md` (milestones M4–M5). It **depends on `docs/specs/ABI2_PLAN.md` phase B3**: event ports and outputs, message events, the ingress registry, egress, transport, state and config-dependent ports. Don't build it on ABI 1, and don't put strings or packets into float buffers. This file replaces the design details of the MIDI, OSC, UDP and Monitor rows in `docs/ROADMAP.md`, while keeping their acceptance intent. Update that table when C1 finishes.

`AGENTS.md` rules apply:
- No real devices or audio unless an explicit flag is given and the user authorizes it.
- One task per issue, branch and PR.
- Two attempts, then leave evidence.
- A handoff comment per session.

## 1. What the user wants (requirements)

1. A Pd-style set of small, composable modules: MIDI In/Out/Monitor, OSC Input/Output, UDP Receive/Send, Monitor/Print.
2. **Routing CCs is easy and always editable.** There's no automatic or chronological channel or port assignment that the user can't see. Defaults can be suggested once, but every routing is visible and changeable.
3. **MIDI learn** ("listen") for CCs, notes and parameters.
4. About 4 MIDI inputs to start. Possibly many more OSC/UDP sources from Raspberry Pi Pico boards. The user may run a Python mapping layer before Wirescape.
5. Future: DMX, Arduino/serial and other systems, **without rebuilding the core.**

## 2. Architecture: three layers

```
 devices / sockets           host services (non-realtime, Qt-free C++)            engine (realtime)
 ─────────────────           ─────────────────────────────────────────            ─────────────────
 MIDI ports  ──callback──▶  MIDI service ─┐                                      ┌─▶ MIDI In module ─▶ MIDI→CV / CC Map ─▶ patch
 UDP sockets ──recv──────▶  Net service  ─┼─▶ Endpoint registry + Activity bus ──┤   OSC In / UDP Receive modules
 (later) serial, DMX     ─▶ ...          ─┘        │ (ingress SPSC rings, D13)   └─▶ Monitor taps
                                                   ▼
                              Learn + Mappings (host feature, UI)
 MIDI out / UDP send ◀──── I/O dispatcher ◀──────────── egress SPSC ring ◀────── MIDI Out / OSC Out / UDP Send modules
```

**1. Host services** (`native/io/`, no Qt, C++20 allowed):
- Open devices and sockets, parse, timestamp, and push into ingress rings.
- Drain egress and send.
- Own reconnects and counters.
- Each service sits behind an interface with a **fake backend** for tests.

**2. Modules** (ABI 2 SDK, C++17, portable):
- Hold only routing, scaling and conversion logic.
- They never touch devices. They name an **endpoint** in their config, and the host binds them.
- The same module code works in a plugin (the DAW supplies MIDI) or on Daisy (UART/USB MIDI supplies events).

**3. Host features:**
- **Endpoint registry:** a stable name for each device or port.
- **Activity panel:** everything incoming, even when nothing is patched.
- **Learn.**
- **Mappings:** direct "this control → that parameter" without a cable.

**One module or many?** Devices and sockets are opened **once** by a service. **Any number of modules may bind to the same endpoint.** The recommended setup:
- One **MIDI In** per device (or one set to "All enabled devices"), fanning out to as many MIDI→CV and CC Map modules as you like.
- One **OSC In** per UDP port. Several Picos can share a port with address prefixes (`/pico1/…`, `/pico2/…`). That's one firewall rule, and addresses separate the boards.

For about 4 MIDI devices, either way is easy. Nothing is assigned automatically.

## 3. Shared foundations (build once, C1)

### 3.1 Endpoint registry
- Each endpoint has an `id` (UUID), a `kind` (midi_in, midi_out, udp_port, udp_dest), a **user label** (for example "Pico Keys"), a **system name**, and a duplicate index for identical devices.
- Patches reference endpoints **by label, then system name**, never by enumeration order. A missing endpoint leaves the module **editable and offline** (a status badge), as with missing modules today. It never blocks loading.
- **Enable list** in Settings: the user picks which MIDI ports are opened. Ports aren't opened just because they exist.
- **Device listing runs on a worker with a timeout.** Some Windows drivers can hang enumeration (see Windows MIDI Services known issues). The UI never blocks.

### 3.2 Activity bus and Learn
- Every service publishes a **bounded summary** of recent traffic to the UI:
  - per (endpoint, channel, type, number/address): the last value, rate in Hz, and last-seen time.
  - Polled at 30 Hz. Holds up to 2,048 keys, evicting the oldest.
- **Learn on a row:** the row enters "listening". The **first new message** of a compatible type from any enabled endpoint fills that row's endpoint, channel, type and number/address. The UI shows it as "Pico Keys · ch 2 · CC 74", and every field stays editable. Esc cancels.
- **Learn never changes any other row** and never auto-assigns channels.
- **Learn from the list:** the Activity panel and each module's "Seen" drop-down list recent sources. Clicking one creates a row. This is the main path for OSC from Picos.
- **Global "Map" mode:** click any parameter on any module, then move a hardware control or send an OSC address. This creates a Mapping (3.3).

### 3.3 Mappings (host-level, saved in the patch)
- A row is: source (endpoint + channel + CC/NRPN/pitch-bend/aftertouch/note, or OSC address + argument) → target (node + parameter ID).
- Options:
  - input range, output range, curve
  - **takeover mode:** Jump, Pickup (soft takeover: waits until the control crosses the current value) or Scale
  - **relative encoders:** two's complement, binary offset or sign-magnitude
  - apply as **base value** (default, behaves like turning the knob) or as **modulation** (D7)
- There's a Mappings panel listing every mapping in the patch, sortable and editable, with enable/disable per row.
- Mappings run in the host → ingress `PARAM` events (D8), coalesced latest-wins per block.

### 3.4 Timing
- Services stamp input with a monotonic nanosecond clock. Use the API's own timestamp when it's better: Windows MIDI Services provides sub-microsecond timestamps.
- Mapping to frames follows ABI 2 D13: Immediate or Timestamped (default) per patch.
- MIDI Out and OSC Out events carry frame time → wall time. MIDI Out can use Windows MIDI Services scheduled sends where available. Otherwise it sends when dequeued, and jitter is reported.

### 3.5 Bounds, counters and safety (all services)
- Fixed ring sizes, set at start: ingress 4,096 entries per producer, egress 4,096, message arena per block (D8).
- **Every drop or truncation increments a visible counter:** ring_full, oversize, malformed, late, unknown_endpoint, send_fail.
- **Note safety:**
  - Endpoint disconnect or close → the service injects releases for every active note from that source, so MIDI→CV releases those voices.
  - MIDI Out on stop, panic, patch replace or close → note-offs for tracked notes, then CC 123 (All Notes Off) on every channel used.
  - Note-offs are never dropped (D8 reserve).
- **Shutdown order:** stop acquisition → join service threads → drain or discard ingress → stop render → flush egress releases → close devices and sockets → destroy. Same principle as `docs/specs/PROVIDER_BRIDGE_CONTRACT.md`.
- **Networking defaults:**
  - Receive sockets bind to `127.0.0.1` unless the user picks "All interfaces (LAN)". Show that Windows Firewall will ask the first time.
  - Packet size cap: default 2,048 bytes, configurable up to 65,507 bytes.
  - No sockets open until a module that needs one is in the patch and enabled.

### 3.6 Libraries (decide and record in C1)
- **MIDI: libremidi** (C++20, cross-platform). On Windows it supports WinMM, WinUWP and **Windows MIDI Services**, and it supports MIDI 2.0 UMP. Read its LICENSE.md and record the licence.
  - Fallback if it causes problems: RtMidi (WinMM backend).
  - Wrap it behind `native/io/midi/midi_backend.h`, so the engine and modules never see it.
- **Windows MIDI Services** (rolling out in retail Windows 11 24H2/25H2/26H1 during 2026):
  - **Multi-client:** Wirescape and Ableton can share a port. Under old WinMM most drivers are single-client, so this is a real UX win.
  - Built-in **loopback endpoints** for app-to-app MIDI and for our explicit loopback probe.
  - Accurate timestamps and scheduled output.
  - Known rollout issues: virtual-port tools such as loopMIDI weren't always visible (a fix began rolling out April 30, 2026), and some Korg drivers conflict. Report these as environment issues, not app bugs.
- **OSC: write our own bounded OSC 1.0 codec** (with OSC 1.1 types T/F/N/I and arrays), about 400 lines with fuzz tests.
  - Reason: exact control over bounds, no allocation on receive (parse into a preallocated buffer), and no LGPL dependency.
  - The spec is short and normative (see Sources).
- **UDP sockets:** a thin Qt-free Winsock2/BSD wrapper with nonblocking sockets and one network thread using `WSAPoll`/`poll` (10 ms timeout).
  - Switch to standalone Asio only if the wrapper grows beyond about 300 lines.
  - Keeping Qt out of services lets a headless or Pi build reuse them.

## 4. Module set (final recommendation)

The user's list, refined. **MIDI Monitor becomes one general Monitor with a MIDI decoder**, plus the always-available Activity panel. A dedicated **MIDI CC Map** and **MIDI→CV** are added, so routing stays explicit and Pd-like.

| Module | Inputs | Outputs | Notes |
|---|---|---|---|
| MIDI In | — | `midi` events | Endpoint (one device or "All enabled") and channel filter (Omni or 1–16, explicit). Pure source |
| MIDI → CV | `midi` events | poly `pitch`, `gate`, `velocity`, `aftertouch`, `pressure`, `bend`, `mod`, plus `notes` events | Uses the ABI 2 voice allocator. Mode, voice count, channel filter, bend range and retrigger are all explicit |
| MIDI CC Map | `midi` events | one signal per row (config-dependent ports, D12) | Table of rows, each with its own Learn |
| MIDI Out | `notes`/`midi` events, poly pitch/gate/velocity, CV rows | — | Endpoint, channel per row, rate and deadband |
| MIDI Clock | `midi` events | transport (sync) / `midi` clock events | In: follow external clock. Out: send clock and start/stop from the transport |
| OSC In | — | one output per row (signal, gate, trigger or message), plus `message` events | UDP port, address rows, Seen list |
| OSC Out | signals/events per row | — | Destination host:port, address rows, send policy |
| UDP Receive | — | `message` events, decoded numeric outputs per field, `packet` trigger | Raw, text or struct decoder |
| UDP Send | signals/events per field | — | Text template or struct encoder |
| Monitor (Print) | events (fan-in), 4 signal inputs | — | Log and Table modes, MIDI/OSC/UDP decoders, name prefix like Pd `print` |

### 4.1 MIDI CC Map rows (the "easy CC routing" piece)
Each row has:
- **Source:** endpoint, or "Any from input". The upstream MIDI In already filtered it.
- **Channel:** Omni or 1–16.
- **Message type:** CC (7-bit), CC 14-bit (MSB 0–31 paired with LSB +32), NRPN, RPN, pitch bend, channel pressure, poly aftertouch (note), program change, or note-as-gate/velocity.
- Number, input range and output range.
- **Output hint:** cv_uni, cv_bi, gate or trigger.
- Curve.
- **Smoothing:** default 10 ms for continuous types, 0 for stepped.
- Relative encoder mode.
- Name: becomes the output port label.

Rows can be added, removed, reordered and duplicated. Learn works per row. Learn detects 14-bit pairs automatically when an LSB follows its MSB, and the user can turn that off. Ports are keyed by a row ID, so reordering never breaks cables.

### 4.2 MIDI → CV
- **Allocator modes** (from ABI 2 D2, all explicit):
  - Rotate, Reuse, Reset
  - **MPE** (lower/upper zone; per-channel pitch bend, pressure and CC74 become per-voice `bend`/`pressure`/`timbre`)
  - Mono with last/low/high priority and legato on/off
- Voice count 1–16. Pitch bend range in semitones (separate for MPE). Sustain pedal (CC 64) behaviour: hold, or ignore.
- `pitch` follows D4 (MIDI note 60 → 0.0).
- Pitch bend is applied into `pitch` (option) and also output raw as `bend`.

### 4.3 MIDI Out
- **Notes path:** `notes` events in → note on/off with the incoming `note_id` tracked. Or poly pitch/gate/velocity in → notes (gate edges, pitch quantized to the nearest note, with an optional per-note pitch-bend mode).
- **CC rows** mirror CC Map: input → channel/CC/14-bit/NRPN. Send policy: on change with deadband (default 1 step), max rate (default 200 Hz per row), or on trigger.
- Clock out lives in the MIDI Clock module.

### 4.4 OSC In
- **Config:**
  - UDP port, and bind address (localhost default, LAN optional).
  - Optional source IP filter.
  - Late policy for bundles with future time tags (schedule, the default; or apply now). Immediate time tag = arrival time.
- **Each row has:**
  - Address pattern: glob with `*` `?` `[]` `{}`, matched against incoming literal addresses.
  - Argument index.
  - Type: auto, float, int, bool, string or blob.
  - **Output kind:**
    - `cv`: scaled from the input range to the output range, with curve and smoothing.
    - `gate`: T/F, or a number above 0.5.
    - `trigger`: fires on every matching message, including type-less or Impulse messages.
    - `message`: the full message as an event.
  - Optional **"learn range"**: watches min/max for a few seconds.
  - **Timeout:** if nothing arrives for N ms → a stale badge, and gate goes low if chosen.
- **Wildcard to poly:** a row such as `/pico1/knob/*` can output a **poly signal**, with one channel per matched index (sorted numerically). One cable then carries all 8 knobs.
- **Seen list:** recent addresses with their type tags, rate and last values. Click one to create a row.
- **Bounds:**
  - Packet ≤ cap; address ≤ 256 bytes; ≤ 32 arguments; bundle nesting ≤ 4.
  - A string or blob longer than the arena slot → truncated, with a flag and a counter.
  - Numeric rows coalesce latest-wins per block. Triggers count up to 64 per row per block, and the rest are counted as dropped.

### 4.5 OSC Out
- **Destination:** host:port. The hostname is resolved on the network thread and cached, with re-resolve on failure.
- **Each row has:** address, a source input (signal or event), a type tag (`f`, `i`, `T/F`, `s`) and a send policy:
  - on change ≥ deadband (default)
  - max rate in Hz (default 60)
  - on trigger
  - continuous at a rate
- Option to **bundle all due rows per tick** with an immediate time tag.
- Counters: sent, send_fail, rate_limited.

### 4.6 UDP Receive and Send
- **Receive:** port, bind address, source filter, size cap. Decoders:
  - **Raw:** message event only (shown as hex and text in Monitor).
  - **Text:** UTF-8; delimiter whitespace, comma or newline; numeric fields → outputs N1..Nk; non-numeric → message.
  - **Struct:** a format string like Python's `struct`, for example `<HffB` (endianness `<`/`>`; types `b B h H i I f d ?`). One output per field. A size mismatch increments a counter and outputs nothing.
- **Send:** destination; a **text template** (`"{0:.3f},{1}\n"`) or a **struct** format; one input per field; send policies as in OSC Out.
- The decoder code lives in `native/io/codecs/` so a **Serial** service (Arduino) can reuse it later (C7).

### 4.7 Monitor (Print)
- **Inputs:** one event input (fan-in from any number of sources) and 4 signal inputs, sampled at display rate (poly-aware).
- **Log mode:** a Pd `print`-style scrolling list. Each line has:
  - timestamp (ms since start or wall clock, toggle; the audio frame is shown on hover)
  - source
  - decoded content (MIDI as "ch 2 · CC 74 · 0.58 (74)", OSC as address + typed arguments, UDP as text/hex)
- **Table mode:** one row per key (CC, address or signal) with last value, a sparkline, rate and count. **Better for high-rate Pico data** than a scrolling log.
- **Controls:**
  - pause (counting continues; display freezes)
  - clear
  - filter (text or regular expression, applied in the UI)
  - copy selection or all, as TSV
  - a name prefix
  - **per-key rate limit** (default 30 lines/s; extra lines fold into "×N")
- **Realtime side:**
  - Each Monitor tap gets a fixed SPSC ring of 1,024 fixed-size 128-byte entries. Payloads are truncated with a flag.
  - Visible drop counter.
  - The UI keeps at most 10,000 lines in its own ring.
- Formatting happens only in the UI. Never format strings on the audio thread.

## 5. Portability of these modules

| Piece | Wirescape desktop | CLAP/VST3 plugin | Daisy |
|---|---|---|---|
| MIDI In | MIDI service → events | The DAW's MIDI input events | libDaisy UART/USB MIDI handler → events |
| MIDI→CV, CC Map, MIDI Out logic | Same module code | Same | Same |
| MIDI Out sink | Service → device | Plugin output events | UART/USB MIDI out |
| OSC / UDP | Network service | Optional, off by default | Only on boards with networking. Otherwise reported as unsupported in the export bundle |
| Monitor | UI | Plugin GUI (optional) | Not exported (debug only) |

## 6. Phases (Track C, after ABI 2 B3)

Each phase lists its deliverables, then the **acceptance** criteria.

**C1: shared foundations.**
- Deliverables:
  - Endpoint registry, enable list and labels.
  - Activity bus.
  - Fake MIDI and fake network backends.
  - Monitor module in both modes.
  - Counters.
  - Shutdown order.
  - Mapping storage in patch v2.
- **Acceptance:**
  - With fake backends: ordering, bounds, drop counters and Monitor rate limiting.
  - Shutdown under traffic with no leaks or use-after-free.
  - Patch round-trip of endpoints and mappings, including a missing endpoint staying offline but editable.
  - No real devices or sockets in `check`.

**C2: MIDI input.**
- Deliverables:
  - libremidi backend.
  - MIDI In and MIDI→CV.
  - Voice allocator integration.
  - Learn and the Mappings panel for MIDI (with takeover modes).
  - Disconnect releases.
- **Acceptance:**
  - Fake-backend tests for every allocator mode and MPE.
  - 14-bit pairing.
  - Pickup takeover.
  - Disconnect releases all voices.
  - 2,000 messages/s for 30 s with zero audio-thread allocations and no stuck notes.
  - **Explicit-only** probe `wirescape_midi_probe --confirm-midi` using a Windows MIDI Services loopback endpoint (never in `check`), writing a JSON report like the audio session probe.

**C3: CC Map, MIDI Out, MIDI Clock.**
- **Acceptance:**
  - Row add/remove/reorder keeps cables.
  - Relative encoder modes.
  - Out deadband and rate.
  - Stop/panic/close sends tracked note-offs and CC 123.
  - Clock in follows tempo changes in a fake-clock test.
  - Clock out is stable within ±1 sample of the transport in offline render.

**C4: UDP Receive and Send, text and struct codecs.**
- **Acceptance:**
  - Codec unit tests and a malformed-input corpus.
  - Automated **127.0.0.1 loopback** test in CTest. It's allowed because it binds localhost only, with no audio and no firewall prompt.
  - Size cap and truncation counters.

**C5: OSC codec, OSC In and Out, OSC learn and mappings.**
- **Acceptance:**
  - Round-trip of all types, including bundles and time tags.
  - The pattern matcher against the spec examples.
  - Fuzz corpus (truncated, misaligned, bad type tags, oversized blob counts, nesting too deep, overlong address). Run it in CTest; libFuzzer is optional on Clang.
  - Wildcard → poly.
  - Stale timeout.
  - 5,000 messages/s localhost stress for 30 s with zero audio-thread allocations.

**C6: hardening and documentation.**
- Deliverables:
  - Reconnect matrix with the fake backend.
  - A long offline session.
  - Build-guide pages for writing new endpoint services.
  - Update `docs/ROADMAP.md` and `docs/STATUS.md`.
- **Acceptance:** `check --package` green, CI green, and a handoff comment. No claim about real-device stability beyond what the probes actually ran.

**C7 (future, not scheduled):**
- **Serial In/Out** for Arduino: reuses the codecs, plus SLIP-framed OSC over serial.
- **DMX Out** over Art-Net or sACN: both are UDP. Universe and channel rows, about 44 Hz frame rate, a hold-last or blackout policy.
- **Python Script** module (`docs/specs/ABI2_PLAN.md`, section 7).

None of these need engine or ABI changes. They're new services plus ordinary modules.

## 7. Notes for the user's Pico and Python setup

- Your Python mapping layer before Wirescape is fine. To get the most out of Learn and the Seen list:
  - Send **OSC** with stable, descriptive addresses (`/pico1/knob/3`, `/pico1/button/2`).
  - Use **floats normalized to 0..1**.
  - Add a heartbeat such as `/pico1/alive 1` every second, so Wirescape can show online/stale status.
- Picos can also skip OSC and send packed binary over UDP. Then use UDP Receive with a struct format such as `<Bffff`. Later, the Python Script module can replace the external mapper if you want everything inside one patch.

## Sources

- Windows MIDI Services GA (multi-client, MIDI 2.0, loopback, timestamps and scheduling, naming): https://blogs.windows.com/windowsexperience/2026/02/17/making-music-with-midi-just-got-a-real-boost-in-windows-11/
- Windows MIDI Services rollout known issues (Korg drivers, dynamic-port visibility, WinRT timestamps): https://devblogs.microsoft.com/windows-music-dev/windows-midi-services-rollout-known-issues-and-workarounds/
- Windows 11 MIDI 2.0 and multi-client coverage: https://sonicstate.com/news/2026/04/02/windows11-midi-updates-add-20-multi-clients/
- libremidi (C++20; WinMM, WinUWP, Windows MIDI Services, CoreMIDI, ALSA, PipeWire, JACK; UMP): https://awesome.ecosyste.ms/projects/github.com%2Fceltera%2Flibremidi (repository: celtera/libremidi on GitHub; check LICENSE.md)
- OSC 1.0 specification (addresses, patterns, type tags, bundles, time tags): https://opensoundcontrol.stanford.edu/spec-1_0.html · examples: https://opensoundcontrol.stanford.edu/spec-1_0-examples.html · site with the 1.1 notes: https://opensoundcontrol.stanford.edu/
- VCV Rack polyphony (channel model the allocator feeds): https://vcvrack.com/manual/Polyphony · MIDI-CV Reset mode discussion: https://community.vcvrack.com/t/core-vcv-midi-cv-reset-mode-explanation/4308 · MPE in VCV: https://community.vcvrack.com/t/how-does-vcv-rack-work-with-mpe-devices/10580
- CLAP event and note model (plugin mapping): https://u-he.com/fwd/clap/
