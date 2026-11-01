<!-- SPDX-License-Identifier: GPL-3.0-only -->

# Research: data-driven module panels

**Scope:** read-only audit of the 2026-11-01 `main` tree; proposed HTML/SVG front end; no ABI, DSP, patch, QML, build, or test changes. This is research, not approval to resume implementation. **Design target:** every loadable module, including an unfamiliar third-party module, gets an intelligible, usable panel without a per-module component.

## Executive finding

**Feasible, but the web catalog must expose the full native descriptor rather than just the current QML adapter.** ABI 2 already describes stable string IDs, names, signal/event port kinds and hints, stereo pairs, numeric bounds/defaults, tapers, steps, modulation flags and optional formatting interfaces. The SDK helper supplies zero named options and no custom value formatter by default, although the **ABI already supports** option arrays, display decimals and a value-to-text callback. A *non-executable, optional sidecar* supplies sections, semantic labels, formatting and control choices; missing or malformed sidecars get a deterministic generic panel. This changes neither the ABI header nor existing patch IDs. Evidence: native/include/wirescape/module2.h:23-29,37-46,94-108; native/sdk/wirescape_sdk.h:16-28; native/control/app_engine.cpp:26-31.

**Measured source inventory:** 19 ABI-2 built-in types, 2 independently built SDK example types, and 13 ABI-1 module implementations, **34 compiled descriptor definitions / 21 distinct module type IDs**. ABI-1 IDs overlap their ABI-2 ports; they are compatibility variants, not 13 new module types. The descriptor audit below totals **136 parameter entries and 103 port entries**, counting both ABI variants. The 19 built-ins appear in native/control/module_catalog.inc:2-20; SDK examples are built in native/engine2/CMakeLists.txt:76-81; ABI-1 sources are collected by native/CMakeLists.txt:34-40. These counts are **static source counts, not runtime enumeration or timing benchmarks**. An ABI-1 library can still be loaded but the app skips a duplicate ID: native/control/app_engine.cpp:50-68.


### Second-pass findings and corrections (2026-11-01)

The counts above remain unchanged. This pass verified that **34 definitions is not 34 types in the visible UI palette**: the host statically registers 19 native ABI-2 built-ins, the SDK examples build independently, and the external loader calls the ABI-1 loader; ABI-1 duplicates are skipped by type ID (native/control/module_catalog.inc:2-20; native/engine2/CMakeLists.txt:76-81; native/control/app_engine.cpp:26-31,50-68). A **generic HTML renderer** can support all 34 descriptor variants in offline fixtures, but a **plugin-capable web host** additionally requires an ABI-2 discovery/registration path. Do not describe that path as implemented.

Three independent identities must not be confused: (1) stable **module type string** (e.g. `essentials.vco`); (2) **patch-instance integer node ID**; (3) stable **ABI-2 parameter string ID**. Native processing also uses transient parameter array indices, while the legacy UI/patch adapter maps to numeric IDs through `legacy_index`. The existing C++ adapter constructs numeric IDs from `legacy_index` and its `app_engine_parameter_id()` resolves string IDs by descriptor index; patch-v2 conversion deliberately maps between the two (native/control/app_engine.cpp:26-31,118; native/document/app_patch.cpp:9-22,37-45). A web panel may display parameter slot and legacy index for debugging, but must **never serialize either as the patch-v2 target ID**.

A fourth distinction is essential: **descriptor completeness is not configuration completeness**. CC-to-CV has zero parameters yet accepts eight configurable rows; MIDI In also accepts endpoint text configuration; these values come from patch-v2 `module.config` and quiescent host adapters, not from `ws2_param` (native/modules2/midi_cc_to_cv/dsp.h:7-13; native/modules2/midi_cc_to_cv/config.h:7-19; native/document/runtime_module_config.h:10-31). A generic panel can display their ports from native metadata immediately, but **cannot truthfully expose these settings until a separate typed host-owned config schema/binding is provided**.

The current Qt module view is built from a **v1-shaped editor projection**, even when a native patch-v2 document is being edited; the conversion exists in `editor_view()` and `editor_patch()` (native/document/app_patch.cpp:16-56). The Qt view-side validator separately checks v1-shaped objects (native/ui/studio_controller.cpp:213-260). The runtime patch loader, by contrast, recognizes native v1/v2 and v2 `modulation` edges (native/document/runtime_patch.cpp:30-39,59-73). The HTML host should choose native patch-v2 documents as persistence authority, retaining the existing lossless conversion behavior rather than treating QML's view data as the saved format. **Preserving an unavailable module in a document is distinct from accepting it into an executable graph**; the runtime loader rejects unavailable/inactive modules (native/document/runtime_patch.cpp:49-54).

### Important limits

- The owner-described single-file “WIRESCAPE Studio 01” HTML sketch was **not supplied with this task**. The visual hierarchy below follows the described header/typed cables/knobs/“01 / MOTION” vocabulary, not verified pixel values or exact CSS from that sketch. Planner can supply the sketch before visual implementation.
- The ABI-2 `module.json` manifests list numeric legacy indices and coarse `signal`/`event` port types, **not** full descriptor hints/flags. The native `ws2_descriptor` and `ws2_param` remain authoritative (native/modules2/chaos_lite/module.json:7-45; native/modules2/chaos_lite/dsp.h:7-34; native/include/wirescape/module2.h:37-46).
- Numeric manifest indices must not be conflated with native ABI-2 `legacy_index`: the SDK helper's default legacy index is zero, including the macro-expanded Step Sequencer parameters. Use the string parameter ID to round-trip settings (native/sdk/wirescape_sdk.h:20-25; native/sdk/examples/step_sequencer/dsp.h:10-13,30-33).
- The ABI-2 structure **supports** `option_count`, `options`, `decimals` and `value_to_text`, but the current `ws::param` helper sets `option_count=0`, `options=nullptr`, `decimals=3` and `value_to_text=nullptr` for the audited built-ins/examples (native/include/wirescape/module2.h:41-46; native/sdk/wirescape_sdk.h:21-25). Some numeric mode semantics are documented in DSP, allocator enums and module READMEs, but **those labels are not supplied as machine-readable ABI options**. The appendix includes such **code-derived, cited enum labels** with an `enum_source` field; unknown mappings remain `enum_values:null`.
- **Do not infer audio rate from a generic “signal” or event from a port's name.** The ABI-2 hint and kind are separate (native/include/wirescape/module2.h:23-25,37-40). An ABI-1 control port is only `WS_CONTROL`, without a more precise hint or stereo pairing (native/include/wirescape/module.h:16-31).
- The current Qt UI is one shared `ModuleCard.qml`, not 34 bespoke panels. Its control-style function hardcodes selected numeric module IDs, and unknowns default to a slider (native/ui/qml/ModuleCard.qml:92-139). The special Clock run button is in native/ui/qml/ParameterControl.qml:183-195; Output selection is in native/ui/qml/ModuleCard.qml:141-147; per-app keyboard and device controls are in native/ui/qml/Main.qml:430-461. None is a module-specific QML file.

## Audited module list

In the following tables, **category is a proposed UI category**, not an existing native field; `I/O/P` is the count of declared input ports/output ports/parameters. Every port and parameter, with stable IDs, kinds, ranges, taper, flags, pair information and source lines, is serialized in the JSON appendix. The `source` value there identifies each original descriptor declaration. “Special today” means current shared QML or host UI behavior, not a separate QML component.

### ABI 2: built-in catalogue

| Module ID / display name | Proposed category | I/O/P | Special today; evidence |
| --- | --- | ---: | --- |
| `essentials.adsr` ADSR | Envelope | 2/1/4 | Sustain as knob, other controls sliders; native/modules2/adsr/dsp.h:6-10; native/ui/qml/ModuleCard.qml:98 |
| `essentials.chaos_lite` CHAOS-lite | Instrument | 4/2/26 | Stereo left/right with pair `stereo`; generic UI today; native/modules2/chaos_lite/dsp.h:7-34 |
| `essentials.clock` Clock | Timing | 1/1/4 | BPM knob, ticks stepper, run button; native/modules2/clock/dsp.h:6-8; native/ui/qml/ModuleCard.qml:97,105-106,136-137; native/ui/qml/ParameterControl.qml:183-195 |
| `essentials.function_envelope` Function Envelope | Envelope | 1/1/3 | Knob treatment by numeric ID; native/modules2/function_envelope/dsp.h:6-8; native/ui/qml/ModuleCard.qml:103 |
| `host.keyboard` Keyboard | MIDI / control | 0/4/3 | Generic card; host keyboard performance strip is app-level; native/modules2/host_keyboard/dsp.h:6-15; native/ui/qml/Main.qml:430-441 |
| `essentials.ladder` Ladder Filter | Filter | 1/1/3 | Knob treatment; native/modules2/ladder/dsp.h:6-8; native/ui/qml/ModuleCard.qml:102 |
| `essentials.lfo` LFO | Motion | 0/1/5 | Rate/depth/offset knobs; no verified DSP phase dot today; native/modules2/lfo/dsp.h:6-15; native/ui/qml/ModuleCard.qml:100 |
| `ports.macro_oscillator` Macro Oscillator | Instrument | 8/2/10 | 24 numeric models; generic card; native/modules2/macro_oscillator/dsp.h:9-30 |
| `midi.cc_to_cv` CC to CV | MIDI / control | 1/8/0 | Eight config-driven CC rows, not ABI params; native/modules2/midi_cc_to_cv/dsp.h:7-13; native/modules2/midi_cc_to_cv/config.h:7-21 |
| `midi.in` MIDI In | MIDI / control | 0/1/1 | Channel filter; physical endpoints live in global MIDI dialog; native/modules2/midi_in/dsp.h:7-10; native/ui/qml/Main.qml:166-187 |
| `midi.to_cv` MIDI to CV | MIDI / control | 1/8/9 | Mode/priority/legato controls require labels, currently generic; native/modules2/midi_to_cv/dsp.h:7-20 |
| `essentials.mixer` Mixer | Utility | 2/1/2 | Two faders; native/modules2/mixer/dsp.h:6-9; native/ui/qml/ModuleCard.qml:96 |
| `essentials.noise` Noise | Source | 0/1/1 | Generic slider; native/modules2/noise/dsp.h:6-8 |
| `essentials.output` Output | I/O | 1/1/0 | “Use this output” host selection action; not physical device selection; native/modules2/output/dsp.h:6-9; native/ui/qml/ModuleCard.qml:141-147 |
| `essentials.scale_offset` Scale/Offset | Utility | 1/1/2 | Scale/offset knobs; native/modules2/scale_offset/dsp.h:6-8; native/ui/qml/ModuleCard.qml:105 |
| `essentials.slew` Slew | Utility | 1/1/2 | Rise/fall knobs; native/modules2/slew/dsp.h:6-8; native/ui/qml/ModuleCard.qml:104 |
| `essentials.vca` VCA | Dynamics | 2/1/1 | Gain CV input; generic slider; native/modules2/vca/dsp.h:6-9 |
| `essentials.vcf` VCF | Filter | 1/2/2 | Low12 and Low24 are separate outputs, not a stereo pair; cutoff log taper; native/modules2/vcf/dsp.h:6-10 |
| `essentials.vco` VCO | Oscillator | 3/1/1 | Frequency knob; note-event input; native/modules2/vco/dsp.h:6-11; native/ui/qml/ModuleCard.qml:99 |

### ABI 2: SDK teaching modules

| Module ID / display name | Proposed category | I/O/P | Special; evidence |
| --- | --- | ---: | --- |
| `example.random_source` Random Source | Random / control | 1/2/3 | Clock-triggered sample-and-hold with smooth output; source native/sdk/examples/random_source/dsp.h:8-22; not in built-in catalog |
| `example.step_sequencer` Step Sequencer | Sequencer | 2/4/26 | Eight pitch/gate/velocity triples; gate values 0 rest, 1 note, 2 tie; native/sdk/examples/step_sequencer/dsp.h:7-40; not in built-in catalog |

### ABI 1: still compiled / compatibility definitions

ABI 1 declares numeric parameter IDs, min/max/default/unit and audio-or-control ports; **no ABI-2-like modulation flags, taper or stereo hints**, so the appendix represents absent properties as `null`, not inferred ABI-2 metadata (native/include/wirescape/module.h:16-31). These 13 folders have module build targets through native/CMakeLists.txt:35-41. Their stable module IDs intentionally match corresponding ABI-2 Essentials.

| ID suffix / name | Proposed category | I/O/P | Descriptor source |
| --- | --- | ---: | --- |
| `adsr` ADSR | Envelope | 0/1/4 | native/modules/adsr/module.cpp:95-100 |
| `clock` Clock | Timing | 1/1/4 | native/modules/clock/module.cpp:67-70 |
| `function_envelope` Function Envelope | Envelope | 1/1/3 | native/modules/function_envelope/module.cpp:74-77 |
| `ladder` Ladder Filter | Filter | 1/1/3 | native/modules/ladder/module.cpp:85-88 |
| `lfo` LFO | Motion | 0/1/3 | native/modules/lfo/module.cpp:54-58 |
| `mixer` Mixer | Utility | 2/1/2 | native/modules/mixer/module.cpp:63-74 |
| `noise` Noise | Source | 0/1/1 | native/modules/noise/module.cpp:49-52 |
| `output` Output | I/O | 1/1/0 | native/modules/output/module.cpp:16-18 |
| `scale_offset` Scale/Offset | Utility | 1/1/2 | native/modules/scale_offset/module.cpp:39-43 |
| `slew` Slew | Utility | 1/1/2 | native/modules/slew/module.cpp:56-59 |
| `vca` VCA | Dynamics | 2/1/1 | native/modules/vca/module.cpp:32-36 |
| `vcf` VCF | Filter | 1/2/2 | native/modules/vcf/module.cpp:85-88 |
| `vco` VCO | Oscillator | 0/1/1 | native/modules/vco/module.cpp:44-46 |

**Migration caveat:** a panel must distinguish ABI 1 from ABI 2 even when IDs match: ABI-1 VCO has no patchable pitch, FM or note-event ports, whereas ABI-2 VCO declares all three (native/modules/vco/module.cpp:44-46; native/modules2/vco/dsp.h:8-11). Never synthesize ports from an unrelated version.

### Verified mode semantics (not ABI option arrays)

The following labels are **derived from implementation and module documentation**, not populated ABI-2 `options` fields. Sidecar choices must preserve their numeric values: Keyboard/MIDI modes 0 Rotate, 1 Reuse, 2 Reset, 3 Mono; mono priority 0 Last, 1 Low, 2 High (native/voice/allocator.h:12-13; native/modules2/host_keyboard/README.md:14-16; native/modules2/midi_to_cv/README.md:8-11); MIDI channel filter 0 Omni and 1–16 channels (native/modules2/midi_in/dsp.h:26-41); CHAOS coupling route 0 Direct, 1 Multiply, 2 Delay (native/modules2/chaos_lite/coupler.h:13-26); CHAOS LFO target 0 Oscillator A pitch, 1 Body tuning, 2 Filter cutoff (native/modules2/chaos_lite/dsp.h:92-103); step gate 0 Rest, 1 Note, 2 Tie (native/sdk/examples/step_sequencer/dsp.h:8-13). **Macro Oscillator's 24 model names are not declared in the ABI descriptor**, so keep numeric 0–23 until names are independently verified (native/modules2/macro_oscillator/dsp.h:19-22).


### Descriptor coverage versus behavior/configuration coverage

| Case | Native data available today | Web-host requirement (proposed, not implemented) |
| --- | --- | --- |
| Ordinary ABI-2 controls | 108 parameter declarations across 21 ABI-2 types; 106 linear and 2 logarithmic tapers, 30 stepped, 78 modulatable, 0 flagged `WS2_HIDDEN`. Counts from the normalized audited descriptors; ABI flags/tapers are defined at native/include/wirescape/module2.h:27-28 and helper at native/sdk/wirescape_sdk.h:21-25. | Generic controls use native range/taper/flags; never reclassify a non-modulatable parameter by name |
| Dense layouts | CHAOS-lite and Step Sequencer each have 26 parameters (native/modules2/chaos_lite/dsp.h:17-34; native/sdk/examples/step_sequencer/dsp.h:9-13,30-33). | Sections, grids and vertical scrolling must remain keyboard- and screen-reader-reachable |
| Stereo and event routing | Exactly two declared `stereo_pair` ports, CHAOS left/right (native/modules2/chaos_lite/dsp.h:13-15); event signals retain a separate ABI kind (native/include/wirescape/module2.h:23-25). | Paired gesture yields **two** separately saved cables; event source cannot be sent to a signal port merely because colours match |
| CC-to-CV | Zero `ws2_param` values; eight configurable MIDI CC rows (native/modules2/midi_cc_to_cv/dsp.h:7-13; native/modules2/midi_cc_to_cv/config.h:7-19). | Provide host-certified `module.config.rows` schema/data and a generic eight-row form; otherwise display ports and “Configuration unavailable”, not fabricated knobs |
| MIDI In | Channel filter is a native param; `config.endpoint`/`system_name` are strings accepted by the runtime document adapter (native/modules2/midi_in/dsp.h:7-10; native/document/runtime_module_config.h:13-16). | Host device selector capability, permission status and persisted symbolic endpoint; the adapter returns an empty module config byte array, so **do not infer that the parameter descriptor alone performs physical device selection** |
| Output | Zero native parameters; a native sum input and audio output (native/modules2/output/dsp.h:6-9). | Expose a typed host action to select the graph output, separately from physical device selection; no device opens on render |
| Macro Oscillator | `prepare` rejects non-48000 sample rates and caps prepared voices at 8 (native/modules2/macro_oscillator/dsp.h:47-54). | Show **runtime-reported** availability and unsupported rate/voice warnings, never rewrite capability or claim offline renders pass at arbitrary rates |
| Runtime parameter edges | The v2 runtime recognizes `kind=modulation`, maps target stable parameter IDs to indices and validates signed `amount` (native/document/runtime_patch.cpp:59-66). | Separate parameter-edge targets from input-jack targets; always validate with host and preserve unknown nested JSON |

**Typed configuration contract, recommended:** use a host-supplied, versioned **declarative form schema**, not a per-module JavaScript renderer. For `midi.cc_to_cv`, a read-only schema can expose rows 1–8 and fields `channel` (integer 0–16, 0=Omni), `controller` (integer 0–127), `fourteen_bit` (boolean), `bipolar` (boolean), and `smoothing_ms` (finite 0–2000). When `fourteen_bit=true`, `controller<=31`. Defaults are channel 0, consecutive CC numbers 1–8, both booleans false and 5 ms smoothing. **Authoritative validation** remains in the existing quiescent JSON adapter and `cc_configuration` validator (native/document/runtime_module_config.h:17-32; native/modules2/midi_cc_to_cv/config.h:7-19). The renderer binds each field by exact `config.rows[index].field` path, preserves any other saved fields, and submits one typed transaction rather than writing the module's raw 68-byte struct.

The host can expose generic `actions[]` (stable action ID, label, enabled/disabled reason, confirmation/permission policy) for **graph-output selection, MIDI assignment, configuration editor and optionally transport**. The view does not call OS/device APIs, arbitrary module code or bundled scripts. It must not equate transport action capabilities with DSP parameters. `native/ui/qml/ModuleCard.qml:141-147` currently makes Output selection a special QML button; `native/ui/qml/Main.qml:166-187,430-461` hosts MIDI/audio services separately.

**Discovery and packaging:** the ABI-2 built-ins are statically enumerated, the two SDK teaching modules are separately built, and `app_engine_load_module()` currently delegates to the ABI-1 dynamic loader (native/control/module_catalog.inc:2-20; native/engine2/CMakeLists.txt:76-81; native/control/app_engine.cpp:59-65). Sidecars are **not currently installed automatically** by those ABI-2 build rules; adding adjacent source JSON will not expose it to a deployed Tauri/web process without a packaging/resource-discovery task (native/engine2/CMakeLists.txt:86-94,139,185; compare the ABI-1 manifest copy in native/CMakeLists.txt:27-32). For a production bridge, include `schema_version`, `abi_version`, `module_id` and **native descriptor module version** in each catalog snapshot; bind any optional sidecar to the correct module binary/provenance and fall back when a version mismatch or path escape occurs. A module label must never be treated as an executable/script path. A future external ABI-2 plugin loader is a distinct engine/SDK decision; the generic panel proposal does not authorize one.

**Licence boundary:** a per-module sidecar is presentation data shipped with the module and must retain that module's applicable licensing provenance. JSON does not accept an HTML comment-style SPDX header; before adding sidecars, the planner should approve a valid-JSON SPDX metadata convention and packaging tests without relaxing the repository's SPDX rule (AGENTS.md:69-85, native/sdk/examples/step_sequencer/dsp.h:1-4). No new sidecar file is created in this research PR.

## Missing presentation metadata: facts versus additive recommendations

| Feature | Observed native/Qt state | Additive sidecar proposal |
| --- | --- | --- |
| Category and family | ABI descriptors contain `id`, `name`, no `category` (native/include/wirescape/module2.h:94-98) | `category`, `family`, `accent_token`; never used as the persistent type ID |
| Sections and ordering | QML iterates descriptor arrays in order with a flat Flow (native/ui/qml/ModuleCard.qml:122-139) | Ordered `sections[]`, `items[]`, stable `param_id`, optional `port_order`; items not referenced remain in an automatic “Other” section |
| Visual emphasis | Hardcoded ID-based control styles and first-parameter priority in QML (native/ui/qml/ModuleCard.qml:92-121) | `widget: knob|small_knob|fader|stepper|toggle|segments|select|readout`, `size: hero|regular|compact`; renderer may override for accessibility |
| Short labels | Only long ABI names available (native/include/wirescape/module2.h:37-46) | `short_label`, `description`, `aria_label`; all labels use IDs for lookup, never position |
| Format of values and bounds | Host renders Hz and s with fixed rules; Qt tooltip uses 3 significant digits (native/ui/studio_controller.cpp:44-49; native/ui/qml/ParameterControl.qml:20-35) | `format: {kind, precision, significant_digits, scale, suffix, compact, choices}` separately for value and range; test exact 0.009999999776482582–20000 Hz regression described in issue #182 |
| Numeric modes | ABI has option pointers but SDK helper leaves options null (native/include/wirescape/module2.h:41-46; native/sdk/wirescape_sdk.h:20-28) | `choices: [{value,label,short_label}]` only for **verified** mapping; else numeric stepper and full source name |
| Colour and cable semantics | QML maps ports to coarse audio/control/event (native/ui/studio_controller.cpp:113-121; native/ui/qml/ModuleCard.qml:59-89) | Theme tokens keyed on ABI-2 kind/hint, with distinct event and stereo pair treatments; never block a link solely by hint |
| Advanced/hidden | ABI-2 `WS2_HIDDEN` exists (native/include/wirescape/module2.h:27); no generic accordion in the card | `visibility: primary|advanced|hidden`, badges for automation/modulation and search-reveal of advanced settings; never suppress valid param editing from inspector |
| Configuration and telemetry | MIDI CC row settings are configuration data, not parameters; LFO phase readout is not in its parameter array (native/modules2/midi_cc_to_cv/config.h:7-21; native/modules2/lfo/dsp.h:6-15) | Separate `settings` controls on a non-RT configuration bridge; `telemetry` bindings only when engine publishes truthful values |

### Sidecar location and authority

**Recommendation: `panel.json` per module** adjacent to each `module.json` (for SDK examples also adjacent to their manifest), installed/copied into the module catalogue. For ABI-1 compatibility use a separately keyed `abi_version:1` sidecar, never reuse the ABI-2 layout blindly. Example path: `native/modules2/chaos_lite/panel.json`. This is an **additive research proposal, not a file added by this PR**.

Why sidecar rather than descriptor: ABI-2 descriptor layout is in soft freeze; the C ABI has no section/widget/layout fields, and the SDK helper currently supplies only mechanical parameter info (native/include/wirescape/module2.h:41-46,94-108; native/sdk/wirescape_sdk.h:20-28). A separate, versioned declarative schema is themeable, localizable, reloadable off the audio thread and safe for future web hosts without editing ABI headers. `module.json` might instead carry a new optional `panel` object, but that mixes DSP packaging with visual policy; prefer a dedicated file. **Do not infer a missing sidecar as a bad module.**

Recommended overlay contract:

```json
{
  "schema_version": 1,
  "module_id": "essentials.lfo",
  "abi_version": 2,
  "category": "Motion",
  "accent_token": "motion",
  "sections": [
    {"id": "motion", "label": "01 / MOTION", "items": [
      {"param_id": "rate", "widget": "knob", "size": "hero",
       "short_label": "RATE",
       "format": {"kind": "frequency", "significant_digits": 3}},
      {"param_id": "depth", "widget": "small_knob", "short_label": "DEPTH"},
      {"param_id": "offset", "widget": "small_knob", "short_label": "OFFSET"},
      {"param_id": "sync", "widget": "toggle", "short_label": "SYNC"},
      {"param_id": "beats", "widget": "small_knob", "short_label": "BEATS"}
    ]}
  ],
  "telemetry": [{"key": "phase", "display": "phase_dot",
                 "requires": "verified_engine_telemetry"}]
}
```

**Merge protocol:** validate UTF-8, size and schema off the realtime thread; key by (`abi_version`, stable `module_id`, stable port/param string ID), never by array slot or QML node ID. Require distinct sections/items, unique matching IDs, valid choice values inside the authoritative range and step, and recognized theme tokens. On any invalid field, ignore that field and log only on the UI/control thread; on invalid entire sidecar fall back to native-only. Sidecar cannot redefine IDs, numeric bounds, units, default values, taper, signal hint, event kind, stereo pair, modulation capability, patch serialization, ownership or DSP callbacks. If a future plugin supplies its own sidecar, sanitize all labels as text; no HTML, JS, CSS, shell paths, images or arbitrary network URLs.

**Portability:** serialize module catalog and sidecar content once on a background/control worker, not every frame. The web renderer consumes an immutable snapshot plus bounded value/meter updates; C++ engine and UI talk over a versioned localhost/in-process bridge, with no LAN sockets in unattended tests. Module add/remove/patch operations must retain transactional engine behavior and unknown patch fields. The UI's render loop never calls a realtime DSP function or treats a displayed value as an authoritative control state. Existing safety rules: AGENTS.md:69-85; native/control/app_engine.cpp:26-31,67-69.


### Patch-v2 modulation targets and editor projection

The existing `docs/design/M3_PARAMETER_EDGES.md:7-40` defines a **proposal**, not blanket planner approval: a v2 connection with `kind: "modulation"` uses `from.port` as an output port string, `to.port` as the **target parameter string ID** (not a signal jack), and one signed `amount` in [-1,1]. The source describes multiple edges per parameter, zero amounts, and preserving unknown nested fields. Dragging to a knob is a distinct action from dragging the knob's base value; event outputs and parameters without `WS2_MODULATABLE` must not become drop targets (docs/design/M3_PARAMETER_EDGES.md:19-69; native/include/wirescape/module2.h:27; native/document/runtime_patch.cpp:59-66). Implementation is conditional on the planner's approval of that design.

**No “modulation port” may be invented in the native input port list.** The web UI should expose parameter-edge targets as separate typed drop zones associated with a parameter's stable string ID; keep routing geometry independent from port rows. The proposal's `effective = clamp(base_normalized + sum(amount * source_cv), 0, 1)` then applies taper; per-voice telemetry must identify a chosen voice or a bounded range, never pretend every voice has one effective value (docs/design/M3_PARAMETER_EDGES.md:64-103). Tooltip, saved base value and formatted current effective value remain separate; hide/stale-clear the overlay if data is not published. Live graph editing is gated by a separate safe graph-publication task, not by HTML drag success (docs/design/M3_PARAMETER_EDGES.md:105-124).

### Serialization and bridge invariants

- **Patch document** is a native v2 value tree with stable type/parameter/port IDs, settings, config, state, outputs and preserved unknown members. Maintain the existing v1-to-v2 migration/backups and native-v2 source-of-truth; never persist the v1-shaped `StudioController::modules()` projection by mistake (native/document/app_patch.cpp:16-56; native/document/patch_document.h:14-24; native/document/README.md:4-17).
- **UI view model** uses node instance integer ID only to dispatch an edit and find its on-screen card. When `ws2_param.legacy_index` differs from the array index, use the existing host mapping, **not** an `id: Number(p.id)` assumption (native/control/app_engine.cpp:26-31,118; native/document/app_patch.cpp:9-12,39-45). An old ABI-1 module still uses its numeric IDs in v1 compatibility data.
- **Commands** should carry an expected patch revision/generation, typed action and stable IDs. The host validates availability, module config, modulation authorization, range and graph feasibility; only a confirmed result updates “accepted” UI values. On rejection, the displayed value returns to the last confirmed state. This is a proposed bridge contract, not an existing API.
- **Unknown extensions** must survive edits and undo/redo at their original nesting level. `docs/design/M3_PARAMETER_EDGES.md:34-52` explicitly distinguishes inactive/unavailable storage from executable graphs. Browser JSON parsing alone is not enough to reproduce the native reader's duplicate-key and 2 MiB limits (native/document/runtime_patch.cpp:29-37; native/document/README.md:4-17); delegate document validation/serialization to C++.
- **Port positions** must be observed from actual rendered jack elements after layout and zoom, not reconstructed from a hardcoded y-offset. Current QML port rows use 112px + 24px × index (native/ui/qml/ModuleCard.qml:59-79), while canvas endpoint geometry is independently calculated (native/ui/qml/Main.qml:127-135); sections/collapsible controls must not change routing endpoints incorrectly. Zoom 0.35–1 and pointer hit testing are relevant current Qt behavior, not necessarily the final web spec (native/ui/qml/Main.qml:155-158).
- **Accessibility**: screen-reader labels include node display name, module type, port/parameter stable ID, kind/hint and current value; keyboard can create/disconnect ordinary and permitted modulation edges. Drag/scroll do not steal text entry, and touch hit areas do not shrink below reasonable device-pixel size when canvas zoom changes. These are proposed acceptance rules.

## Generic automatic layout contract

1. **Header:** slim module-index/category eyebrow; native display name; optional short description, bypass/selection/status actions from host capabilities (not fake DSP params). Example `01 / MOTION` as a section caption, not a hardcoded module name. Unique graph node ID and stable module type ID stay separate. For missing modules show an explicit unavailable header with original ports/values retained in the patch.
2. **Ports:** left rail = descriptor inputs, right rail = descriptor outputs, in native order unless a valid sidecar order exists. Each jack carries label, kind (`signal`/`event`), hint (audio/pitch/CV/gate/trigger/MIDI/notes), poly policy, unpatched default and stereo pairing. Match paired L/R jacks visually but keep **two addressable ports** and two saved cables; never reinterpret voices as stereo (native/modules2/chaos_lite/dsp.h:13-15; native/include/wirescape/module2.h:37-40). Keyboard focus exposes every jack and typed cable route. **Signal hints are advisory, not type gates; event-vs-signal kinds remain distinct**, as checked by the runtime loader (native/document/runtime_patch.cpp:64-66).
3. **Body:** ordered sections from `sections[]`; otherwise natural native parameter order in a responsive two-column grid. Section captions `01 / MOTION`, `02 / SHAPE`, etc. Header/footer and left/right port rails remain aligned for all heights. At 280px panel width, use 24px minimum port-row pitch and keep knobs at touch-accessible hit areas; adapt card width for dense 26-param modules instead of forcing microscopic controls. Respect zoom for pointer hit testing and cable endpoints.
4. **Controls:** continuous gets accessible knob by default, primary/hero for single frequency or rate; secondary gets small knob. Step >0 / stepped flag gets an integer stepper; two-state uses toggle; **buttons/segments only with verified choice labels**. Fixed range (`minimum==maximum`) gets disabled readout. For an ABI-1 parameter, use linear slider/knob with explicit range; no claim of native modulation. For 0-param modules display ports, description and any approved host action rather than an empty broken card.
5. **Display:** separate *numeric plain value*, *normalized value*, *formatted value* and *formatted range*. Format float32 bounds with bounded decimal digits; e.g. `0.01 Hz–20 kHz`, not `0.009999999776482582–20000 Hz`. For Hz use Hz/kHz; seconds use ms/s; show unit once when unambiguous. **Formatting precedence:** trusted native `value_to_text` callback invoked only on the control side when supplied, then validated `options`/step choices, then sidecar formatting hint, then deterministic descriptor `decimals`/unit fallback. The current SDK helper leaves callback/options empty and supplies decimals=3 (native/include/wirescape/module2.h:41-46; native/sdk/wirescape_sdk.h:21-25); this code-derived case uses fallback. Use descriptor taper (linear/log/exp) and step for interaction, matching SDK conversion functions, and never infer a log taper solely from unit or ratio (native/sdk/wirescape_sdk.h:26-45; current Qt heuristic at native/ui/qml/ParameterControl.qml:14-17). Tooltip, accessibility and keyboard entry share one formatting implementation; UI formatting never changes the DSP min/max/default. Issue #182 reports the long VCO range string; the **currently inspected Qt code already attempts three-significant-digit formatting** (native/ui/qml/ParameterControl.qml:20-35), so the historical screenshot alone does not establish reproduction on this exact head.
6. **Modulation display:** show base value and effective modulation arc/dot only with authenticated engine telemetry; otherwise hide it. A separate LFO phase dot requires a phase datum or equivalent truthful source, no independently ticking CSS animation. Host-supplied telemetry can be throttled (suggest initial ≤30 updates/s, subject to Pi profiling), never sampled with allocations/locks inside `process`. Validate staleness and clear on disconnect/reset.
7. **Overflow and interactions:** advanced foldout retains controls and keyboard search; section and port counts capped by descriptor limits (32 ports per direction, 32 params in ABI 2; native/include/wirescape/module2.h:17-20). Pan/zoom operates in world coordinates; drag capture, cable hit targets and selection are independent. JSON import/export stores stable IDs, accepted numeric values and unknown extension fields without formatting side effects. Output device/transport permissions remain a **host service action**, not fictitious module data.
8. **Unknown/invalid descriptors:** stable native name, ports and each numeric parameter produce a stock panel. No anonymous sliders where IDs exist; no crashes when `sidecar` absent, stale or from a different ABI. Duplicate string IDs or malformed native descriptors cause catalogue-level quarantine, not a silently remapped patch.

### Priority for hand-designed panels *later*

A complete generic panel remains mandatory even for these. Distinctive future visuals are progressive enhancement, never the only way to interact.

| Priority | Module | Why a custom visualization may earn its maintenance cost |
| --- | --- | --- |
| P1 | CHAOS-lite | 26 parameters across twin oscillators, coupling, body, instability and LFO; named mode/route controls and paired stereo output merit organized signal-flow diagram (native/modules2/chaos_lite/dsp.h:9-34). |
| P1 | Step Sequencer | 8×3 pattern grid and moving clock position improve editing over 26 generic controls; keep all 26 controls accessible in the generic inspector (native/sdk/examples/step_sequencer/dsp.h:7-41). |
| P1 | Macro Oscillator | 24 model choices plus harmonics/timbre/morph and CV attenuverters benefit from model selector and dynamic semantics, but model labels require verified names (native/modules2/macro_oscillator/dsp.h:19-30). |
| P1 | MIDI to CV / CC to CV | Voice allocation and 8 configurable CC mappings warrant table-based control-panel UI, but CC mappings live in control/config data, not the ordinary param descriptor (native/modules2/midi_to_cv/dsp.h:10-20; native/modules2/midi_cc_to_cv/config.h:7-21). |
| P2 | LFO, ADSR, Function Envelope | A truthful phase dot and envelope curve visualization improve readability if telemetry exists; ordinary sliders/knobs remain fully functional (native/modules2/lfo/dsp.h:6-15; native/modules2/adsr/dsp.h:7-10; native/modules2/function_envelope/dsp.h:6-8). |
| P2 | Mixer and Output | Channel strips/meters and host output routing, not complex parameter widgets (native/modules2/mixer/dsp.h:7-9; native/modules2/output/dsp.h:7-9). |
| No dedicated panel yet | VCO, VCF, VCA, Noise, Slew, Scale/Offset, Ladder, Clock, MIDI In, Keyboard, Random Source | Native metadata plus sidecar sections/labels are adequate; add host-approved status/transport widgets only as separate capabilities. |

## Implementation and acceptance proposal (no code in this PR)

**Prototype boundary:** implement one `GenericPanel(descriptor, presentation?, values, telemetry?, hostCapabilities)`; *zero* hardcoded module-ID branches in the generic renderer. Apply the 19 ABI-2 built-ins and two examples as fixtures, then 13 ABI-1 compatibility variants. Output screenshot/DOM snapshots from static catalog JSON with fake read-only values, and deterministic mouse/keyboard tests with an in-memory host. No physical audio/MIDI or LAN access in tests; no Pi/Windows hardware promise from a browser prototype. All 34 must render without exceptions, missing controls or cross-wired IDs.

**Contract acceptance:** run a generated schema/collision test that compares normalized descriptors to actual native entries; independently check every ID, kind/hint, min/max/default, taper, flag, and stereo pair; check unknown sidecar fields ignored, malformed/absent sidecars fallback, values round-trip through JSON unchanged, unknown patch entries are lossless. Include a regression for `Frequency · 0.009999999776482582–20000 Hz` and tests of Hz/kHz and ms/s boundaries. Assert no duplicate per-module JSX/HTML panel source is required. For runtime port reconfiguration, get actual `query_ports` results per configuration rather than treating the static table as immutable (native/include/wirescape/module2.h:94-101; native/sdk/wirescape_sdk.h:166-180).

**Performance budget (proposal, not benchmark):** benchmark catalog parse and initial render on Windows and Pi 5 with 1, 19 and 100 node instances; record decoded JSON bytes, first-contentful panel time, p50/p95 update frame time, patch/drag latency and JS heap. Initial targets: no audio-thread sidecar work, ≤16.7ms UI render budget at 60Hz (or measured 30Hz fallback on Pi), ≤30Hz live visual telemetry and no per-animation catalog reparse. This research performs **zero** runtime performance measurements.


---

## Appendix A — complete native descriptor data (JSON)

This is normalized **research data**, not a replacement for runtime enumeration. Each record has `abi_version`, `module_id`, `display_name`, `category_proposed`, `source`, `ports` (ID, direction, signal/event kind, hint, poly, stereo pairing, unpatched value, source line), `parameters` (ABI-2 string ID and legacy index, or ABI-1 numeric ID; name, bounds, unit, taper, default, step, flag list, modulatable, enum labels when verified, source), and `special_ui`. ABI-2 entries also include native `module_version`, source-backed default `decimals=3`, zero `option_count`, empty `native_options` and `has_value_to_text=false`; ABI 1 has null for these absent fields (native/sdk/wirescape_sdk.h:21-25,253-257; native/include/wirescape/module2.h:41-46). `enum_values` are independently source-derived UI candidates, **not** descriptor-provided ABI options. `null` means *not declared or not verified*, not zero/false. Source lines point to the declaration, including macro definitions for the expanded step array. For ABI-2 parameters, numeric manifest indices are ordinal slots and **do not necessarily equal** the descriptor’s `legacy_index` (SDK helper default is zero). Stable native identity is the string ID (native/sdk/wirescape_sdk.h:20-25; native/sdk/examples/step_sequencer/dsp.h:10-13,30-33). Categories, section ideas and generic UI badges are proposals.

```json
[
  {
    "abi_version": 2,
    "module_id": "essentials.adsr",
    "display_name": "ADSR",
    "category_proposed": "Envelope",
    "source": "native/modules2/adsr/dsp.h:L6",
    "ports": [
      {
        "id": "gate",
        "name": "Gate",
        "direction": "input",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/adsr/dsp.h:L7"
      },
      {
        "id": "notes",
        "name": "Notes",
        "direction": "input",
        "kind": "event",
        "hint": "ev_notes",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/adsr/dsp.h:L7"
      },
      {
        "id": "envelope",
        "name": "Envelope",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/adsr/dsp.h:L8"
      }
    ],
    "parameters": [
      {
        "id": "attack",
        "legacy_index": 0,
        "name": "Attack",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.01,
        "unit": "s",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/adsr/dsp.h:L10",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "decay",
        "legacy_index": 1,
        "name": "Decay",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.1,
        "unit": "s",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/adsr/dsp.h:L10",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "sustain",
        "legacy_index": 2,
        "name": "Sustain",
        "minimum": 0,
        "maximum": 1,
        "default": 0.7,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "automatable",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/adsr/dsp.h:L10",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "release",
        "legacy_index": 3,
        "name": "Release",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.2,
        "unit": "s",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/adsr/dsp.h:L10",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Sustain knob, other sliders (native/ui/qml/ModuleCard.qml:98)",
    "module_version": 2,
    "version_source": "native/modules2/adsr/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.chaos_lite",
    "display_name": "CHAOS-lite",
    "category_proposed": "Instrument",
    "source": "native/modules2/chaos_lite/dsp.h:L9",
    "ports": [
      {
        "id": "pitch",
        "name": "Pitch",
        "direction": "input",
        "kind": "signal",
        "hint": "pitch",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/chaos_lite/dsp.h:L11"
      },
      {
        "id": "gate",
        "name": "Gate",
        "direction": "input",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 1,
        "source": "native/modules2/chaos_lite/dsp.h:L11"
      },
      {
        "id": "velocity",
        "name": "Velocity",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 1,
        "source": "native/modules2/chaos_lite/dsp.h:L12"
      },
      {
        "id": "fm",
        "name": "FM",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/chaos_lite/dsp.h:L12"
      },
      {
        "id": "left",
        "name": "Left",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": "stereo",
        "unpatched_value": 0,
        "source": "native/modules2/chaos_lite/dsp.h:L14"
      },
      {
        "id": "right",
        "name": "Right",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": "stereo",
        "unpatched_value": 0,
        "source": "native/modules2/chaos_lite/dsp.h:L15"
      }
    ],
    "parameters": [
      {
        "id": "shape_a",
        "legacy_index": 0,
        "name": "A shape",
        "minimum": 0,
        "maximum": 2,
        "default": 0.4,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L18",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "shape_b",
        "legacy_index": 1,
        "name": "B shape",
        "minimum": 0,
        "maximum": 2,
        "default": 1.6,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L18",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "detune",
        "legacy_index": 2,
        "name": "B detune",
        "minimum": -60,
        "maximum": 60,
        "default": 7,
        "unit": "cent",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L19",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "octave_a",
        "legacy_index": 3,
        "name": "A octave",
        "minimum": -3,
        "maximum": 3,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L20",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "octave_b",
        "legacy_index": 4,
        "name": "B octave",
        "minimum": -3,
        "maximum": 3,
        "default": -1,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L21",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "level_a",
        "legacy_index": 5,
        "name": "A level",
        "minimum": 0,
        "maximum": 1,
        "default": 0.55,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L22",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "level_b",
        "legacy_index": 6,
        "name": "B level",
        "minimum": 0,
        "maximum": 1,
        "default": 0.35,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L22",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pan_a",
        "legacy_index": 7,
        "name": "A pan",
        "minimum": -1,
        "maximum": 1,
        "default": -0.3,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L23",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pan_b",
        "legacy_index": 8,
        "name": "B pan",
        "minimum": -1,
        "maximum": 1,
        "default": 0.3,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L23",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "route",
        "legacy_index": 9,
        "name": "Route",
        "minimum": 0,
        "maximum": 2,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Direct"
          },
          {
            "value": 1,
            "label": "Multiply"
          },
          {
            "value": 2,
            "label": "Delay"
          }
        ],
        "source": "native/modules2/chaos_lite/dsp.h:L24",
        "enum_source": "native/modules2/chaos_lite/dsp.h:24,106-109; native/modules2/chaos_lite/coupler.h:13-26",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "depth",
        "legacy_index": 10,
        "name": "Coupling",
        "minimum": 0,
        "maximum": 1,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L25",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "body_tune",
        "legacy_index": 11,
        "name": "Body tune",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L25",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "feedback",
        "legacy_index": 12,
        "name": "Body feedback",
        "minimum": 0,
        "maximum": 0.92,
        "default": 0.3,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L26",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "damping",
        "legacy_index": 13,
        "name": "Body damping",
        "minimum": 0,
        "maximum": 0.99,
        "default": 0.4,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L26",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "body_mix",
        "legacy_index": 14,
        "name": "Body mix",
        "minimum": 0,
        "maximum": 1,
        "default": 0.25,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L27",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "roughness",
        "legacy_index": 15,
        "name": "Roughness",
        "minimum": 0,
        "maximum": 1,
        "default": 0.1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L27",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "rough_rate",
        "legacy_index": 16,
        "name": "Roughness rate",
        "minimum": 1,
        "maximum": 150,
        "default": 70,
        "unit": "Hz",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L28",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "drift",
        "legacy_index": 17,
        "name": "Drift",
        "minimum": 0,
        "maximum": 1,
        "default": 0.1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L29",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "jitter",
        "legacy_index": 18,
        "name": "Jitter",
        "minimum": 0,
        "maximum": 1,
        "default": 0.02,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L29",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "fall",
        "legacy_index": 19,
        "name": "Fall",
        "minimum": 0,
        "maximum": 1,
        "default": 0,
        "unit": "oct/s",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L30",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "lfo_rate",
        "legacy_index": 20,
        "name": "LFO rate",
        "minimum": 0.02,
        "maximum": 12,
        "default": 0.3,
        "unit": "Hz",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L30",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "lfo_depth",
        "legacy_index": 21,
        "name": "LFO depth",
        "minimum": 0,
        "maximum": 1,
        "default": 0.1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L31",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "lfo_target",
        "legacy_index": 22,
        "name": "LFO target",
        "minimum": 0,
        "maximum": 2,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Oscillator A pitch"
          },
          {
            "value": 1,
            "label": "Body tuning"
          },
          {
            "value": 2,
            "label": "Filter cutoff"
          }
        ],
        "source": "native/modules2/chaos_lite/dsp.h:L32",
        "enum_source": "native/modules2/chaos_lite/dsp.h:92-103",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "lfo_self",
        "legacy_index": 23,
        "name": "LFO self",
        "minimum": 0,
        "maximum": 1,
        "default": 0.2,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L33",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "cutoff",
        "legacy_index": 24,
        "name": "Cutoff",
        "minimum": 20,
        "maximum": 20000,
        "default": 9000,
        "unit": "Hz",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L33",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "level",
        "legacy_index": 25,
        "name": "Output level",
        "minimum": 0,
        "maximum": 1,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/chaos_lite/dsp.h:L34",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Generic card; stereo output pair is native (dsp.h:13-15)",
    "module_version": 1,
    "version_source": "native/modules2/chaos_lite/dsp.h:L9"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.clock",
    "display_name": "Clock",
    "category_proposed": "Timing",
    "source": "native/modules2/clock/dsp.h:L6",
    "ports": [
      {
        "id": "reset",
        "name": "Reset",
        "direction": "input",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/clock/dsp.h:L7"
      },
      {
        "id": "clock",
        "name": "Clock",
        "direction": "output",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/clock/dsp.h:L7"
      }
    ],
    "parameters": [
      {
        "id": "bpm",
        "legacy_index": 0,
        "name": "BPM",
        "minimum": 20,
        "maximum": 300,
        "default": 120,
        "unit": "BPM",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/clock/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "ticks_per_beat",
        "legacy_index": 1,
        "name": "Ticks per beat",
        "minimum": 1,
        "maximum": 24,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/clock/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "duty",
        "legacy_index": 2,
        "name": "Duty",
        "minimum": 0.01,
        "maximum": 0.99,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/clock/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "run",
        "legacy_index": 3,
        "name": "Run",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": [
          {
            "value": 0,
            "label": "Stop"
          },
          {
            "value": 1,
            "label": "Run"
          }
        ],
        "source": "native/modules2/clock/dsp.h:L8",
        "enum_source": "native/modules2/clock/dsp.h:8; native/ui/qml/ParameterControl.qml:183-195",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Ticks stepper + run button (native/ui/qml/ModuleCard.qml:97,136-137; native/ui/qml/ParameterControl.qml:183-195)",
    "module_version": 2,
    "version_source": "native/modules2/clock/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.function_envelope",
    "display_name": "Function Envelope",
    "category_proposed": "Envelope",
    "source": "native/modules2/function_envelope/dsp.h:L6",
    "ports": [
      {
        "id": "gate",
        "name": "Gate",
        "direction": "input",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/function_envelope/dsp.h:L7"
      },
      {
        "id": "envelope",
        "name": "Envelope",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/function_envelope/dsp.h:L7"
      }
    ],
    "parameters": [
      {
        "id": "rise",
        "legacy_index": 0,
        "name": "Rise",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.01,
        "unit": "s",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/function_envelope/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "fall",
        "legacy_index": 1,
        "name": "Fall",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.1,
        "unit": "s",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/function_envelope/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "curve",
        "legacy_index": 2,
        "name": "Curve",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/function_envelope/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Shared QML generic module card",
    "module_version": 2,
    "version_source": "native/modules2/function_envelope/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "host.keyboard",
    "display_name": "Keyboard",
    "category_proposed": "MIDI / control",
    "source": "native/modules2/host_keyboard/dsp.h:L7",
    "ports": [
      {
        "id": "pitch",
        "name": "Pitch",
        "direction": "output",
        "kind": "signal",
        "hint": "pitch",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/host_keyboard/dsp.h:L9"
      },
      {
        "id": "gate",
        "name": "Gate",
        "direction": "output",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/host_keyboard/dsp.h:L9"
      },
      {
        "id": "velocity",
        "name": "Velocity",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/host_keyboard/dsp.h:L10"
      },
      {
        "id": "notes",
        "name": "Notes",
        "direction": "output",
        "kind": "event",
        "hint": "ev_notes",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/host_keyboard/dsp.h:L11"
      }
    ],
    "parameters": [
      {
        "id": "voices",
        "legacy_index": 0,
        "name": "Voices",
        "minimum": 1,
        "maximum": 16,
        "default": 8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/modules2/host_keyboard/dsp.h:L13",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "mode",
        "legacy_index": 1,
        "name": "Mode",
        "minimum": 0,
        "maximum": 3,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rotate"
          },
          {
            "value": 1,
            "label": "Reuse"
          },
          {
            "value": 2,
            "label": "Reset"
          },
          {
            "value": 3,
            "label": "Mono"
          }
        ],
        "source": "native/modules2/host_keyboard/dsp.h:L14",
        "enum_source": "native/voice/allocator.h:12; native/modules2/host_keyboard/README.md:14-16",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "octave",
        "legacy_index": 2,
        "name": "Octave",
        "minimum": -3,
        "maximum": 3,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/modules2/host_keyboard/dsp.h:L15",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Global keyboard strip in Main.qml:430-441; generic card",
    "module_version": 1,
    "version_source": "native/modules2/host_keyboard/dsp.h:L7"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.ladder",
    "display_name": "Ladder Filter",
    "category_proposed": "Filter",
    "source": "native/modules2/ladder/dsp.h:L6",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/ladder/dsp.h:L7"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/ladder/dsp.h:L7"
      }
    ],
    "parameters": [
      {
        "id": "cutoff",
        "legacy_index": 0,
        "name": "Cutoff",
        "minimum": 20,
        "maximum": 20000,
        "default": 1000,
        "unit": "Hz",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/ladder/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "resonance",
        "legacy_index": 1,
        "name": "Resonance",
        "minimum": 0,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/ladder/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "drive",
        "legacy_index": 2,
        "name": "Drive",
        "minimum": 0,
        "maximum": 2,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/ladder/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Knobs on shared ModuleCard.qml:102",
    "module_version": 2,
    "version_source": "native/modules2/ladder/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.lfo",
    "display_name": "LFO",
    "category_proposed": "Motion",
    "source": "native/modules2/lfo/dsp.h:L6",
    "ports": [
      {
        "id": "control",
        "name": "Control",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/lfo/dsp.h:L8"
      }
    ],
    "parameters": [
      {
        "id": "rate",
        "legacy_index": 0,
        "name": "Rate",
        "minimum": 0.01,
        "maximum": 20,
        "default": 1,
        "unit": "Hz",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "audio_rate_ok",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/lfo/dsp.h:L11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "depth",
        "legacy_index": 1,
        "name": "Depth",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "audio_rate_ok",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/lfo/dsp.h:L12",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "offset",
        "legacy_index": 2,
        "name": "Offset",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "audio_rate_ok",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/lfo/dsp.h:L13",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "sync",
        "legacy_index": 3,
        "name": "Tempo sync",
        "minimum": 0,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Off"
          },
          {
            "value": 1,
            "label": "On"
          }
        ],
        "source": "native/modules2/lfo/dsp.h:L14",
        "enum_source": "native/modules2/lfo/dsp.h:14-15",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "beats",
        "legacy_index": 4,
        "name": "Beats/cycle",
        "minimum": 0.125,
        "maximum": 16,
        "default": 1,
        "unit": "beats",
        "taper": "log",
        "curve": 4,
        "step": 0,
        "flags": [
          "automatable",
          "modulatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/lfo/dsp.h:L15",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Rate/depth/offset knobs (native/ui/qml/ModuleCard.qml:100); no phase telemetry defined",
    "module_version": 2,
    "version_source": "native/modules2/lfo/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "ports.macro_oscillator",
    "display_name": "Macro Oscillator",
    "category_proposed": "Instrument",
    "source": "native/modules2/macro_oscillator/dsp.h:L9",
    "ports": [
      {
        "id": "pitch",
        "name": "Pitch",
        "direction": "input",
        "kind": "signal",
        "hint": "pitch",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L10"
      },
      {
        "id": "model_cv",
        "name": "Model CV",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L11"
      },
      {
        "id": "fm",
        "name": "FM",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L12"
      },
      {
        "id": "harmonics_cv",
        "name": "Harmonics",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L13"
      },
      {
        "id": "timbre_cv",
        "name": "Timbre",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L14"
      },
      {
        "id": "morph_cv",
        "name": "Morph",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L15"
      },
      {
        "id": "trigger",
        "name": "Trigger",
        "direction": "input",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L16"
      },
      {
        "id": "level",
        "name": "Level",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L17"
      },
      {
        "id": "main",
        "name": "Main",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L18"
      },
      {
        "id": "aux",
        "name": "Aux",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/macro_oscillator/dsp.h:L18"
      }
    ],
    "parameters": [
      {
        "id": "model",
        "legacy_index": 0,
        "name": "Model",
        "minimum": 0,
        "maximum": 23,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L21",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "coarse",
        "legacy_index": 1,
        "name": "Coarse",
        "minimum": -48,
        "maximum": 48,
        "default": 0,
        "unit": "st",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L22",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "harmonics",
        "legacy_index": 2,
        "name": "Harmonics",
        "minimum": 0,
        "maximum": 1,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L23",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "timbre",
        "legacy_index": 3,
        "name": "Timbre",
        "minimum": 0,
        "maximum": 1,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L24",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "morph",
        "legacy_index": 4,
        "name": "Morph",
        "minimum": 0,
        "maximum": 1,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L25",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "fm_amount",
        "legacy_index": 5,
        "name": "FM amount",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L26",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "timbre_amount",
        "legacy_index": 6,
        "name": "Timbre amount",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L27",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "morph_amount",
        "legacy_index": 7,
        "name": "Morph amount",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L28",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "decay",
        "legacy_index": 8,
        "name": "Decay",
        "minimum": 0,
        "maximum": 1,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L29",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "lpg_colour",
        "legacy_index": 9,
        "name": "LPG colour",
        "minimum": 0,
        "maximum": 1,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/macro_oscillator/dsp.h:L30",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "24-model selector numeric; no custom QML (dsp.h:19-30)",
    "module_version": 1,
    "version_source": "native/modules2/macro_oscillator/dsp.h:L9"
  },
  {
    "abi_version": 2,
    "module_id": "midi.cc_to_cv",
    "display_name": "CC to CV",
    "category_proposed": "MIDI / control",
    "source": "native/modules2/midi_cc_to_cv/dsp.h:L7",
    "ports": [
      {
        "id": "midi",
        "name": "MIDI",
        "direction": "input",
        "kind": "event",
        "hint": "ev_midi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L8"
      },
      {
        "id": "cc1",
        "name": "CC 1",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      },
      {
        "id": "cc2",
        "name": "CC 2",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      },
      {
        "id": "cc3",
        "name": "CC 3",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      },
      {
        "id": "cc4",
        "name": "CC 4",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      },
      {
        "id": "cc5",
        "name": "CC 5",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      },
      {
        "id": "cc6",
        "name": "CC 6",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      },
      {
        "id": "cc7",
        "name": "CC 7",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      },
      {
        "id": "cc8",
        "name": "CC 8",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_cc_to_cv/dsp.h:L9"
      }
    ],
    "parameters": [],
    "special_ui": "Eight rows in non-parameter config.h:7-21; global MIDI dialog in Main.qml:166-187",
    "module_version": 1,
    "version_source": "native/modules2/midi_cc_to_cv/dsp.h:L7"
  },
  {
    "abi_version": 2,
    "module_id": "midi.in",
    "display_name": "MIDI In",
    "category_proposed": "MIDI / control",
    "source": "native/modules2/midi_in/dsp.h:L7",
    "ports": [
      {
        "id": "midi",
        "name": "MIDI",
        "direction": "output",
        "kind": "event",
        "hint": "ev_midi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_in/dsp.h:L9"
      }
    ],
    "parameters": [
      {
        "id": "channel_filter",
        "legacy_index": 0,
        "name": "Channel (0 = Omni)",
        "minimum": 0,
        "maximum": 16,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Omni"
          },
          {
            "value": 1,
            "label": "Channel 1"
          },
          {
            "value": 2,
            "label": "Channel 2"
          },
          {
            "value": 3,
            "label": "Channel 3"
          },
          {
            "value": 4,
            "label": "Channel 4"
          },
          {
            "value": 5,
            "label": "Channel 5"
          },
          {
            "value": 6,
            "label": "Channel 6"
          },
          {
            "value": 7,
            "label": "Channel 7"
          },
          {
            "value": 8,
            "label": "Channel 8"
          },
          {
            "value": 9,
            "label": "Channel 9"
          },
          {
            "value": 10,
            "label": "Channel 10"
          },
          {
            "value": 11,
            "label": "Channel 11"
          },
          {
            "value": 12,
            "label": "Channel 12"
          },
          {
            "value": 13,
            "label": "Channel 13"
          },
          {
            "value": 14,
            "label": "Channel 14"
          },
          {
            "value": 15,
            "label": "Channel 15"
          },
          {
            "value": 16,
            "label": "Channel 16"
          }
        ],
        "source": "native/modules2/midi_in/dsp.h:L10",
        "enum_source": "native/modules2/midi_in/dsp.h:10,26-41",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Physical MIDI selection belongs to Main.qml:166-187",
    "module_version": 1,
    "version_source": "native/modules2/midi_in/dsp.h:L7"
  },
  {
    "abi_version": 2,
    "module_id": "midi.to_cv",
    "display_name": "MIDI to CV",
    "category_proposed": "MIDI / control",
    "source": "native/modules2/midi_to_cv/dsp.h:L7",
    "ports": [
      {
        "id": "midi",
        "name": "MIDI",
        "direction": "input",
        "kind": "event",
        "hint": "ev_midi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L8"
      },
      {
        "id": "pitch",
        "name": "Pitch",
        "direction": "output",
        "kind": "signal",
        "hint": "pitch",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      },
      {
        "id": "gate",
        "name": "Gate",
        "direction": "output",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      },
      {
        "id": "velocity",
        "name": "Velocity",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      },
      {
        "id": "aftertouch",
        "name": "Aftertouch",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      },
      {
        "id": "pressure",
        "name": "Pressure",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      },
      {
        "id": "bend",
        "name": "Bend",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      },
      {
        "id": "mod",
        "name": "Mod",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      },
      {
        "id": "notes",
        "name": "Notes",
        "direction": "output",
        "kind": "event",
        "hint": "ev_notes",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/midi_to_cv/dsp.h:L9"
      }
    ],
    "parameters": [
      {
        "id": "voice_count",
        "legacy_index": 0,
        "name": "Voices",
        "minimum": 1,
        "maximum": 16,
        "default": 4,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/modules2/midi_to_cv/dsp.h:L12",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "allocation_mode",
        "legacy_index": 1,
        "name": "Mode (rotate/reuse/reset/mono)",
        "minimum": 0,
        "maximum": 3,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rotate"
          },
          {
            "value": 1,
            "label": "Reuse"
          },
          {
            "value": 2,
            "label": "Reset"
          },
          {
            "value": 3,
            "label": "Mono"
          }
        ],
        "source": "native/modules2/midi_to_cv/dsp.h:L13",
        "enum_source": "native/voice/allocator.h:12; native/modules2/midi_to_cv/README.md:8-11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "steal_policy",
        "legacy_index": 2,
        "name": "Steal (oldest)",
        "minimum": 0,
        "maximum": 0,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Oldest"
          }
        ],
        "source": "native/modules2/midi_to_cv/dsp.h:L14",
        "enum_source": "native/modules2/midi_to_cv/dsp.h:14; native/modules2/midi_to_cv/README.md:8-10",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "mono_priority",
        "legacy_index": 3,
        "name": "Priority (last/low/high)",
        "minimum": 0,
        "maximum": 2,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Last"
          },
          {
            "value": 1,
            "label": "Low"
          },
          {
            "value": 2,
            "label": "High"
          }
        ],
        "source": "native/modules2/midi_to_cv/dsp.h:L15",
        "enum_source": "native/voice/allocator.h:13; native/modules2/midi_to_cv/README.md:8-11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "legato",
        "legacy_index": 4,
        "name": "Legato",
        "minimum": 0,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Off"
          },
          {
            "value": 1,
            "label": "On"
          }
        ],
        "source": "native/modules2/midi_to_cv/dsp.h:L16",
        "enum_source": "native/modules2/midi_to_cv/dsp.h:16; native/modules2/midi_to_cv/README.md:8-11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "sustain",
        "legacy_index": 6,
        "name": "Sustain (ignore/hold)",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Ignore"
          },
          {
            "value": 1,
            "label": "Hold"
          }
        ],
        "source": "native/modules2/midi_to_cv/dsp.h:L17",
        "enum_source": "native/modules2/midi_to_cv/dsp.h:17; native/modules2/midi_to_cv/README.md:8-11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch_bend_range_semitones",
        "legacy_index": 7,
        "name": "Bend range",
        "minimum": 0,
        "maximum": 48,
        "default": 2,
        "unit": "st",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/modules2/midi_to_cv/dsp.h:L18",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "apply_bend",
        "legacy_index": 8,
        "name": "Apply bend to pitch",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Off"
          },
          {
            "value": 1,
            "label": "On"
          }
        ],
        "source": "native/modules2/midi_to_cv/dsp.h:L19",
        "enum_source": "native/modules2/midi_to_cv/dsp.h:19; native/modules2/midi_to_cv/README.md:8-11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "channel_filter",
        "legacy_index": 9,
        "name": "Channel (0 = Omni)",
        "minimum": 0,
        "maximum": 16,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Omni"
          },
          {
            "value": 1,
            "label": "Channel 1"
          },
          {
            "value": 2,
            "label": "Channel 2"
          },
          {
            "value": 3,
            "label": "Channel 3"
          },
          {
            "value": 4,
            "label": "Channel 4"
          },
          {
            "value": 5,
            "label": "Channel 5"
          },
          {
            "value": 6,
            "label": "Channel 6"
          },
          {
            "value": 7,
            "label": "Channel 7"
          },
          {
            "value": 8,
            "label": "Channel 8"
          },
          {
            "value": 9,
            "label": "Channel 9"
          },
          {
            "value": 10,
            "label": "Channel 10"
          },
          {
            "value": 11,
            "label": "Channel 11"
          },
          {
            "value": 12,
            "label": "Channel 12"
          },
          {
            "value": 13,
            "label": "Channel 13"
          },
          {
            "value": 14,
            "label": "Channel 14"
          },
          {
            "value": 15,
            "label": "Channel 15"
          },
          {
            "value": 16,
            "label": "Channel 16"
          }
        ],
        "source": "native/modules2/midi_to_cv/dsp.h:L20",
        "enum_source": "native/modules2/midi_to_cv/dsp.h:20; native/modules2/midi_to_cv/README.md:8-11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "No custom panel; mode/allocation flags are stepped (dsp.h:10-20)",
    "module_version": 1,
    "version_source": "native/modules2/midi_to_cv/dsp.h:L7"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.mixer",
    "display_name": "Mixer",
    "category_proposed": "Utility",
    "source": "native/modules2/mixer/dsp.h:L6",
    "ports": [
      {
        "id": "a",
        "name": "A",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/mixer/dsp.h:L7"
      },
      {
        "id": "b",
        "name": "B",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/mixer/dsp.h:L7"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/mixer/dsp.h:L8"
      }
    ],
    "parameters": [
      {
        "id": "level_a",
        "legacy_index": 0,
        "name": "Level A",
        "minimum": 0,
        "maximum": 2,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/mixer/dsp.h:L9",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "level_b",
        "legacy_index": 1,
        "name": "Level B",
        "minimum": 0,
        "maximum": 2,
        "default": 0.5,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/mixer/dsp.h:L9",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Twin faders (native/ui/qml/ModuleCard.qml:96)",
    "module_version": 2,
    "version_source": "native/modules2/mixer/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.noise",
    "display_name": "Noise",
    "category_proposed": "Source",
    "source": "native/modules2/noise/dsp.h:L6",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/noise/dsp.h:L7"
      }
    ],
    "parameters": [
      {
        "id": "level",
        "legacy_index": 0,
        "name": "Level",
        "minimum": 0,
        "maximum": 1,
        "default": 0.2,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/noise/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Shared QML generic module card",
    "module_version": 2,
    "version_source": "native/modules2/noise/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.output",
    "display_name": "Output",
    "category_proposed": "I/O",
    "source": "native/modules2/output/dsp.h:L6",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": "sum",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/output/dsp.h:L7"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/output/dsp.h:L8"
      }
    ],
    "parameters": [],
    "special_ui": "Host output select button in ModuleCard.qml:141-147",
    "module_version": 2,
    "version_source": "native/modules2/output/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.scale_offset",
    "display_name": "Scale/Offset",
    "category_proposed": "Utility",
    "source": "native/modules2/scale_offset/dsp.h:L6",
    "ports": [
      {
        "id": "control",
        "name": "Control",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/scale_offset/dsp.h:L7"
      },
      {
        "id": "control",
        "name": "Control",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/scale_offset/dsp.h:L7"
      }
    ],
    "parameters": [
      {
        "id": "scale",
        "legacy_index": 0,
        "name": "Scale",
        "minimum": -2,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/scale_offset/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "offset",
        "legacy_index": 1,
        "name": "Offset",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/scale_offset/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Knob rule in ModuleCard.qml:105",
    "module_version": 2,
    "version_source": "native/modules2/scale_offset/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.slew",
    "display_name": "Slew",
    "category_proposed": "Utility",
    "source": "native/modules2/slew/dsp.h:L6",
    "ports": [
      {
        "id": "control",
        "name": "Control",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/slew/dsp.h:L7"
      },
      {
        "id": "control",
        "name": "Control",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/slew/dsp.h:L7"
      }
    ],
    "parameters": [
      {
        "id": "rise",
        "legacy_index": 0,
        "name": "Rise",
        "minimum": 0,
        "maximum": 10,
        "default": 0.1,
        "unit": "s/unit",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/slew/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "fall",
        "legacy_index": 1,
        "name": "Fall",
        "minimum": 0,
        "maximum": 10,
        "default": 0.1,
        "unit": "s/unit",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable",
          "per_voice",
          "audio_rate_ok"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/slew/dsp.h:L8",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Knob rule in ModuleCard.qml:104",
    "module_version": 2,
    "version_source": "native/modules2/slew/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.vca",
    "display_name": "VCA",
    "category_proposed": "Dynamics",
    "source": "native/modules2/vca/dsp.h:L6",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vca/dsp.h:L7"
      },
      {
        "id": "gain_cv",
        "name": "Gain CV",
        "direction": "input",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 1,
        "source": "native/modules2/vca/dsp.h:L7"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vca/dsp.h:L8"
      }
    ],
    "parameters": [
      {
        "id": "gain",
        "legacy_index": 0,
        "name": "Gain",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "audio_rate_ok",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/vca/dsp.h:L9",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Shared QML generic module card",
    "module_version": 2,
    "version_source": "native/modules2/vca/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.vcf",
    "display_name": "VCF",
    "category_proposed": "Filter",
    "source": "native/modules2/vcf/dsp.h:L6",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vcf/dsp.h:L7"
      },
      {
        "id": "low12",
        "name": "Low12",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vcf/dsp.h:L8"
      },
      {
        "id": "low24",
        "name": "Low24",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vcf/dsp.h:L8"
      }
    ],
    "parameters": [
      {
        "id": "cutoff",
        "legacy_index": 0,
        "name": "Cutoff",
        "minimum": 20,
        "maximum": 20000,
        "default": 1000,
        "unit": "Hz",
        "taper": "log",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "audio_rate_ok",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/vcf/dsp.h:L10",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "resonance",
        "legacy_index": 1,
        "name": "Resonance",
        "minimum": 0,
        "maximum": 0.95,
        "default": 0,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "audio_rate_ok",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/vcf/dsp.h:L10",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Cutoff/resonance knobs in ModuleCard.qml:101",
    "module_version": 2,
    "version_source": "native/modules2/vcf/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "essentials.vco",
    "display_name": "VCO",
    "category_proposed": "Oscillator",
    "source": "native/modules2/vco/dsp.h:L6",
    "ports": [
      {
        "id": "pitch",
        "name": "Pitch",
        "direction": "input",
        "kind": "signal",
        "hint": "pitch",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vco/dsp.h:L8"
      },
      {
        "id": "fm",
        "name": "FM Hz",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vco/dsp.h:L8"
      },
      {
        "id": "notes",
        "name": "Notes",
        "direction": "input",
        "kind": "event",
        "hint": "ev_notes",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vco/dsp.h:L9"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/modules2/vco/dsp.h:L10"
      }
    ],
    "parameters": [
      {
        "id": "frequency",
        "legacy_index": 0,
        "name": "Frequency",
        "minimum": 0.01,
        "maximum": 20000,
        "default": 220,
        "unit": "Hz",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "per_voice",
          "audio_rate_ok",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/modules2/vco/dsp.h:L11",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "Frequency hero knob in ModuleCard.qml:99,112-114",
    "module_version": 2,
    "version_source": "native/modules2/vco/dsp.h:L6"
  },
  {
    "abi_version": 2,
    "module_id": "example.random_source",
    "display_name": "Random Source",
    "category_proposed": "Random / control",
    "source": "native/sdk/examples/random_source/dsp.h:L8",
    "ports": [
      {
        "id": "clock",
        "name": "Clock",
        "direction": "input",
        "kind": "signal",
        "hint": "trigger",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/random_source/dsp.h:L12"
      },
      {
        "id": "step",
        "name": "Step",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/random_source/dsp.h:L14"
      },
      {
        "id": "smooth",
        "name": "Smooth",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_bi",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/random_source/dsp.h:L15"
      }
    ],
    "parameters": [
      {
        "id": "voices",
        "legacy_index": 0,
        "name": "Voices",
        "minimum": 1,
        "maximum": 16,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/sdk/examples/random_source/dsp.h:L20",
        "slot": 0,
        "manifest_index": 0,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "smooth",
        "legacy_index": 0,
        "name": "Smooth",
        "minimum": 0,
        "maximum": 1,
        "default": 0.05,
        "unit": "s",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/random_source/dsp.h:L21",
        "slot": 1,
        "manifest_index": 1,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "range",
        "legacy_index": 0,
        "name": "Range",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/random_source/dsp.h:L22",
        "slot": 2,
        "manifest_index": 2,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "No custom QML; SDK example not in builtin catalog (native/control/module_catalog.inc:2-20)",
    "module_version": 1,
    "version_source": "native/sdk/examples/random_source/dsp.h:L8"
  },
  {
    "abi_version": 2,
    "module_id": "example.step_sequencer",
    "display_name": "Step Sequencer",
    "category_proposed": "Sequencer",
    "source": "native/sdk/examples/step_sequencer/dsp.h:L16",
    "ports": [
      {
        "id": "clock",
        "name": "Clock",
        "direction": "input",
        "kind": "signal",
        "hint": "trigger",
        "poly": "first",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L20"
      },
      {
        "id": "reset",
        "name": "Reset",
        "direction": "input",
        "kind": "signal",
        "hint": "trigger",
        "poly": "first",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L21"
      },
      {
        "id": "pitch",
        "name": "Pitch",
        "direction": "output",
        "kind": "signal",
        "hint": "pitch",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L24"
      },
      {
        "id": "gate",
        "name": "Gate",
        "direction": "output",
        "kind": "signal",
        "hint": "gate",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L25"
      },
      {
        "id": "velocity",
        "name": "Velocity",
        "direction": "output",
        "kind": "signal",
        "hint": "cv_uni",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L26"
      },
      {
        "id": "notes",
        "name": "Notes",
        "direction": "output",
        "kind": "event",
        "hint": "ev_notes",
        "poly": "poly",
        "stereo_pair": null,
        "unpatched_value": 0,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L27"
      }
    ],
    "parameters": [
      {
        "id": "run",
        "legacy_index": 0,
        "name": "Run",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Off"
          },
          {
            "value": 1,
            "label": "On"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L31",
        "slot": 0,
        "manifest_index": 0,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:31-33",
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "length",
        "legacy_index": 0,
        "name": "Length",
        "minimum": 1,
        "maximum": 8,
        "default": 8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L32",
        "slot": 1,
        "manifest_index": 1,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch1",
        "legacy_index": 0,
        "name": "Pitch 1",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 2,
        "manifest_index": 2,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate1",
        "legacy_index": 0,
        "name": "Gate 1",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 3,
        "manifest_index": 3,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel1",
        "legacy_index": 0,
        "name": "Velocity 1",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 4,
        "manifest_index": 4,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch2",
        "legacy_index": 0,
        "name": "Pitch 2",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 5,
        "manifest_index": 5,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate2",
        "legacy_index": 0,
        "name": "Gate 2",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 6,
        "manifest_index": 6,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel2",
        "legacy_index": 0,
        "name": "Velocity 2",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 7,
        "manifest_index": 7,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch3",
        "legacy_index": 0,
        "name": "Pitch 3",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 8,
        "manifest_index": 8,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate3",
        "legacy_index": 0,
        "name": "Gate 3",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 9,
        "manifest_index": 9,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel3",
        "legacy_index": 0,
        "name": "Velocity 3",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 10,
        "manifest_index": 10,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch4",
        "legacy_index": 0,
        "name": "Pitch 4",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 11,
        "manifest_index": 11,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate4",
        "legacy_index": 0,
        "name": "Gate 4",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 12,
        "manifest_index": 12,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel4",
        "legacy_index": 0,
        "name": "Velocity 4",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 13,
        "manifest_index": 13,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch5",
        "legacy_index": 0,
        "name": "Pitch 5",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 14,
        "manifest_index": 14,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate5",
        "legacy_index": 0,
        "name": "Gate 5",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 15,
        "manifest_index": 15,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel5",
        "legacy_index": 0,
        "name": "Velocity 5",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 16,
        "manifest_index": 16,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch6",
        "legacy_index": 0,
        "name": "Pitch 6",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 17,
        "manifest_index": 17,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate6",
        "legacy_index": 0,
        "name": "Gate 6",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 18,
        "manifest_index": 18,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel6",
        "legacy_index": 0,
        "name": "Velocity 6",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 19,
        "manifest_index": 19,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch7",
        "legacy_index": 0,
        "name": "Pitch 7",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 20,
        "manifest_index": 20,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate7",
        "legacy_index": 0,
        "name": "Gate 7",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 21,
        "manifest_index": 21,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel7",
        "legacy_index": 0,
        "name": "Velocity 7",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 22,
        "manifest_index": 22,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "pitch8",
        "legacy_index": 0,
        "name": "Pitch 8",
        "minimum": -2,
        "maximum": 2,
        "default": 0,
        "unit": "oct",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L11 (WS_STEP macro),L33",
        "slot": 23,
        "manifest_index": 23,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      },
      {
        "id": "gate8",
        "legacy_index": 0,
        "name": "Gate 8",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 1,
        "flags": [
          "stepped",
          "automatable"
        ],
        "modulatable": false,
        "enum_values": [
          {
            "value": 0,
            "label": "Rest"
          },
          {
            "value": 1,
            "label": "Note"
          },
          {
            "value": 2,
            "label": "Tie"
          }
        ],
        "source": "native/sdk/examples/step_sequencer/dsp.h:L9,L12 (WS_STEP macro),L33",
        "slot": 24,
        "manifest_index": 24,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false,
        "enum_source": "native/sdk/examples/step_sequencer/dsp.h:7-13"
      },
      {
        "id": "vel8",
        "legacy_index": 0,
        "name": "Velocity 8",
        "minimum": 0,
        "maximum": 1,
        "default": 0.8,
        "unit": "",
        "taper": "linear",
        "curve": 4,
        "step": 0,
        "flags": [
          "modulatable",
          "automatable"
        ],
        "modulatable": true,
        "enum_values": null,
        "source": "native/sdk/examples/step_sequencer/dsp.h:L13 (WS_STEP macro),L33",
        "slot": 25,
        "manifest_index": 25,
        "decimals": 3,
        "option_count": 0,
        "native_options": [],
        "has_value_to_text": false
      }
    ],
    "special_ui": "No custom QML; 8×3 pitch/gate/velocity array expanded from WS_STEP, with documented gate meanings (dsp.h:7-41)",
    "module_version": 1,
    "version_source": "native/sdk/examples/step_sequencer/dsp.h:L16"
  },
  {
    "abi_version": 1,
    "module_id": "essentials.adsr",
    "display_name": "ADSR",
    "category_proposed": "Envelope",
    "source": "native/modules/adsr/module.cpp:L100",
    "ports": [
      {
        "id": "envelope",
        "name": "Envelope",
        "direction": "output",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/adsr/module.cpp:L95"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Attack",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.01,
        "unit": "s",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/adsr/module.cpp:L96",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Decay",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.1,
        "unit": "s",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/adsr/module.cpp:L97",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 2,
        "slot": 2,
        "name": "Sustain",
        "minimum": 0,
        "maximum": 1,
        "default": 0.7,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/adsr/module.cpp:L98",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 3,
        "slot": 3,
        "name": "Release",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.2,
        "unit": "s",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/adsr/module.cpp:L99",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.clock",
    "display_name": "Clock",
    "category_proposed": "Timing",
    "source": "native/modules/clock/module.cpp:L70",
    "ports": [
      {
        "id": "reset",
        "name": "Reset",
        "direction": "input",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/clock/module.cpp:L67"
      },
      {
        "id": "clock",
        "name": "Clock",
        "direction": "output",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/clock/module.cpp:L68"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "BPM",
        "minimum": 20,
        "maximum": 300,
        "default": 120,
        "unit": "BPM",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/clock/module.cpp:L69",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Ticks per beat",
        "minimum": 1,
        "maximum": 24,
        "default": 1,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/clock/module.cpp:L69",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 2,
        "slot": 2,
        "name": "Duty",
        "minimum": 0.01,
        "maximum": 0.99,
        "default": 0.5,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/clock/module.cpp:L69",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 3,
        "slot": 3,
        "name": "Run",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/clock/module.cpp:L69",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.function_envelope",
    "display_name": "Function Envelope",
    "category_proposed": "Envelope",
    "source": "native/modules/function_envelope/module.cpp:L77",
    "ports": [
      {
        "id": "gate",
        "name": "Gate",
        "direction": "input",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/function_envelope/module.cpp:L74"
      },
      {
        "id": "envelope",
        "name": "Envelope",
        "direction": "output",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/function_envelope/module.cpp:L75"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Rise",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.01,
        "unit": "s",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/function_envelope/module.cpp:L76",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Fall",
        "minimum": 0.0001,
        "maximum": 30,
        "default": 0.1,
        "unit": "s",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/function_envelope/module.cpp:L76",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 2,
        "slot": 2,
        "name": "Curve",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/function_envelope/module.cpp:L76",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.ladder",
    "display_name": "Ladder Filter",
    "category_proposed": "Filter",
    "source": "native/modules/ladder/module.cpp:L88",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/ladder/module.cpp:L85"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/ladder/module.cpp:L86"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Cutoff",
        "minimum": 20,
        "maximum": 20000,
        "default": 1000,
        "unit": "Hz",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/ladder/module.cpp:L87",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Resonance",
        "minimum": 0,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/ladder/module.cpp:L87",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 2,
        "slot": 2,
        "name": "Drive",
        "minimum": 0,
        "maximum": 2,
        "default": 0,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/ladder/module.cpp:L87",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.lfo",
    "display_name": "LFO",
    "category_proposed": "Motion",
    "source": "native/modules/lfo/module.cpp:L58",
    "ports": [
      {
        "id": "control",
        "name": "Control",
        "direction": "output",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/lfo/module.cpp:L54"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Rate",
        "minimum": 0.01,
        "maximum": 20,
        "default": 1,
        "unit": "Hz",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/lfo/module.cpp:L55",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Depth",
        "minimum": 0,
        "maximum": 1,
        "default": 1,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/lfo/module.cpp:L56",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 2,
        "slot": 2,
        "name": "Offset",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/lfo/module.cpp:L57",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.mixer",
    "display_name": "Mixer",
    "category_proposed": "Utility",
    "source": "native/modules/mixer/module.cpp:L74",
    "ports": [
      {
        "id": "a",
        "name": "A",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/mixer/module.cpp:L64"
      },
      {
        "id": "b",
        "name": "B",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/mixer/module.cpp:L65"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/mixer/module.cpp:L68"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Level A",
        "minimum": 0,
        "maximum": 2,
        "default": 0.5,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/mixer/module.cpp:L71",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Level B",
        "minimum": 0,
        "maximum": 2,
        "default": 0.5,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/mixer/module.cpp:L72",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.noise",
    "display_name": "Noise",
    "category_proposed": "Source",
    "source": "native/modules/noise/module.cpp:L52",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/noise/module.cpp:L50"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Level",
        "minimum": 0,
        "maximum": 1,
        "default": 0.2,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/noise/module.cpp:L51",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.output",
    "display_name": "Output",
    "category_proposed": "I/O",
    "source": "native/modules/output/module.cpp:L18",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/output/module.cpp:L16"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/output/module.cpp:L17"
      }
    ],
    "parameters": [],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.scale_offset",
    "display_name": "Scale/Offset",
    "category_proposed": "Utility",
    "source": "native/modules/scale_offset/module.cpp:L43",
    "ports": [
      {
        "id": "control",
        "name": "Control",
        "direction": "input",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/scale_offset/module.cpp:L39"
      },
      {
        "id": "control",
        "name": "Control",
        "direction": "output",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/scale_offset/module.cpp:L40"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Scale",
        "minimum": -2,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/scale_offset/module.cpp:L41",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Offset",
        "minimum": -1,
        "maximum": 1,
        "default": 0,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/scale_offset/module.cpp:L42",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.slew",
    "display_name": "Slew",
    "category_proposed": "Utility",
    "source": "native/modules/slew/module.cpp:L59",
    "ports": [
      {
        "id": "control",
        "name": "Control",
        "direction": "input",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/slew/module.cpp:L56"
      },
      {
        "id": "control",
        "name": "Control",
        "direction": "output",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/slew/module.cpp:L57"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Rise",
        "minimum": 0,
        "maximum": 10,
        "default": 0.1,
        "unit": "s/unit",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/slew/module.cpp:L58",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Fall",
        "minimum": 0,
        "maximum": 10,
        "default": 0.1,
        "unit": "s/unit",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/slew/module.cpp:L58",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.vca",
    "display_name": "VCA",
    "category_proposed": "Dynamics",
    "source": "native/modules/vca/module.cpp:L36",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/vca/module.cpp:L32"
      },
      {
        "id": "gain_cv",
        "name": "Gain CV",
        "direction": "input",
        "kind": "signal",
        "hint": "control",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/vca/module.cpp:L33"
      },
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/vca/module.cpp:L34"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Gain",
        "minimum": 0,
        "maximum": 2,
        "default": 1,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/vca/module.cpp:L35",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.vcf",
    "display_name": "VCF",
    "category_proposed": "Filter",
    "source": "native/modules/vcf/module.cpp:L88",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "input",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/vcf/module.cpp:L85"
      },
      {
        "id": "low12",
        "name": "Low12",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/vcf/module.cpp:L86"
      },
      {
        "id": "low24",
        "name": "Low24",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/vcf/module.cpp:L86"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Cutoff",
        "minimum": 20,
        "maximum": 20000,
        "default": 1000,
        "unit": "Hz",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/vcf/module.cpp:L87",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      },
      {
        "id": 1,
        "slot": 1,
        "name": "Resonance",
        "minimum": 0,
        "maximum": 0.95,
        "default": 0,
        "unit": "",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/vcf/module.cpp:L87",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  },
  {
    "abi_version": 1,
    "module_id": "essentials.vco",
    "display_name": "VCO",
    "category_proposed": "Oscillator",
    "source": "native/modules/vco/module.cpp:L46",
    "ports": [
      {
        "id": "audio",
        "name": "Audio",
        "direction": "output",
        "kind": "signal",
        "hint": "audio",
        "poly": null,
        "stereo_pair": null,
        "unpatched_value": null,
        "source": "native/modules/vco/module.cpp:L44"
      }
    ],
    "parameters": [
      {
        "id": 0,
        "slot": 0,
        "name": "Frequency",
        "minimum": 0.01,
        "maximum": 20000,
        "default": 220,
        "unit": "Hz",
        "taper": null,
        "curve": null,
        "step": null,
        "flags": null,
        "modulatable": null,
        "enum_values": null,
        "source": "native/modules/vco/module.cpp:L45",
        "decimals": null,
        "option_count": null,
        "native_options": null,
        "has_value_to_text": null
      }
    ],
    "special_ui": "No module-specific QML; shared ModuleCard.qml with ABI1 compatibility descriptors",
    "module_version": null,
    "version_source": null
  }
]
```

## Decisions needed from the planner

1. **Presentation storage:** (A) adjacent `panel.json` per module (recommended); (B) additive `presentation` object in `module.json`; (C) embed into C descriptor (not recommended under soft freeze). **Recommend A**.
2. **Host catalog API:** (A) expose the full ABI-2 descriptor + sidecar in a versioned JSON control snapshot (recommended); (B) extend the existing ABI-1-adapter JSON with fields; (C) introspect from JS. **Recommend A**, keeping native-only UI fallback.
3. **Enum labels and missing semantics:** (A) explicit validated sidecar choices backed by code-reviewed mappings (recommended); (B) numeric-only steppers; (C) infer labels from parameter names. **Recommend A** where verified, B otherwise; never C.
4. **Visual fidelity:** (A) obtain and attach the original “WIRESCAPE Studio 01” sketch then lock tokens/spacing (recommended); (B) approximate from this layout spec. **Recommend A** before implementation; current recommendations are intentionally token-based.
5. **First custom panels:** (A) generic renderer for all 34 definitions first, then CHAOS/Sequencer/Macro (recommended); (B) custom CHAOS first; (C) all generic permanently. **Recommend A**.
6. **Legacy scope:** (A) keep ABI-1 variants as import/compatibility fixtures with native-descriptor fallback (recommended); (B) expose both ABI versions as distinct addable catalog variants; (C) remove legacy binaries. **Recommend A**, with no change to shipped sources during pause.
7. **Telemetry and configuration:** (A) a future control-thread snapshot API with explicit source/stale state (recommended); (B) CSS-estimated animations; (C) defer all indication. **Recommend A**, hiding indicators until truthful data exists.
