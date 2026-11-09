<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Status and verification

Updated 2026-11-09. Wirescape is an alpha Electron application around Studio and
the native `wirescape-engine`. Historical checkpoint tables remain in Git
history. A source build, device-free test and owner hardware session establish
different things; the evidence below keeps those boundaries explicit.

| Scope | Current evidence | Boundary |
|---|---|---|
| Application foundation | Sidecar, RealEngine, Electron shell, patch codec/catalogue sync and live replan are merged; [architecture](ARCHITECTURE.md), [plan](PLAN.md) | The app uses the native engine over stdio; Studio owns lossless editing |
| Modules and patches | 28 ABI 2 catalogue entries, including Quantizer, Delay, Reverb, Spectral, Blend, Resonator and Groove S0; eight curated patches and shared patch library are merged | Open SEQ-16, Groove S1 and Interleave work is not implied complete |
| Windows protocol | [#439](https://github.com/kalharbi11/wirescape/pull/439) merged; protocol and golden tests execute on Windows | A skipped test is not passing evidence |
| Owner Windows session, 2026-11-09 | [#463](https://github.com/kalharbi11/wirescape/pull/463) branch: WASAPI headphones selectable, starters switch and sound, computer keyboard and Launchkey MIDI work at 48 kHz/256 | First-session branch evidence; final merge/CI/Pi gates and extended testing remain separate |
| Owner Pi session, 2026-11-07 | Headless playback and M8 MIDI, recorded in [DEMO_CLASS](DEMO_CLASS.md) | Does not certify the current Electron app on the Pi |
| Automated app/render evidence | Measured LFO phase telemetry, curated patch fixtures and device-free render evidence merged; [#442](https://github.com/kalharbi11/wirescape/pull/442) extends real Electron/sidecar tests | Fake MIDI/null audio tests do not establish physical audio/MIDI or Pi display performance |
| Pi source setup | [#450](https://github.com/kalharbi11/wirescape/pull/450), [#452](https://github.com/kalharbi11/wirescape/pull/452), then [#453](https://github.com/kalharbi11/wirescape/pull/453) are under validation | No common accepted SHA containing those changes and #463 is recorded yet |

## Remaining acceptance

- The owner has not yet verified the Electron app on the Pi with the CM477/M8,
  live patching by ear, device recovery or the longer jam in #271.
- The actual 60-minute device-free Pi edit soak remains #296; queued, old or
  incomplete reports do not discharge it. Pi display/performance evidence remains separate.
- [#475](https://github.com/kalharbi11/wirescape/pull/475) encodes the tested Windows
  source-build path. Downloadable installers, clean-VM distribution validation,
  install/uninstall/rollback and release certification are not available yet.
- OSC, MIDI Learn UI, the sequencer upgrades and remaining Phase 2 work retain
  their issue-specific gates. The SDK remains experimental; no ABI freeze or
  Daisy/plugin cross-build certification is claimed.
- Qt/ABI1 retirement is staged in [CLEANUP](planner/CLEANUP.md). Old tests and
  fixture coverage stay until their reviewed replacements land.

Required hosted checks are `guard`, `linux-arm64`, `windows-x64` and `sanitizers`,
plus task/Web/WASM/Pi evidence where applicable. Read the exact PR head and report
rather than inferring readiness from another revision or from this summary.
