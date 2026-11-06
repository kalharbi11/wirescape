# Wirescape plan (v3: web UI integration)

Owner: the planner. Anyone may tick a checkbox or add a "Signed" line in an issue. Last updated 2026-11-06.
How work is picked up and signed: `AGENTS.md`. Earlier plans (M0–M3, J, W) live in git history; their results are summarised under "Where we are".

## Now
- **Direction:** the new web UI (`kalharbi11/wirescape-ui`, finished with a fake engine and checked by the owner on the Pi) replaces the Qt Quick UI. The C++ engine stays. The Qt UI is frozen and is removed in Phase 4.
- **Priority order:** the Pi first, then Windows. On the Pi, these four matter most: **MIDI, CC, audio that starts without a button, and patch changes while audio keeps running.**
- **Phase 0 (today → class demo 2026-11-07):** a Pi demo built from what already works. No engine changes. Runbook: `docs/DEMO_CLASS.md`.
- **Phase 1 starts right after the demo**, or tonight in parallel if someone is free. It doesn't depend on Phase 0.
- **The pause on #29 is lifted** once this plan merges. The old queue comment (6540585199) and the scheduled check-in routines stay retired; this plan replaces them.

## Where we are (2026-11-06)
- **Engine (this repo, `main`):** ABI 2 `engine2` is the app engine. 19 modules in the app catalogue: VCO, LFO, VCF, ADSR, VCA, Clock, Function Envelope, Ladder, Mixer, Noise, Output, Scale/Offset, Slew, Host Keyboard, CHAOS-lite, MIDI In, MIDI→CV, CC→CV, Macro Oscillator (Plaits-derived). Polyphony, feedback loops and modulation in normalized space all work. The headless `wirescape play` starts real audio and MIDI as soon as it launches (behind `--confirm-audio` / `--confirm-midi`). Qt Studio runs fullscreen on the Pi (#174).
- **UI (`wirescape-ui`, `main` at `8cafdea`):** Vite + TypeScript, no framework. 21 catalogue modules, catalogue-driven panels, editing, undo, patch v2 save/load with unknown fields kept, system panels, 12 starters, Pi profile. Everything talks through `EngineClient` (`src/engine/EngineClient.ts`); `FakeEngine` is the only implementation.
- **Missing:** nothing connects the two. There's no browser-facing host, no `RealEngine`, and no real telemetry. Live graph edits exist only in draft #209, and `main` still refuses topology edits while audio runs. The UI's Step Sequencer and Random Source are SDK examples the app catalogue doesn't load yet.
- **Reference research (draft PRs, not decisions):** #217 engine protocol, #216 web stack, #213 open-work triage, #215 module panels, #214 DSP libraries, #212 launch-test findings. UI repo: [PHASE8_HANDOFF.md](https://github.com/kalharbi11/Wirescape-UI/blob/main/docs/PHASE8_HANDOFF.md) (integration gaps).

## Target architecture
```
Chromium (Pi kiosk) / browser app window (Windows)
        │  http + WebSocket on 127.0.0.1 only, per-launch token
wirescape-host  (C++17, Qt-free, one process)
  ├─ serves the built UI (apps/studio/dist) as static files
  ├─ protocol: commands → acks/refusals; status + telemetry pushed ≤30 Hz
  ├─ AppEngine + runtime_patch loader (existing) ── realtime audio thread (PortAudio)
  ├─ MIDI service (existing midi_host), later OSC (#82)
  └─ settings + patch folder (user_data/)
```

### Decisions (planner, 2026-11-06)
| # | Decision | Why |
|---|---|---|
| D1 | One C++ host process serves both the UI files and the protocol. No Tauri, Electron or Rust before Phase 4, and only then if a browser app window isn't good enough on Windows. | Fewest moving parts. The Pi uses system Chromium anyway. |
| D2 | The UI moves into this repo as `apps/studio/` (git subtree, history kept) at the start of Phase 1. After that `wirescape-ui` is read-only. | One PR can change both sides of the protocol; one CI; the Pi runner already serves this repo. |
| D3 | The UI owns the editable patch document (it's already lossless). The host owns the running graph: it validates, executes and refuses. Conformance tests in both directions guard the format. The host never rewrites the document; if a patch is missing `version` or `outputs`, it fills them only in its execution copy. | Avoids building a second document editor in C++ before Qt goes. |
| D4 | Protocol: JSON text messages over one WebSocket. Every command has an `id`; the host answers `ack` or `refuse` with the engine generation. Status and telemetry are pushed. Binary frames come later, for scope data only. | Simple to debug, and both the C++ and TS test clients are easy to write. |
| D5 | Parameter changes go through the engine's event queue (already live). Topology changes, modulation amounts and patch switches build a candidate graph that is swapped in by live replan (#209), so audio keeps running. Until #209 lands, the fallback is stop → swap → start with a short fade, target gap ≤150 ms. | You get patch changes on the fly in Phase 1 and a seamless version in Phase 2. |
| D6 | Audio and MIDI start when the host starts. The owner's launcher or autostart passes `--confirm-audio --confirm-midi`, and that is the device consent. The host remembers the last device; if it's lost, the host retries and the UI shows the state. Tests, CI and the Pi runner use null or fake devices only. | Gives "audio without a button" without breaking the devices-off rule. |
| D7 | Ruling for the #209 stop: when a live edit adds or removes a `midi.in` or `host.keyboard` module, the host refreshes the MIDI service subscriptions on the control thread right after the replan, and audio keeps running. Notes held on a removed source are released. Module IDs are never reused within a session: the UI allocates them monotonically and the engine already rejects stale reuse. | Unblocks #209 with a bounded, testable seam. |
| D8 | CC mapping writes `midi.cc_to_cv` config rows, the layer the DSP reads, through a host command. `ui.ccMap` stays only as a display hint. A `cc_to_cv` module holds up to 8 rows (existing limit); MIDI Learn adds another module when one is full. | Matches the native layer that already works. |
| D9 | The host sends its module catalogue on connect. The UI palette shows only modules the engine can run. Unknown modules in a patch stay visible and preserved, and are marked "not available". `public/wirescape_modules.json` is generated from native manifests, and a CI check catches drift. | IDs stay in sync automatically. |
| D10 | Pi first. Every Phase 1–2 item that touches compiled code needs a Pi runner build plus tests with fake devices. Each checkpoint includes one owner listening check on the Pi. | Matches the owner's priority. |
| D11 | Qt stays buildable and untouched until Phase 4. The only exception is a fix needed for the Phase 0 fallback. | No double work. |
| D12 | New dependencies named here (allowed without a separate stop): **civetweb** (MIT; HTTP, static files and WebSocket server/client in C++, pinned and vendored as one source pair, or FetchContent with a hash), **`ws`** (MIT, devDependency, mock host for UI tests). Anything else is still a stop. | Keeps the host small; avoids hand-writing WebSocket framing and SHA-1. |

## Phases

Each phase lists **sections** (the letter blocks). Sections inside a phase can run in parallel unless they say "after". A section is one issue, one branch and one PR (big ones may split). Task files with acceptance blocks: `docs/tasks/P<phase><letter>_*.md`.

### Phase 0: Class demo (2026-11-06 → 11-07). Runbook: `docs/DEMO_CLASS.md`
Goal: a smooth Pi demo. Sound starts when the demo starts, MIDI keys play, CC knobs move the sound, and the new web UI is on screen.
- [ ] **0A Demo patches** (`docs/tasks/P0A_DEMO_PATCHES.md`): `native/examples/class_demo_*.json`, three patches (CHAOS + MIDI, VCO/VCF/ADSR + LFO, Macro). Each has MIDI In → MIDI→CV and a `cc_to_cv` with 8 rows on CC numbers the owner gives, cabled to musically useful parameters. Validated by offline `wirescape play` renders.
- [ ] **0B Pi demo launcher** (`docs/tasks/P0B_PI_DEMO_LAUNCHER.md`): `tools/pi/demo.sh`. It starts `wirescape play <patch> --confirm-audio --confirm-midi`, serves the UI build on localhost and opens Chromium in kiosk mode. `demo.sh 1|2|3` switches patches (about a 1 s gap; seamless switching is Phase 2). `demo.sh stop` stops everything. Ships with a dry-run test that opens no devices.
- [ ] **0C Owner rehearsal tonight**, using the checklist in the runbook. The fallback is Qt Studio fullscreen with the same patches.
- Not in Phase 0: the web UI driving the real engine. In the demo, say it plainly: "engine and sound today; this interface connects to it next."

### Phase 1: Bridge. The web UI plays the real engine on the Pi
Start with **1.0**. It's small, and the parallel sections code against it.
- [ ] **1.0 Protocol v0** (`docs/tasks/P1_0_PROTOCOL.md`) → `docs/specs/HOST_PROTOCOL` (`.md`, written in P1.0). Maps every `EngineClient` call to a message. Defines `ack`/`refuse`, generation, telemetry v0 fields (output meters, CPU, dropouts, held notes, audio/MIDI state), auth (token + Origin check, 127.0.0.1 only), size limits and versioning. Also defines the additive `EngineClient` change for async refusal: an `onRefusal` callback plus pending-edit reconcile.
- [ ] **1A UI import + web CI** (`P1A_UI_IMPORT.md`): subtree-import `wirescape-ui` `main` into `apps/studio/`; add a hosted `web` CI job (typecheck, build, Playwright with FakeEngine); point the UI repo's README here.
- [ ] **1B `wirescape-host`** (`P1B_HOST.md`, after 1.0): `native/host/`, a civetweb server with static files, the WebSocket protocol, the token, AppEngine from `runtime_patch`, `loadGraph`, `setParam`, notes, panic, status, output meter, the D5 fallback swap, audio and MIDI at launch (D6), and the catalogue on connect (D9). CTest `host_protocol_tests` uses civetweb's client against a null audio device.
- [ ] **1C `RealEngine.ts`** (`P1C_REAL_ENGINE.md`, after 1.0): implements `EngineClient` over the protocol, with engine selection (`?engine=real|fake`, default real when served by the host), async refusal and rollback, reconnect that restores the patch, and a mock host (`ws`) for Playwright tests covering refusal, stale replies, disconnect and reconnect.
- [ ] **1D Patch codec + catalogue sync** (`P1D_PATCH_CODEC.md`): UI export fills module `version` from the catalogue, keeps `settings`, and keeps `outputs` routes correct when Output modules are added or removed. A native test loads every UI starter and fixture through `runtime_patch`; a UI test round-trips the native examples; unknown fields survive both directions. A script generates `wirescape_modules.json` from native manifests and CI checks for drift.
- [ ] **1E Pi bring-up** (`P1E_PI_BRINGUP.md`, after 1B): `tools/pi/studio.sh` (host + Chromium kiosk). A Pi runner job builds host + UI and runs the protocol tests with null audio. A contention benchmark runs 10 min of headed Chromium (stress patch) beside the host with null audio, and must show zero render overruns at 256 frames.
- **Phase 1 is done when:** on the Pi, one script gives sound with no button press. The web UI loads all starters, plays from the MIDI keyboard, changes sound live with knobs, applies cable and module edits (fallback gap ≤150 ms), moves the meters, and panic works. Pi runner and hosted CI are green, and the owner has listened.

### ◆ Checkpoint A: after Phase 1
Owner + planner (or any agent in the planner role), about 30 minutes, on the Pi. Review: does the architecture hold, which protocol v0 → v1 changes are needed, and the order of Phase 2. Phase 2 sections that don't depend on the outcome (2C, 2D, 2E) keep going during the review.

### Phase 2: A live instrument on the Pi
- [ ] **2A Seamless live edits:** finish #209 with ruling D7 and wire it into the host. Add, remove, connect and disconnect, change modulation amounts, and switch whole patches while playing, all with no stop. Acceptance: Pi gap ≤20 ms at 256 frames (#209's existing bound), held notes survive edits that don't touch their path, and a TSan/ASan run passes.
- [ ] **2B MIDI and CC:** device list and enable/disable in Settings, hot-plug safe. MIDI Learn → `cc_to_cv` rows (D8), saved in the patch. The global MIDI keyboard default (#184 semantics). The on-screen keyboard plays through the host. Patchable MIDI controls from #210/#163 are additive, with IDs unchanged.
- [ ] **2C Audio robustness:** choose and remember the device; reconnect after device loss; real dropout counter; CPU = callback load. Port #200's semantics (mono devices, stale-route refusal) into the Qt-free audio service. Sample-rate and buffer settings are optional (the UI hides them if missing).
- [ ] **2D Patches on the Pi:** the host lists and saves patches in `user_data/patches/`. Starters are bundled read-only. Autosave and crash recovery go through the host. A patch browser in the UI switches patches live (uses 2A when merged, the fallback swap before that).
- [ ] **2E Pi appliance:** autostart at login (systemd user service for the host + labwc/Chromium kiosk), restart on crash, audio thread realtime priority (setup doc for the owner), hidden cursor, and measured boot-to-sound time.
- [ ] **2F Soak:** a 60-minute Pi runner soak with fake devices: host + Chromium + a scripted edit every few seconds, with zero crashes and overruns. It reuses #164's idle-loop infrastructure.

### ◆ Checkpoint B: "the Pi is an instrument" (replaces the Pi half of J9 #45)
The owner jams for 30 minutes on the Pi with an external MIDI keyboard and CC controller: live patching, patch switching, no stuck notes, no dropouts, no crashes. Fold #182's open items in here and close it if they pass.

### Phase 3: Real telemetry and module parity
- [ ] **3A Telemetry v1** (#143): a bounded, render-owned snapshot published to the host thread. It carries effective parameter values, LFO phase, envelope position, per-module meters, event activity, held notes and connected-output values. Scope taps arrive as binary frames, throttled by visibility. There is no serialisation on the audio thread.
- [ ] **3B Modulation parity:** cross-engine vectors (FakeEngine vs native) for log cutoff, linear, negative amounts, multiple sources, bounds and stepped parameters. Differences are fixed or recorded.
- [ ] **3C Module parity:** register the SDK examples Step Sequencer and Random Source in the app catalogue (additive). A per-module smoke test through the host loads each module, sets every parameter, renders offline and checks the output is finite. Every panel override is checked against the real engine.
- [ ] **3D OSC:** finish #82 (codec, service, Monitor), with host settings (enable, port, LAN toggle, off by default) and the Monitor log in the UI. Fuzzing #94 follows.
- [ ] **3E Stereo pairs:** port #178/#204's semantics. Native `stereo_pair` metadata goes in the catalogue, the UI gets a paired-cable gesture, and both cables are created in one transaction.

### Phase 4: Windows, packaging, Qt retirement
- [ ] **4A Windows host + launcher:** host build on Windows; a launcher that opens Edge/Chrome in `--app` mode; a desktop shortcut; an update check showing SHA/date (#188 semantics).
- [ ] **4B Qt-free document library:** port the lossless v2 writer and v1→v2 migration off QtCore, so the host can import old v1 files.
- [ ] **4C Remove the Qt UI:** remove `native/ui/`, the QML, and the Qt CI and static-analysis context; keep every engine and document test. The owner sets `allow-test-removal` for the Qt UI tests only. Update `ARCHITECTURE.md` and `STATUS.md`.
- [ ] **4D Packaged shell (only if needed):** decided at Checkpoint C. Tauri/WebView2 per #216, or keep the browser app window.
- [ ] **4E Laptop session** (the Windows half of J9 #45).

### ◆ Checkpoint C: Qt is gone, both platforms work → Preview 0.7

### Phase 5: Later (in plan, not priority; in this order)
1. Touchscreen: real touch gestures on the Pi display, touch-friendly multi-cable add (UI Q-B), an on-screen keyboard.
2. Macro Oscillator (Plaits) signature panel polish and checking all 24 models.
3. A CHAOS-lite custom panel. It uses the generic panel until the owner redesigns it.
4. Dark theme (UI Q-C).
5. Beta 0.8 musical core: Quantizer, a native Basic Sequencer, MIDI Out/Clock, MPE (old M4–M6).
6. Instruments (old M7): subpatches/macros, Channel Expander, CHAOS rebuild, a proper Plaits/Braids port (Track X).
7. Portability and freeze: CLAP adapter, Daisy build, ABI freeze (#44, #161), Windows mono perf #64, benchmark gate #91.

## Checkpoints: how they work
- There are three (A, B, C), spaced a phase apart. Each one: tag `checkpoint-<X>` on `main`, run `python tools/review_pack.py` for the phase, then hold one session (owner + planner-role agent, about 30 minutes) whose decisions are recorded at the top of this file.
- **They don't stop work.** Sections of the next phase that don't depend on the review keep going. Only a High finding blocks, and only the sections it touches.
- An optional 10-minute debrief after the class: post what broke or confused people as one issue comment. It feeds Checkpoint A.

## Open work at resume (apply once, then delete this table)
| Item | Action |
|---|---|
| #209 / #186 live graph | Keep. It becomes 2A with ruling D7. |
| #82 / #72 OSC | Keep. It becomes 3D (Qt presentation dropped). |
| #210 / #163 MIDI controls | Keep. It becomes 2B. |
| #200 / #140 audio output | Close the PR (Qt). Port its semantics and test ideas in 2C. |
| #192 / #187 starters | Close. The UI already carries the six starters; the patch library is 2D. |
| #204 / #178 stereo pairs | Close the PR. It becomes 3E. |
| #181 / #180 module guide | Keep (T0 docs). Fix its link-scanner blocker. |
| #161 / #159 portability | Park. It becomes Phase 5.7. |
| #218 planner note | Close as superseded by this plan. |
| Research #211–#217 | Close without merging; keep the branches. Phase 1 tasks link them. |
| Issues #201 #198 #184 #183 #174 #167 #154 #152 #139 #136 #132 | Close: fixed on `main` or historical (#213 triage). |
| #188 → 4A · #143 → 3A · #70 → 1E/2E · #164 → 2F · #45 → Checkpoints B + 4E · #182 → Checkpoint B | Relabel and link. Keep open. |

## Settled engineering decisions (unchanged; detail in `docs/specs/ABI2_PLAN.md`)
| Area | Decision |
|---|---|
| Cables | float32, audio rate, 1–16 channels, CONSTANT flag. No type-gated cables, feedback allowed |
| Polyphony | Channels on the cable; the source sets the count; one shared voice allocator |
| Units | Audio ±1; pitch 1.0/oct, 0 = C4; gates 0/1 via Schmitt trigger; hints never block |
| CV → parameters | Host-side, non-destructive, amount on the cable, normalized space, per voice |
| Events | Per-block sorted lists with sample offsets; overflow never drops note-offs |
| Compatibility | Module, port and parameter IDs never change; migration is lossless; unknown data is preserved |
| Licence | SDK MIT; app and Essentials GPL-3.0 |
| Network | Localhost by default; LAN only via an explicit toggle |
| CI | Linux ARM64 on every PR; Windows on ready PRs or `ci/windows`; sanitizers; the Pi via `wirescape-pi` only |

## Vision (short)
Wirescape is an open-source modular patching environment for music. Modules are easy for people and AI agents to build. Complex instruments work as well as simple utilities. Module DSP moves unchanged into plugins and Daisy-class hardware. MIDI, OSC and more plug in as services. The Pi 5 is the performance target.

## Document map
`AGENTS.md` (rules) · `docs/WORKFLOW.md` (claim/sign/hand-off detail) · `docs/DEMO_CLASS.md` (Phase 0) · `docs/specs/` (contract; `HOST_PROTOCOL.md` from 1.0) · `docs/ARCHITECTURE.md` (built today) · `docs/STATUS.md` (evidence) · `docs/tasks/` · `docs/reviews/` · `docs/SDK_AUTHOR_GUIDE.md` · `docs/history/` and `legacy/` (frozen; don't read unless asked).
