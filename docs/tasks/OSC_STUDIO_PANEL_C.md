<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC slice C: friendly OSC In editor and status (#603, part of #72)

The owner reported that OSC In has no menu, so there is no way to say what it listens to. The `osc.in` module already takes a `config` object (`native/modules2/osc_in/README.md`):

```
{port, lan, rows:[{pattern, argument, kind, input_min, input_max, output_min, output_max, smoothing_ms, timeout_ms, stale}]}
```

The sidecar already reports `status.osc` (`enabled`, `bind`, `port`, `lan`, `routes[]` with `state`, `received` and `dropped`). Neither is visible in Studio. This packet is **UI only** (T1): no engine, protocol or spec edits.

Read first: `apps/studio/AGENTS.md`, `native/modules2/osc_in/README.md`, `docs/specs/ENGINE_PROTOCOL.md` §status `osc`.

## What to build
1. **OSC In card:**
   - A status line: *Listening on 127.0.0.1:9000*, *Port busy*, or *LAN not confirmed*, plus a received-message counter that ticks.
   - A green activity dot that flashes when `received` grows.
   - An **Edit…** button.
2. **Editor dialog** (it writes `config` through the existing config edit/replan path, as Groove does):
   - **Port** (1024–65535). An "Allow other devices on my network (LAN)" toggle with a one-line warning. The toggle only saves `lan:true`; the existing `--confirm-lan` consent still governs the actual binding, so say so in the dialog.
   - **Eight row editors**, labelled to match the card's `row1`…`row8` jacks. Each has:
     - an address pattern, with a hint such as `/pico1/knob/1`, `*` allowed;
     - a kind: CV / Gate / Trigger;
     - an argument index;
     - input min/max, output min/max and smoothing (ms);
     - on timeout: hold or zero.
     Invalid values are refused inline, using the README's validation rules, and are never saved.
   - **Presets:** "Pico knobs 1–8" (the defaults), "TouchOSC faders /1/fader1–8" and "Reset to Pico defaults". (No "Clear": native fills all eight rows with Pico defaults and has no row disable. Keel ruling, #603.)
   - **"How to send":** a copyable example, *send `/pico1/knob/1 0.5` to `127.0.0.1:9000` (UDP)*, and a pointer to wiring `messages` into a Monitor card to see the raw traffic.
3. **Unknown fields (Keel ruling, #603):** unknown fields at node and patch level are preserved. Inside `config`, the editor writes only `port|lan|rows`. If a loaded config has other keys, show one line, "engine will refuse: <keys>", and drop them only when the user saves from the editor. A patch saved without opening the editor keeps them unchanged. An old patch with no `config` opens with the defaults shown.

## Not in this packet (follow-ups; they need a Keel protocol ruling)
- "Learn" (press, then move a control, to fill the address).
- A list of recently seen addresses: the status carries no payloads today.
- A Monitor card message list: that is C2 #604 (T2, a protocol addition).

## Tests
- Playwright with FakeEngine:
  - the editor round-trips the config;
  - an invalid pattern or range is refused;
  - a preset fills the rows;
  - unknown node/patch fields survive; unknown `config` keys show the warning and are dropped only on an editor save;
  - the status line and counter follow FakeEngine `status.osc` (listening, port-busy, lan-not-confirmed).
- Screenshots of the card and the dialog in the PR.
- **Patch guide impact:** update the OSC section of `docs/PATCH_DESIGNER_GUIDE.md` (how to set an address and test with the Monitor).

```acceptance
tier: T1
size: <= 700 changed lines, <= 10 files
must_pass:
  - bash -lc 'cd apps/studio && npm ci && WIRESCAPE_CHROMIUM=$(command -v chromium || command -v chromium-browser || command -v google-chrome) npm test'
  - ci: guard, linux-arm64, windows-x64, sanitizers green
stops: [protocol/spec edit, engine or sidecar edit, enabling LAN without the existing consent, dropping unknown node/patch fields, native or config-schema change (per-row disable is #612, T2), new dependency]
```

Signed: Compass (Claude), 2026-11-11
