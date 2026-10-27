# Wirescape native foundation

Experimental native engine and Qt Quick app alongside the preserved legacy build.

Start from the repository root:

```powershell
python tools/dev.py build
python tools/dev.py test
python tools/dev.py ui-test
python tools/dev.py launch
```

Qt is optional for headless builds: `python tools/dev.py build --headless`.
Use a C++17 compiler, CMake 3.20+, and Qt 6.5+ Core/Gui/Qml/Quick/QuickControls2 plus Test for the app and its checks. The current local SDK is Qt 6.10.3 for MSVC x64. Linux/ARM portability is a target, not yet a verified result.

Each Essentials directory has its own source, manifest, and CMake entry. Shared libraries export `ws_module_entry`, defined by `include/wirescape/module.h`. Host construction/preparation allocates buffers; processing uses the prepared mono graph and bounded timestamp queue. The ABI remains experimental.

The app opens with a sine VCO → VCA → Output patch, with ADSR controlling VCA gain. Audio stays off until explicitly started. The native PortAudio adapter loads a backend library beside the executable or from `WIRESCAPE_PORTAUDIO`; no Python callback is involved. An installed backend does not establish live-audio validation.

See `docs/STATUS.md` for exact verified results, limitations, and the next bounded task. See `docs/history/FOUNDATION_WORKFLOW.md` for build/report commands. The project retains its root GPL license; vendored PortAudio header licensing is in `ui/third_party/PORTAUDIO_LICENSE.txt`.


## Current control contract

- Essentials include sine VCO, linear ADSR, VCA, two-input mono Mixer, Output and Scale/Offset control utility. Mixer performs a raw weighted sum with unpatched inputs silent and no clipping; default levels are 0.5 each.
- The default voice is silent after Start until Hold gate, Space on the canvas, or a keyboard note. Refresh discovers outputs before starting one. No automatic playback/restart is performed.
- Notes are monophonic last-on/retrigger, with no held-note fallback or sustain manager. ADSR release must match the current source/note identity. Manual gate uses a separate identity; the most recent gate/note-on owns the envelope. Releasing an older owner does nothing. Losing application focus panics.
- Saved Frequency is the base/manual setting. A played MIDI note selects absolute equal-tempered pitch until another note or Frequency change; it does not rewrite the saved knob. The knob is not an effective live-pitch meter. Note/gate timing is unsmoothed. Changes to VCO Frequency, VCA Gain, mixer levels and ADSR Sustain after processing starts use a shared linear 5 ms sample-count ramp; initial prepared settings apply immediately. Reset snaps to the last target and clears ramp progress.
- A render call examines at most 256 queue entries and processes at most 64 graph spans. Exceeding either budget silences the whole block, advances time, increments `overload_blocks`, invalidates queued controls and returns `WS_CAPACITY`. The device treats this as a processing fault and stops; restart is explicit. This fail-closed policy precedes any dense sequencer/sensor expansion, and is not a throughput claim.
- Engine samples stay unscaled. GUI/live output and CLI WAV export both apply the shared 0.2 host gain once, then clamp to [-1,1]. GUI test render remains 4 seconds mono with a 2-second gate; long releases can truncate. CLI demo has a different musical schedule, so it is not a matched waveform comparison.
- Native patch v1 accepts structurally valid incomplete graphs. Missing module payloads/unknown fields round-trip, execution stays disabled, and editing can recover readiness. Fully available graphs with unexpected compilation failures do not replace the accepted patch.

`python tools/dev.py package` creates and checks a local portable Windows folder; see the workflow/checkpoint for its validation boundary. Module DLLs remain trusted native code discovered by filename/export; manifests are not yet an enforced enablement/trust system. The ABI, including the extended experimental meter structure, is not stable.

Optional `native/document/` inspection tooling uses QtCore outside the Qt-free engine. It preserves original bytes and identifies evidenced document formats; it does not migrate or execute legacy patches. See `docs/history/DOCUMENT_READER_STAGE.md` for its verified inspection boundary.

Scale/Offset uses CONTROL input/output `control`: input times Scale (-2..2, default 1) plus Offset (-1..1, default 0), without clipping. An unpatched input is zero. Both parameters use the shared 5 ms ramp. See `docs/history/NATIVE_MODULE_AUTHORING.md` for the current experimental authoring contract.
