<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Status and verification

Updated 2026-11-10. Wirescape is an alpha Electron application around Studio and
the native `wirescape-engine`. Historical checkpoint tables remain in Git
history. A source build, device-free test and owner hardware session establish
different things; the evidence below keeps those boundaries explicit.

| Scope | Current evidence | Boundary |
|---|---|---|
| Application foundation | Sidecar, RealEngine, Electron shell, patch codec/catalogue sync and live replan are merged; [architecture](ARCHITECTURE.md), [plan](PLAN.md) | The app uses the native engine over stdio; Studio owns lossless editing |
| Modules and patches | ABI2 catalogue includes the bounded OSC In/Monitor modules from [#518](https://github.com/kalharbi11/wirescape/pull/518), plus existing Essentials and effect modules | OSC module registration and codec tests do **not** imply the current sidecar UDP host or Monitor UI has shipped; #72/#529 remain separate |
| Groove updates | G3 measured telemetry [#501](https://github.com/kalharbi11/wirescape/pull/501), G1 per-track lengths/gates/conditions [#505](https://github.com/kalharbi11/wirescape/pull/505), and Studio G2a/G2b editor [#509](https://github.com/kalharbi11/wirescape/pull/509)/[#516](https://github.com/kalharbi11/wirescape/pull/516) merged | Later chord-step and per-track-speed features have their own acceptance; no physical Pi performance claim |
| CHAOS and patch library | Eight UI-only presets [#522](https://github.com/kalharbi11/wirescape/pull/522), additive seeded LFO shapes [#533](https://github.com/kalharbi11/wirescape/pull/533), and three CHAOS Lab patches [#531](https://github.com/kalharbi11/wirescape/pull/531) merged | Existing CHAOS ID and old sine default retained; patch publication still depends on the deployed build |
| MIDI and audio settings | [#500](https://github.com/kalharbi11/wirescape/pull/500) adds Studio knob MIDI Learn/Forget; [#399](https://github.com/kalharbi11/wirescape/pull/399) adds output route/rate metadata with refusal of incompatible or ambiguous choices | Device-free/fake-MIDI and hosted checks do not prove physical controller or audio-device recovery |
| Windows protocol | [#439](https://github.com/kalharbi11/wirescape/pull/439) merged; protocol and golden tests execute on Windows | A skipped test is not passing evidence |
| Owner Windows session, 2026-11-09 | [#463](https://github.com/kalharbi11/wirescape/pull/463) runtime fixes merged: WASAPI headphones selectable, starters switch and sound, computer keyboard and Launchkey MIDI work at 48 kHz/256 | First-session owner evidence; extended hardware/soak acceptance remains separate |
| Owner Pi session, 2026-11-07 | Headless playback and M8 MIDI, recorded in [DEMO_CLASS](DEMO_CLASS.md) | Does not certify the current Electron app on the Pi |
| Automated app/render evidence | Measured LFO phase telemetry, curated patch fixtures and device-free render evidence merged; [#442](https://github.com/kalharbi11/wirescape/pull/442) extends real Electron/sidecar tests | Fake MIDI/null audio tests do not establish physical audio/MIDI or Pi display performance |
| Pi source setup | [#450](https://github.com/kalharbi11/wirescape/pull/450), [#452](https://github.com/kalharbi11/wirescape/pull/452) and [#453](https://github.com/kalharbi11/wirescape/pull/453) established an accepted earlier main parent [`cd7999e`](https://github.com/kalharbi11/wirescape/commit/cd7999e4a02308df0db918cfbab6974b97b6782f) (preflight 28/28 and native/Electron smoke), as recorded in [PI_FRESH_INSTALL](PI_FRESH_INSTALL.md) | That earlier SHA does not certify later November 10 merges, a full Pi installation or owner listening |

## Remaining acceptance

- The owner has not yet verified the Electron app on the Pi with the CM477/M8,
  live patching by ear, device recovery or the longer jam in #271.
- The actual 60-minute device-free Pi edit soak remains #296; queued, old or
  incomplete reports do not discharge it. Pi display/performance evidence remains separate.
- [Windows source setup](WINDOWS_SETUP.md) merged in #475; combined-main validation
  and an accepted install SHA remain pending. Downloadable installers, clean-VM validation,
  install/uninstall/rollback and release certification are not available yet.
- OSC slice B sidecar UDP/consent (#529), later Monitor presentation and remaining
  sequencer additions retain their own gates. MIDI Learn UI has merged (#500),
  but real-controller validation remains separate. The SDK is experimental;
  no ABI freeze or Daisy/plugin cross-build certification is claimed.
- Qt/ABI1 retirement is staged in [CLEANUP](planner/CLEANUP.md). Old tests and
  fixture coverage stay until their reviewed replacements land.

Required hosted checks are `guard`, `linux-arm64`, `windows-x64` and `sanitizers`,
plus task/Web/WASM/Pi evidence where applicable. Read the exact PR head and report
rather than inferring readiness from another revision or from this summary.
