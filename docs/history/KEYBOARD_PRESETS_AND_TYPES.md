# Keyboard routing, sound presets and discrete Surge parameter contracts

## Keyboard routing

Each *generator instance* carries two **user-interface** fields in the serialized patch:

- `keyboardArmed: boolean` — receive computer keyboard note events alongside other armed instances.
- `keyboardSolo: boolean` — temporarily limit computer keyboard note routing to this CHAOS. Only one keyboardSolo is kept by the editor's UI.

They do **not** modify the audio graph's typed ports or mute any incoming gate/MIDI/other audio. `web/client.js` maintains an `activeNotes` map from note number to the exact module IDs that received note-on; the matching note-off is delivered to those owners even after selection/arming changes. Computer-keyboard input is a frontend editing convenience, not MIDI input. The deep editor belongs to the selected instance independently of keyboard routing.

Future modules may provide their own keyboard-input flags, but must not rely on Wirescape's selected editing object for note targeting. A future MIDI interface should use a separate graph/MIDI routing policy.

## Preset storage

`wirescape/preset_store.py` defines a versioned `wirescape.chaos.preset` document with `name`, `mode`, `values`, `advanced`, `raw`. `user_data/chaos_presets/*.json` files are saved atomically, are not part of the git checkout, and are intentionally excluded from drop-in update archives.

Built-in presets live in `modules/chaos/presets/`. The original five contain only macro overrides and are read in legacy mode; the new examples use the full sound snapshot format.

The browser handles `preset_loaded` by updating all local settings and transmitting a single authoritative graph `patch` message to the host (the audio thread applies changes at the next buffer boundary). Saved Wirescape patches also store complete CHAOS settings directly; they do not depend on the availability of a user preset later.

## Raw parameter widgets

The Surge parameter snapshot (`modules/chaos/params.json`) provides **native minimum, maximum, default and type** for actual native controls. The external patch format retains normalized values in `[0,1]` for backward compatibility. UI editors render discrete types (`int` with bounded ranges, `bool`) as dropdowns. The dropdown sends normalized values to the host, which converts to Surge's native range and rounds integer controls. String/enum text from the local native Surge binding can be generated separately with `tools/scan_surge_choices.py`. For unknown index names, the label is numeric rather than guessed.

The schema also provides `macro_descriptions` and `front_descriptions` for tooltips. No raw control should claim to display the current engine value unless that current value has been directly queried from the actual Surge engine. An unedited control says **NO OVERRIDE**, and its slider/selector's initial position is a reference from the snapshot, not a live readback.

## Per-voice ADSR

`wirescape/mapping.py` exports `ENVELOPE_CONTROLS`, mapping front controls A/B ATTACK/DECAY/SUSTAIN/RELEASE to the actual `Amp EG` parameters of the two independent Surge engines. `wirescape/graph.py` routes a normalized write through the preexisting native-raw command queue when values change. If a RAW override targets the same parameter, it wins; when cleared the front ADSR value is reasserted. Changes happen at block boundaries, not per sample, and are not a separate Python envelope DSP processor.

## Sound modes and performance

CLUSTER, FM and NOISE are three parameter/routing scenes of **one architecture**. CLUSTER defaults to two notes per key; FM applies minimum FM and delay-bend interaction and unmutes B; NOISE enables an internal oscillator. None is claimed to be a custom VHIKK-style replacement engine. With chords, unison and many modules, total native Surge polyphony can be large. The existing host status meter can indicate increased DSP load, but real audio latency and CPU headroom must be measured on Windows and Pi 5.
