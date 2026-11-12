# Wirescape plan (v3: standalone app with the web UI)

Owner: the planner. Anyone may tick a checkbox or add a "Signed" line in an issue. Last updated 2026-11-12 (Compass: owner priority OSC → Quad VCA/LPG → ASIO; D20).
How work is picked up and signed: `AGENTS.md`. Earlier plans (M0–M3, J, W) live in git history; their results are summarised under "Where we are".

## Now
- **Update 2026-11-12 (Compass; owner priority, board #473).** **v1.0.0** = today's `main` + the three lanes below (D20). Then the #593 candidate PR (versions 1.0.0), the owner test #581 on that merged SHA, and the tag. Nobody plans the next update until the owner does. Finish what is open before starting anything new (D18).
- **1. OSC (first call on builders, reviews, lead rulings).** Merged: C1 #618, C2 #620, row enable #645 (#612). Next: #659 (#657, `gpt-builder1`) adds the `config.version` gate per Keel, Coda T2 → closeout #672 (`docs/tasks/OSC_CLOSEOUT.md`) → #94 codec fuzzing (`claude-builder1`).
- **2. Quad VCA/LPG #490.** Q1 #601/#611 (`gpt-builder2`, native, independent T1) → merge → Q2 #602 (Studio card, two library patches, guide).
- **3. ASIO.** #627/#631 (`claude-builder1`, T2: Codex Lead or `gpt-reviewer`, Keel for the spec) and Windows setup #605/#608 (`gpt-builder2`, T1). The owner alone checks real ASIO sound.
- **Other open work keeps its writer but never takes review time from the lanes:** Groove inputs #614/#619, D5 #648 (#487), Web parity #629.
- **Supporting fixes only when they block a lane:** the Windows docs-only foundation failure (#666) is the current one. Fix the cause (UTF-8 IO, deterministic test clock); no skipped coverage.
- **After the three lanes:** Qt removal #661 (R3a #482 → R3b #468 → R5c → R4 #481, pre-authorized), then optimisation, then improving existing modules and the engine. Stereo Output #600, CI speed, Pi evidence (#271, #296) and engine debt (#91) follow; the owner ranks them.
- **Reviews:** T1 needs another session; T2 the other family; Keel approves ABI/spec/CI policy. Owner checks are batched in #45/#581.

## Where we are (2026-11-09)
**Works (verified on `main`):** Phase 1 code is merged: the `wirescape-engine` sidecar, `RealEngine`, the Electron shell, autostart, patch codec and catalogue sync. Live replan (#209). 28 ABI2 modules in the app catalogue, including Quantizer, Delay, Reverb, Spectral (with Interleave/Vocoder), Blend, Resonator, the 16-step Step Sequencer, Random Source and Groove (S1 native dual 32-step tracks). Telemetry with a measured LFO phase (#379), eight curated patches (#412), the device-free offline render and WAV evidence (#398), and the Windows sidecar pipe fix (#439, so the protocol tests now run on Windows). Pi headless audio and M8 MIDI were verified by the owner on 2026-11-07 (`docs/DEMO_CLASS.md` §1).

**Verified by the owner 2026-11-09:** the Electron app on Windows with real audio and MIDI (#463 branch). **Not yet verified by the owner:** the Electron app on the Pi, Settings device lists with the CM477 and M8, and live patching by ear (#271).

**Missing:** any packaged build (W1 source setup merged in #475; W2 package not started), OSC (#72), MIDI Learn (#287), the 60-minute Pi soak (#296), and the Groove S1(b) editor. Pi app automation exists (#450 preflight, #452 real ARM64 Electron smoke); an accepted owner SHA is pending (#483).

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
| D11 | ~~Qt stays buildable and untouched until Phase 4 removes it.~~ **Superseded 2026-11-09 by D15.** | Owner pivot (#437). |
| D12 | Dependencies named here (no separate stop needed): **electron** (MIT) and, in Phase 4, **electron-builder** (MIT), both pinned in `apps/desktop/package.json`. Playwright's existing Electron support covers app tests. Anything else is still a stop. | Small, mainstream and cross-platform. |
| D13 | Sidecar contingency (Compass, 2026-11-06): if #247 fails CI again after Keel's checklist, split 1B into **1B-1** (stdio protocol, session/gen, catalogue `hello`, null sink, fake MIDI, the D5 fade; everything CI can prove) and **1B-2** (PortAudio + real MIDI enumeration/selection and persistence). 1B-1 alone unblocks 1E on the null sink and #250. | Avoids a third attempt at one L-size T2 PR on the critical path; the device half is the part only the Pi and the owner can fully prove anyway. |
| D14 | **Web demo** (Compass, 2026-11-07, owner request): a static showcase site, the real engine compiled to WebAssembly in an AudioWorklet behind the unchanged Studio UI (`native/web`, `apps/studio/src/web`, `.github/workflows/web-demo.yml`, GitHub Pages). Emscripten is the one named new build dependency (CI only, pinned). No MIDI, OSC, devices or files. It is not a product stage: D1 (Electron app) stands, and the web build never blocks Phases 1 to 4. | Lets professors and friends try Wirescape from a link. It reuses the real DSP and UI, and costs one T1 lane. |
| D15 | **Qt is retired now** (owner, #437; Compass, 2026-11-09). No Qt fixes, no Qt fallback, no waiting for feature parity. MIDI and audio bugs belong to Studio + Electron + `wirescape-engine`. Old modules and sequencers may be redesigned rather than ported. Deletion follows dependency order (`docs/planner/CLEANUP.md` R1→R3), never by weakening a test. | One product; Qt costs agents time on every PR (#370, #416, #435). |
| D16 | Old (v1) patch import lives in the **Studio patch codec** (`PatchDoc.ts` `migrateV1`, already there). The engine keeps reading v1 execution copies in `runtime_patch`. The QtCore document library goes once its fixtures are asserted against both (R2, Keel review). There is no new C++ document library; this replaces the old 4B. | D3: the UI owns the document. Avoids a second document editor in C++. |
| D17 | **ABI1 retires after Qt**, as its own Keel-led T2 track (R6): the ABI1 bridge in engine2, `native/src/`, the 13 ABI1 modules in `native/modules/` and the ABI1 tests, with equivalence vectors and baselines frozen first. Folder renames (R7: `modules2`→`modules`, `engine2`→`engine`) wait for a quiet window. | A second module path inside the realtime core plus duplicate modules confuse agents and reviews. |
| D18 | **Finish before start.** While 10 or more implementation PRs are open, no lane opens a new module or feature issue; it merges or closes what it has first. Existing claims continue. Only one sequencer effort at a time is in active build (16-step → Groove → Chord). | ~20 open PRs with few merging; throughput comes from merging, not from more parallel starts. |
| D19 | **Git history is the archive.** No separate archive repo. `legacy/` and `docs/history/` leave the tree at tag `archive/qt-final`, done task files move to `docs/tasks/done/`, and `Wirescape-UI` is archived on GitHub. The read map is AGENTS → STATE → PLAN → task → ARCHITECTURE/specs. | Smaller search space and context; nothing is lost. |
| D20 | **App versions are releases, not development phases** (owner, 2026-11-12). A version (`v1.0.0`, then `v1.0.x` fixes, `v1.1.0` …) is a rebuilt, tested build the owner chooses to share. Development work is named by phases, sections and issue numbers, never by a future version, and nobody plans "v1.1 work" until the owner does. **v1.0.0** = today's 1.0 plus the owner-test fixes (#597) and the current priority lanes: OSC finished, Quad VCA/LPG, ASIO driver visibility (FlexASIO, #627) and the Studio visual fixes (#599). It is tagged by #593 once the owner test (#581) finds no major problems. Checkpoint tags (`checkpoint-<X>`, `archive/*`) are agent bookkeeping and are not app versions. | The owner ships updates when they are ready; phases describe the work, versions describe what users get. |

## Phases

Each phase lists sections (letter blocks). Sections in a phase run in parallel unless they say "after". One section = one issue = one branch/PR (big ones may split). Task files: `docs/tasks/P<phase><letter>_*.md`.

### Phase 0: Class demo, stopped and deferred (not passed)
- [x] 0A Demo patches (#221): keep and merge after review. They are the Phase 1 test patches.
- [x] 0B Pi demo launcher + `play --list-midi` (#222): merge the `play --list-midi` part and the player start. The browser/kiosk part of `demo.sh` stays as is but is deferred; don't extend it.
- [x] 0C Owner hardware check (2026-11-07): partial pass (record: `docs/DEMO_CLASS.md` §1).
- [x] **0D Live MIDI clock fix** (#237, merged 2026-11-06) (`docs/tasks/done/P0D_LIVE_MIDI_CLOCK.md`, T2, small): the headless audio callback renders with the live clock, plus a regression test. **Do first.** Phase 1's engine sidecar inherits it.
- [x] **0E Audible demo CC mappings** (#240, merged 2026-11-06) (`docs/tasks/done/P0E_DEMO_CC_DEPTH.md`): CC21–28 sweep obvious ranges in all 3 patches, with an offline test that proves it. Parallel with Phase 1.
- [ ] 0F Owner, run first in **Test Session 1** ([board #473](https://github.com/kalharbi11/wirescape/issues/473)): rebuild from a clean checkout and verify all 3 patches × CC21–28 with the M8 (`docs/DEMO_CLASS.md` §5b). Hardware acceptance of Phase 0 needs all 24 combinations.

### Phase 1: The standalone app with the real engine (Pi first, Windows works too)
**1.0 comes first** (small, about half a day). 1A, 1D and 1G can start right away; 1B, 1C and 1F follow 1.0 and run in parallel.
- [x] **1.0 Protocol v0** (#239, merged 2026-11-06) (`docs/tasks/done/P1_0_PROTOCOL.md`): the stdio protocol (D4), mapping every `EngineClient` call, `ack`/`refuse`/generation, the real device lists, status and telemetry v0, limits and the additive async-refusal extension.
- [x] **1A UI import** (#238, merged) (`P1A_UI_IMPORT.md`): `apps/studio/` + a hosted `web` CI job.
- [x] **1B `wirescape-engine` sidecar** (#247, merged) (`P1B_ENGINE_SIDECAR.md`, after 1.0): reuses `play`'s engine/MIDI/PortAudio code with the stdio protocol, **real audio and MIDI device lists and selection**, start at launch (D6), the D5 fallback swap and the catalogue (D9).
- [x] **1C `RealEngine.ts`** (#249, merged) (`P1C_REAL_ENGINE.md`, after 1.0): `EngineClient` over the preload bridge, async refusal and rollback, restart and restore, and a mock bridge for Playwright.
- [x] **1D Patch codec + catalogue sync** (#242, merged 2026-11-06) (`P1D_PATCH_CODEC.md`).
- [x] **1F Desktop shell** (#253, merged) (`P1F_DESKTOP_SHELL.md`, after 1.0): an Electron window with title bar, minimize, close, a File menu (open/save with native dialogs, quit), F11 fullscreen and Esc to leave it; it starts, watches and restarts the engine. Pi desktop entry and a Windows shortcut.
- [x] **1G UI fixes from the owner's Pi notes** (#241, merged 2026-11-06) (`P1G_UI_FIXES.md`): sharp lines when zoomed, a slightly grey canvas, an LFO scope trace instead of the dot, a MIDI In device dropdown, and real MIDI device menus in Settings.
- [ ] **1H Protocol conformance harness** (#250, T2, after the basic shapes of 1B and 1C exist): one golden NDJSON transcript from `ENGINE_PROTOCOL.md`, checked against both the real sidecar and RealEngine's mock bridge. It must be green before 1E signs off.
- [ ] **1E Bring-up + evidence** (`P1E_BRINGUP.md`, after 1B + 1F): launch scripts for the Pi and Windows, Pi runner build/tests, and a contention benchmark (the app UI animating while the engine renders: zero overruns).
- **Phase 1 is done when:** on the Pi, the owner double-clicks Wirescape and gets a normal window with sound running and no Start button. Settings lists the real CM477 and M8. Each of the three demo patches plays from the M8, and CC21–28 move the sound. Knobs change sound live, cable edits apply (fallback gap ≤150 ms), meters move, panic works, and the window minimizes and closes. Hosted CI and the Pi runner are green.

### ◆ Checkpoint A: after Phase 1
Owner + planner-role agent, about 30 minutes, on the Pi with the M8. Review whether the architecture holds, protocol v1 changes, and the Phase 2 order. Sections 2C, 2D and 2E keep going during the review.

### Phase 2: A live instrument on the Pi
- [ ] **2A Seamless live edits** (2A-1 #209 merged; 2A-2 sidecar wiring #267/#273): finish #209 with D7 and wire it into the sidecar. Add, remove, connect, disconnect, modulation amounts and patch switches, all while playing. Pi gap ≤20 ms at 256 frames; held notes survive unrelated edits; TSan/ASan clean.
- [ ] **2B MIDI and CC:** hot-plug-safe device enable, MIDI Learn → `cc_to_cv` rows (D8) saved in the patch, the global keyboard default (#184 semantics), the on-screen keyboard through the engine, and the patchable MIDI controls from #210/#163 (additive IDs).
- [ ] **2C Audio robustness:** remember the device; reconnect after loss; real dropouts; CPU = callback load; #200's semantics (mono devices, stale routes) in the Qt-free audio service; optional rate/buffer settings.
- [ ] **2D Patches:** a patch folder in `user_data/patches/`, bundled read-only starters, autosave and crash recovery, and a patch browser that switches live.
- [x] **2E Pi appliance** (#264, merged; owner check in #271 E): the app starts at login (desktop autostart), restarts the engine on crash, runs audio at realtime priority (owner setup doc), and boot-to-sound time is measured.
- [ ] **2F Soak:** a 60-minute Pi runner soak (engine with fake devices plus scripted edits); zero crashes and overruns. Reuses #164.

### ◆ Checkpoint B: "the Pi is an instrument" (the Pi half of J9 #45)
The owner jams for 30 minutes on the Pi with the M8: live patching, patch switching, no stuck notes, dropouts or crashes. #182's open items are folded in here.

### Phase 3: Real telemetry and module parity
- [ ] **3A Telemetry v1** (#143): a bounded snapshot owned by the render thread carrying effective values, LFO phase, envelope, per-module meters, event activity and held notes. Scope buffers are throttled by visibility. No serialisation on the audio thread.
- [ ] **3B Modulation parity:** cross-engine vectors (FakeEngine vs native).
- [ ] **3C Module parity:** ~~register Step Sequencer and Random Source~~ (#263, merged), a per-module smoke test through the engine, and every panel override checked.
- [ ] **3D OSC:** finish #82, with settings (enable, port, LAN toggle off by default) and the Monitor log in the UI; then fuzzing #94.
- [ ] **3E Stereo pairs:** #178/#204 semantics with a paired-cable gesture.

### Track S: Sound kit (parallel; original modules from the owner's prototypes)
Runs beside Phases 2–3 and never blocks them. Every module follows `docs/SDK_AUTHOR_GUIDE.md` §15 (porting a prototype). New IDs only; no ABI change without a Keel ruling.
- [ ] **S1 Conjunctions modules** (`docs/tasks/S1_CONJUNCTIONS_MODULES.md`): in order: Spectral (one FFT module, vocoder included), Blend, Cross Mod, Waveset, Shaper, Grains. Convolver waits for S3.
- [ ] S2 Effects: **S2-1 Delay** (#345, merged) and **S2-2 Reverb** (merged, original FDN); EQ and compressor/limiter later (research in `docs/reviews/EFFECTS_OPTIONS.md`).
- [ ] S6 CHAOS rebuild, component first ([board #473](https://github.com/kalharbi11/wirescape/issues/473)): **R1 Resonator** (#344, merged); R2 Waveshaper after the owner hears R1; R3 a CHAOS starter patch from Essentials; R4 a subpatch once Phase 5 subpatches exist.
- [ ] S3 Sample buffer service (SDK/engine, T2, Keel) and then the Sampler (start/loop CV, one-shot/gate/loop, reverse, 8 voices). Not started.
- [ ] S4 GUTTUR throat voice. **On hold** (owner).
- [ ] S5 Multichannel output + Quad Panner (needs a 4-output interface). Not started.

### Phase 4: Windows install, Qt retirement and cleanup (runs now, beside Phase 2; owner pivot 2026-11-09)
Sequence and file lists: `docs/planner/CLEANUP.md`.
- [x] **4A Windows source bootstrap** (#436 W1, merged #475; the owner's first session ran on #463): one PowerShell entry point, Qt-free build, pinned PortAudio DLL, isolated profile, device-off smoke, then the owner's audio/MIDI launch.
- [x] **4B Qt decoupling** (merged: R1 #476, R2 #478. R1 Qt-free default build + PortAudio move; R2 patch-fixture coverage onto the Studio codec, D16).
- [ ] **4C Remove Qt** (R3): `native/ui/`, the QtCore document library, Qt CI and static contexts, the laptop runner's Qt mode. The owner sets `allow-test-removal` for Qt-only tests only.
- [ ] **4D Tools and docs** (R4 `tools/dev.py` around Electron; R5 README, STATUS, a single read map, task archive, `legacy/` out of the tree).
- [ ] **4E Windows package** (#436 W2): electron-builder (D12) with the engine, PortAudio and licences, a SHA manifest, a clean-VM install/uninstall, and an update check (#188 semantics).
- [ ] **4F Pi setup and package** (#436 P1, #444–#446; then a `.deb` or AppImage built on the Pi runner).
- [ ] **4G ABI1 retirement and renames** (R6, R7; Keel, T2).
- [ ] **4H Laptop session** (the Windows half of J9 #45).

### ◆ Checkpoint C: Qt is gone, both platforms are packaged

### Phase 5: Later (in plan, not priority; in this order)
1. Touchscreen: real touch gestures on the Pi display, touch-friendly multi-cable add (UI Q-B), an on-screen keyboard.
2. Macro Oscillator (Plaits) signature panel polish and all 24 models checked.
3. A CHAOS-lite custom panel (generic until the owner redesigns it).
4. Dark theme (UI Q-C).
5. Musical core: Quantizer (pulled forward into the module lane, `docs/tasks/done/MOD_QUANTIZER.md`), a native Basic Sequencer, MIDI Out/Clock, MPE.
6. Instruments: subpatches/macros, Channel Expander, CHAOS rebuild, a proper Plaits/Braids port.
7. Portability and freeze: CLAP adapter, Daisy, ABI freeze (#44, #161), Windows mono perf #64, benchmark gate #91.
8. A browser/LAN mode (open the UI from another device). Deferred; nothing in Phases 1–4 depends on it.

## Checkpoints: how they work
- There are three (A, B, C), spaced a phase apart. Each one: tag `checkpoint-<X>` on `main`, run `python tools/review_pack.py` for the phase, then hold one session (owner + planner-role agent, about 30 minutes) whose decisions are recorded at the top of this file.
- **They don't stop work.** Sections of the next phase that don't depend on the review keep going. Only a High finding blocks, and only the sections it touches.

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
`AGENTS.md` (rules) · `docs/WORKFLOW.md` (claim/sign/hand-off detail) · `docs/DEMO_CLASS.md` (Phase 0 record + owner audio/MIDI check) · `docs/specs/` (contract; the engine protocol from 1.0) · `docs/ARCHITECTURE.md` (built today) · `docs/STATUS.md` (evidence) · `docs/tasks/` · `docs/reviews/` · `docs/SDK_AUTHOR_GUIDE.md` · `docs/planner/CLEANUP.md` (Qt retirement sequence) · `docs/history/` and `legacy/` (frozen; leaving the tree under D19).
