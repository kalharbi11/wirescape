# Wirescape plan (v3: standalone app with the web UI)

Owner: the planner. Anyone may tick a checkbox or add a "Signed" line in an issue. Last updated 2026-11-06 (Compass).
How work is picked up and signed: `AGENTS.md`. Earlier plans (M0–M3, J, W) live in git history; their results are summarised under "Where we are".

## Now
- **Updated 2026-11-07 after the Pi test (owner notes + test report).** The browser/kiosk demo is **stopped and deferred, not passed**: the web UI ran on its simulated engine, so it showed fake audio and MIDI devices, and kiosk mode had no way to leave. **New direction:** go straight to the **standalone Wirescape app**, a real window with title bar, minimize, close and menus, containing the web UI (which the owner likes) wired to the real C++ engine. No browser, kiosk, HTTP server or LAN step.
- **Pi hardware test 2026-11-07 (owner): PARTIAL PASS.** Real audio through the CM477 (`--device pulse`), M8 notes play the native subtractive synth, and CC21 drives VCA gain. MIDI notes only worked after a local one-line fix (the headless audio callback rendered without the live MIDI clock), which is now **P0D**, the first thing to build. CC21 → filter wasn't audible (**P0E**). CHAOS, Macro and the full CC21–28 set are still unverified.
- **Progress 2026-11-06 (Compass):** P0D, P1.0 and P1A are merged. P1B sidecar, P1C RealEngine and P1F shell are unblocked and run in parallel. P0E (#240), P1D (#242) and P1G (#241) are in final review/fix. **Owner Pi re-test is batched:** one session after #240 merges (P0F), not one per fix.
- **Priority order:** (1) ~~P0D live MIDI clock fix~~ done; (2) the standalone app (Phase 1) with real audio and MIDI device menus, Pi first; (3) P0E audible CC mappings; (4) the owner's UI fixes (P1G, in parallel); (5) live editing (Phase 2); then everything else.
- **Windows can speed up testing:** the app and engine are cross-platform from day one, so a section can be smoke-tested on the laptop when that's quicker. The Pi stays the acceptance target, and Pi-only problems (like the USB adapter) are tested on the Pi.
- **The pause on #29 is lifted** once this plan merges. The old queue comment and scheduled check-ins stay retired.

## Where we are (2026-11-07)
**Works (verified):**
- Native engine: ABI 2 `engine2` with 19 modules in the app catalogue: VCO, LFO, VCF, ADSR, VCA, Clock, Function Envelope, Ladder, Mixer, Noise, Output, Scale/Offset, Slew, Host Keyboard, CHAOS-lite, MIDI In, MIDI→CV, CC→CV, Macro Oscillator. Polyphony, feedback, normalized modulation.
- Pi Release headless build of `~/wirescape-demo` (branch `demo` = #222 + #221) **passed** on 2026-11-07.
- **Audible output on the Pi:** `Six_Essentials_Demo.json` via `--device pulse` (the CM477 selected in Pi OS), 48 kHz / 256: sound, CPU 1–2%, `xruns=0`, `faults=0`.
- **M8 → engine:** with a local fix (P0D), `m8_2_subtractive.json` plays M8 notes 36/40/43/48 with correct pitch, CPU 5–6%, `xruns=0`. libremidi is connected to the M8 (`aconnect`: 24:0 → 129:0).
- **CC → DSP:** CC21 (full 0–127) through `midi.cc_to_cv` drives VCA gain in a diagnostic patch copy.
- Pi MIDI at the Linux level: Dirtywave M8 over USB (endpoint `M8 MIDI 1|M8:M8 MIDI 1`); `aseqdump` shows notes 36/40/43/48 and CC21 on MIDI channel 1.
- Demo patches (#221): CHAOS, subtractive, Macro, with CC21–28 mapped. Launcher + `play --list-midi` (#222).
- Web UI (`wirescape-ui` `main`): complete with `FakeEngine`; on the Pi it runs smoothly in Chromium (owner, 2026-11-07).

**Unverified:**
- The P0D fix on `main` (it exists only as the owner's local edit).
- CC21 → filter cutoff audibility, the full CC21–28 set (CCH must be set to CC28 on the M8), and the CHAOS and Macro patches with the M8.
- Note release and stuck notes under MIDI disconnect, and long-run stability with live MIDI.

**Missing:** the app shell, the engine sidecar, `RealEngine`, and real device lists in the UI. Live graph edits exist only in draft #209. The UI's Step Sequencer and Random Source aren't in the app catalogue.

**Not the path:** Qt Studio. It's a native app with real device menus, but it's the old UI the owner is replacing. It stays frozen; the owner may use it as a hardware sanity check only.

## Target architecture
```
Wirescape app (Electron: native window, title bar, minimize/close, menus, F11 fullscreen, Esc leaves fullscreen)
  ├─ renderer: the web UI (apps/studio), unchanged except RealEngine via the preload bridge
  └─ main process starts and supervises
       wirescape-engine (C++17, Qt-free sidecar): AppEngine + runtime_patch + PortAudio + MIDI service
            ↕ stdin/stdout, newline-delimited JSON (protocol v0). No network sockets at all
```

### Decisions (planner, 2026-11-07; replaces the 2026-11-06 set)
| # | Decision | Why |
|---|---|---|
| D1 | The app is **Electron** (main + renderer + preload) with the engine as a **separate C++ process**. Rejected: Qt Studio (old UI), a system webview/WebKitGTK (different renderer on the Pi, performance unknown), a browser tab or kiosk (not an app, no way to quit). | Electron ships the same Chromium the UI already runs smoothly in on the Pi, gives a real window on both platforms, and keeps an engine crash from taking the window down. |
| D2 | The UI moves into this repo as `apps/studio/` (git subtree, history kept); the shell lives in `apps/desktop/`. After the import, `wirescape-ui` is read-only. | One repo, one CI, one PR per change across both sides. |
| D3 | The UI owns the editable patch document (already lossless). The engine owns the running graph: it validates, executes and refuses. It never rewrites the document; a missing `version` or `outputs` is filled in only in its execution copy. Conformance tests cover both directions. | No second document editor in C++ before Qt goes. |
| D4 | Protocol: one JSON object per line over the engine's stdin/stdout. Commands carry an `id`; the engine answers `ack` or `refuse` with the graph generation. Status (≤10 Hz) and telemetry (≤30 Hz) are pushed. Logs go to stderr. The preload exposes exactly one bridge object to the renderer (`contextIsolation` on, `nodeIntegration` off). | Simplest possible transport, nothing to secure on the network, easy to test. |
| D5 | Parameter changes go through the engine's event queue (already live). Topology changes, modulation amounts and patch switches build a candidate graph; it's swapped in by live replan (#209) once that lands. Until then: stop → swap → start with a short fade, gap ≤150 ms. | Patch changes on the fly in Phase 1, seamless in Phase 2. |
| D6 | Audio and MIDI start when the app starts. The owner's launcher (desktop entry or shortcut) passes `--confirm-audio --confirm-midi` to the app, which forwards them to the engine; that is the device consent. The last audio device and the enabled MIDI devices are remembered in `user_data/settings.json`. If a device is lost, the engine retries. Tests and CI start the engine with a null sink and fake MIDI. | "Audio without a button" without breaking the devices-off rule. |
| D7 | Ruling for #209's stop: after a live edit that adds or removes a `midi.in`/`host.keyboard` module, the engine refreshes the MIDI subscriptions on its control thread while audio keeps running. Held notes on a removed source are released. Module IDs are never reused within a loaded document; a successful `loadGraph` starts a fresh ID namespace (Keel, 2026-11-06, `ENGINE_PROTOCOL.md` §5). | Unblocks #209 with a bounded seam. |
| D8 | CC mapping writes `midi.cc_to_cv` config rows (up to 8 per module; MIDI Learn adds a module when one is full). `ui.ccMap` stays only as a display hint. | The DSP already reads this layer. |
| D9 | The engine sends its module catalogue and real device lists on start. The UI palette shows only modules the engine can run; unknown modules in a patch stay preserved and marked "not available". `public/wirescape_modules.json` is generated from native manifests, and CI checks for drift. | IDs stay in sync automatically. |
| D10 | Pi first for acceptance; Windows allowed for faster smoke tests. Compiled-code sections need Pi runner evidence with fake devices. Each checkpoint includes one owner hardware check on the Pi. | Matches the owner's priority. |
| D11 | Qt stays buildable and untouched until Phase 4 removes it. | No double work. |
| D12 | Dependencies named here (no separate stop needed): **electron** (MIT) and, in Phase 4, **electron-builder** (MIT), both pinned in `apps/desktop/package.json`. Playwright's existing Electron support covers app tests. Anything else is still a stop. | Small, mainstream and cross-platform. |

## Phases

Each phase lists sections (letter blocks). Sections in a phase run in parallel unless they say "after". One section = one issue = one branch/PR (big ones may split). Task files: `docs/tasks/P<phase><letter>_*.md`.

### Phase 0: Class demo, stopped and deferred (not passed)
- [x] 0A Demo patches (#221): keep and merge after review. They are the Phase 1 test patches.
- [x] 0B Pi demo launcher + `play --list-midi` (#222): merge the `play --list-midi` part and the player start. The browser/kiosk part of `demo.sh` stays as is but is deferred; don't extend it.
- [x] 0C Owner hardware check (2026-11-07): partial pass (record: `docs/DEMO_CLASS.md` §1).
- [x] **0D Live MIDI clock fix** (#237, merged 2026-11-06) (`docs/tasks/P0D_LIVE_MIDI_CLOCK.md`, T2, small): the headless audio callback renders with the live clock, plus a regression test. **Do first.** Phase 1's engine sidecar inherits it.
- [ ] **0E Audible demo CC mappings** (`docs/tasks/P0E_DEMO_CC_DEPTH.md`): CC21–28 sweep obvious ranges in all 3 patches, with an offline test that proves it. Parallel with Phase 1.
- [ ] 0F Owner, after 0D/0E merge: rebuild from a clean checkout and verify all 3 patches × CC21–28 with the M8 (`docs/DEMO_CLASS.md` §5b). Hardware acceptance of Phase 0 needs all 24 combinations.

### Phase 1: The standalone app with the real engine (Pi first, Windows works too)
**1.0 comes first** (small, about half a day). 1A, 1D and 1G can start right away; 1B, 1C and 1F follow 1.0 and run in parallel.
- [x] **1.0 Protocol v0** (#239, merged 2026-11-06) (`docs/tasks/P1_0_PROTOCOL.md`): the stdio protocol (D4), mapping every `EngineClient` call, `ack`/`refuse`/generation, the real device lists, status and telemetry v0, limits and the additive async-refusal extension.
- [x] **1A UI import** (#238, merged) (`P1A_UI_IMPORT.md`): `apps/studio/` + a hosted `web` CI job.
- [ ] **1B `wirescape-engine` sidecar** (`P1B_ENGINE_SIDECAR.md`, after 1.0): reuses `play`'s engine/MIDI/PortAudio code with the stdio protocol, **real audio and MIDI device lists and selection**, start at launch (D6), the D5 fallback swap and the catalogue (D9).
- [ ] **1C `RealEngine.ts`** (`P1C_REAL_ENGINE.md`, after 1.0): `EngineClient` over the preload bridge, async refusal and rollback, restart and restore, and a mock bridge for Playwright.
- [ ] **1D Patch codec + catalogue sync** (`P1D_PATCH_CODEC.md`).
- [ ] **1F Desktop shell** (`P1F_DESKTOP_SHELL.md`, after 1.0): an Electron window with title bar, minimize, close, a File menu (open/save with native dialogs, quit), F11 fullscreen and Esc to leave it; it starts, watches and restarts the engine. Pi desktop entry and a Windows shortcut.
- [ ] **1G UI fixes from the owner's Pi notes** (`P1G_UI_FIXES.md`, independent; best after 1A): sharp lines when zoomed, a slightly grey canvas, an LFO scope trace instead of the dot, a MIDI In device dropdown, and real MIDI device menus in Settings.
- [ ] **1E Bring-up + evidence** (`P1E_BRINGUP.md`, after 1B + 1F): launch scripts for the Pi and Windows, Pi runner build/tests, and a contention benchmark (the app UI animating while the engine renders: zero overruns).
- **Phase 1 is done when:** on the Pi, the owner double-clicks Wirescape and gets a normal window with sound running and no Start button. Settings lists the real CM477 and M8. Each of the three demo patches plays from the M8, and CC21–28 move the sound. Knobs change sound live, cable edits apply (fallback gap ≤150 ms), meters move, panic works, and the window minimizes and closes. Hosted CI and the Pi runner are green.

### ◆ Checkpoint A: after Phase 1
Owner + planner-role agent, about 30 minutes, on the Pi with the M8. Review whether the architecture holds, protocol v1 changes, and the Phase 2 order. Sections 2C, 2D and 2E keep going during the review.

### Phase 2: A live instrument on the Pi
- [ ] **2A Seamless live edits:** finish #209 with D7 and wire it into the sidecar. Add, remove, connect, disconnect, modulation amounts and patch switches, all while playing. Pi gap ≤20 ms at 256 frames; held notes survive unrelated edits; TSan/ASan clean.
- [ ] **2B MIDI and CC:** hot-plug-safe device enable, MIDI Learn → `cc_to_cv` rows (D8) saved in the patch, the global keyboard default (#184 semantics), the on-screen keyboard through the engine, and the patchable MIDI controls from #210/#163 (additive IDs).
- [ ] **2C Audio robustness:** remember the device; reconnect after loss; real dropouts; CPU = callback load; #200's semantics (mono devices, stale routes) in the Qt-free audio service; optional rate/buffer settings.
- [ ] **2D Patches:** a patch folder in `user_data/patches/`, bundled read-only starters, autosave and crash recovery, and a patch browser that switches live.
- [ ] **2E Pi appliance:** the app starts at login (desktop autostart), restarts the engine on crash, runs audio at realtime priority (owner setup doc), and boot-to-sound time is measured.
- [ ] **2F Soak:** a 60-minute Pi runner soak (engine with fake devices plus scripted edits); zero crashes and overruns. Reuses #164.

### ◆ Checkpoint B: "the Pi is an instrument" (the Pi half of J9 #45)
The owner jams for 30 minutes on the Pi with the M8: live patching, patch switching, no stuck notes, dropouts or crashes. #182's open items are folded in here.

### Phase 3: Real telemetry and module parity
- [ ] **3A Telemetry v1** (#143): a bounded snapshot owned by the render thread carrying effective values, LFO phase, envelope, per-module meters, event activity and held notes. Scope buffers are throttled by visibility. No serialisation on the audio thread.
- [ ] **3B Modulation parity:** cross-engine vectors (FakeEngine vs native).
- [ ] **3C Module parity:** register Step Sequencer and Random Source in the app catalogue (additive), a per-module smoke test through the engine, and every panel override checked.
- [ ] **3D OSC:** finish #82, with settings (enable, port, LAN toggle off by default) and the Monitor log in the UI; then fuzzing #94.
- [ ] **3E Stereo pairs:** #178/#204 semantics with a paired-cable gesture.

### Phase 4: Windows packaging and Qt retirement
- [ ] **4A Windows installer:** electron-builder package with the bundled engine and PortAudio DLL, a desktop shortcut, and an update check showing SHA/date (#188 semantics).
- [ ] **4B Qt-free document library:** the lossless v2 writer and v1→v2 migration off QtCore, so the app can import old v1 files.
- [ ] **4C Remove the Qt UI:** `native/ui/`, the QML, and the Qt CI and static-analysis context. Keep every engine and document test; the owner sets `allow-test-removal` for the Qt UI tests only. Update `ARCHITECTURE.md` and `STATUS.md`.
- [ ] **4D Pi package:** a `.deb` or AppImage built on the Pi runner, so updates don't need a local rebuild.
- [ ] **4E Laptop session** (the Windows half of J9 #45).

### ◆ Checkpoint C: Qt is gone, both platforms are packaged → Preview 0.7

### Phase 5: Later (in plan, not priority; in this order)
1. Touchscreen: real touch gestures on the Pi display, touch-friendly multi-cable add (UI Q-B), an on-screen keyboard.
2. Macro Oscillator (Plaits) signature panel polish and all 24 models checked.
3. A CHAOS-lite custom panel (generic until the owner redesigns it).
4. Dark theme (UI Q-C).
5. Beta 0.8 musical core: Quantizer, a native Basic Sequencer, MIDI Out/Clock, MPE.
6. Instruments: subpatches/macros, Channel Expander, CHAOS rebuild, a proper Plaits/Braids port.
7. Portability and freeze: CLAP adapter, Daisy, ABI freeze (#44, #161), Windows mono perf #64, benchmark gate #91.
8. A browser/LAN mode (open the UI from another device). Deferred; nothing in Phases 1–4 depends on it.

## Checkpoints: how they work
- There are three (A, B, C), spaced a phase apart. Each one: tag `checkpoint-<X>` on `main`, run `python tools/review_pack.py` for the phase, then hold one session (owner + planner-role agent, about 30 minutes) whose decisions are recorded at the top of this file.
- **They don't stop work.** Sections of the next phase that don't depend on the review keep going. Only a High finding blocks, and only the sections it touches.

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
| #221 / #222 demo patches + launcher | Review and merge (Phase 0 rows above). |
| Research #211–#217 | Close without merging; keep the branches. Phase 1 tasks link them. |
| Issues #201 #198 #184 #183 #174 #167 #154 #152 #139 #136 #132 | Close: fixed on `main` or historical (#213 triage). |
| #188 → 4A · #143 → 3A · #70 → 0C/1E · #164 → 2F · #45 → Checkpoints B + 4E · #182 → Checkpoint B | Relabel and link. Keep open. |

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
`AGENTS.md` (rules) · `docs/WORKFLOW.md` (claim/sign/hand-off detail) · `docs/DEMO_CLASS.md` (Phase 0 record + owner audio/MIDI check) · `docs/specs/` (contract; the engine protocol from 1.0) · `docs/ARCHITECTURE.md` (built today) · `docs/STATUS.md` (evidence) · `docs/tasks/` · `docs/reviews/` · `docs/SDK_AUTHOR_GUIDE.md` · `docs/history/` and `legacy/` (frozen; don't read unless asked).
