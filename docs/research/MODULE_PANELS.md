<!-- SPDX-License-Identifier: GPL-3.0-only -->

# Research: data-driven module panels

**Scope:** read-only audit of the 2026-11-01 `main` tree; proposed HTML/SVG front end; no ABI, DSP, patch, QML, build, or test changes. This is research, not approval to resume implementation. **Design target:** every loadable module, including an unfamiliar third-party module, gets an intelligible, usable panel without a per-module component.

## Executive finding

**Feasible, but the web catalog must expose the full native descriptor rather than just the current QML adapter.** ABI 2 already describes stable string IDs, names, signal/event port kinds and hints, stereo pairs, numeric bounds/defaults, tapers, steps and modulation flags. The SDK helper gives parameters no named options and no custom formatting callback by default. A *non-executable, optional sidecar* supplies sections, semantic labels, formatting and control choices; missing or malformed sidecars get a deterministic generic panel. This changes neither the ABI header nor existing patch IDs. Evidence: native/include/wirescape/module2.h:23-29,37-46,94-108; native/sdk/wirescape_sdk.h:16-28; native/control/app_engine.cpp:26-31.

**Measured source inventory:** 19 ABI-2 built-in types, 2 independently built SDK example types, and 13 ABI-1 module implementations, **34 compiled descriptor definitions / 21 distinct ABI-2 module IDs**. ABI-1 IDs overlap their ABI-2 ports; they are compatibility variants, not 13 new module types. The manifest/descriptor audit below totals **136 parameter entries and 103 port entries**, counting both ABI variants. The 19 built-ins appear in native/control/module_catalog.inc:2-20; SDK examples are built in native/engine2/CMakeLists.txt:90-98; ABI-1 sources are collected by native/CMakeLists.txt:35-41. These counts are **static source counts, not runtime enumeration or timing benchmarks**. An ABI-1 library can still be loaded but the app skips a duplicate ID: native/control/app_engine.cpp:50-68.

### Important limits

- The owner-described single-file “WIRESCAPE Studio 01” HTML sketch was **not supplied with this task**. The visual hierarchy below follows the described header/typed cables/knobs/“01 / MOTION” vocabulary, not verified pixel values or exact CSS from that sketch. Planner can supply the sketch before visual implementation.
- The ABI-2 `module.json` manifests list numeric legacy indices and coarse `signal`/`event` port types, **not** full descriptor hints/flags. The native `ws2_descriptor` and `ws2_param` remain authoritative (native/modules2/chaos_lite/module.json:7-45; native/modules2/chaos_lite/dsp.h:7-34; native/include/wirescape/module2.h:37-46).
- The ABI-2 helper sets `option_count=0`, `options=nullptr` and `value_to_text=nullptr` when creating parameters (native/sdk/wirescape_sdk.h:20-28). Some numeric mode semantics can be read from DSP comments, but they are **not present as machine-readable option labels**. Consequently the appendix records `enum_values:null` except for comment-confirmed step-gate semantics; this is a metadata gap, not a guess about the DSP.
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

## Generic automatic layout contract

1. **Header:** slim module-index/category eyebrow; native display name; optional short description, bypass/selection/status actions from host capabilities (not fake DSP params). Example `01 / MOTION` as a section caption, not a hardcoded module name. Unique graph node ID and stable module type ID stay separate. For missing modules show an explicit unavailable header with original ports/values retained in the patch.
2. **Ports:** left rail = descriptor inputs, right rail = descriptor outputs, in native order unless a valid sidecar order exists. Each jack carries label, kind (`signal`/`event`), hint (audio/pitch/CV/gate/trigger/MIDI/notes), poly policy and stereo pairing. Match paired L/R jacks visually but keep **two addressable ports**; never reinterpret two voices as stereo. Keyboard focus exposes every jack and typed cable route. No visual hint rejects a native-allowed connection.
3. **Body:** ordered sections from `sections[]`; otherwise natural native parameter order in a responsive two-column grid. Section captions `01 / MOTION`, `02 / SHAPE`, etc. Header/footer and left/right port rails remain aligned for all heights. At 280px panel width, use 24px minimum port-row pitch and keep knobs at touch-accessible hit areas; adapt card width for dense 26-param modules instead of forcing microscopic controls. Respect zoom for pointer hit testing and cable endpoints.
4. **Controls:** continuous gets accessible knob by default, primary/hero for single frequency or rate; secondary gets small knob. Step >0 / stepped flag gets an integer stepper; two-state uses toggle; **buttons/segments only with verified choice labels**. Fixed range (`minimum==maximum`) gets disabled readout. For an ABI-1 parameter, use linear slider/knob with explicit range; no claim of native modulation. For 0-param modules display ports, description and any approved host action rather than an empty broken card.
5. **Display:** separate *numeric plain value*, *normalized value*, *formatted value* and *formatted range*. Implement decimal rounding after float32 boundary conversion; e.g. `0.01 Hz–20 kHz`, not `0.009999999776482582–20000 Hz`. For Hz use Hz/kHz; seconds use ms/s; show unit once when unambiguous. Use descriptor taper (linear/log/exp) and step for interaction; never infer log taper solely from unit or ratio (current QML does: native/ui/qml/ParameterControl.qml:14-17). Tooltip, accessible text and keyboard adjustment use identical formatting.
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

**Contract acceptance:** run a generated schema/collision test that compares normalized descriptors to actual native entries; independently check every ID, kind/hint, min/max/default, taper, flag, and stereo pair; check unknown sidecar fields ignored, malformed/absent sidecars fallback, values round-trip through JSON unchanged, unknown patch entries are lossless. Include a regression for `Frequency · 0.009999999776482582–20000 Hz` and tests of Hz/kHz and ms/s boundaries. Assert no duplicate per-module JSX/HTML panel source is required. For runtime port reconfiguration, get actual `query_ports` results per configuration rather than treating the static table as immutable (native/include/wirescape/module2.h:94-101; native/sdk/wirescape_sdk.h:122-126).

**Performance budget (proposal, not benchmark):** benchmark catalog parse and initial render on Windows and Pi 5 with 1, 19 and 100 node instances; record decoded JSON bytes, first-contentful panel time, p50/p95 update frame time, patch/drag latency and JS heap. Initial targets: no audio-thread sidecar work, ≤16.7ms UI render budget at 60Hz (or measured 30Hz fallback on Pi), ≤30Hz live visual telemetry and no per-animation catalog reparse. This research performs **zero** runtime performance measurements.

## Decisions needed from the planner

1. **Presentation storage:** (A) adjacent `panel.json` per module (recommended); (B) additive `presentation` object in `module.json`; (C) embed into C descriptor (not recommended under soft freeze). **Recommend A**.
2. **Host catalog API:** (A) expose the full ABI-2 descriptor + sidecar in a versioned JSON control snapshot (recommended); (B) extend the existing ABI-1-adapter JSON with fields; (C) introspect from JS. **Recommend A**, keeping native-only UI fallback.
3. **Enum labels and missing semantics:** (A) explicit validated sidecar choices backed by code-reviewed mappings (recommended); (B) numeric-only steppers; (C) infer labels from parameter names. **Recommend A** where verified, B otherwise; never C.
4. **Visual fidelity:** (A) obtain and attach the original “WIRESCAPE Studio 01” sketch then lock tokens/spacing (recommended); (B) approximate from this layout spec. **Recommend A** before implementation; current recommendations are intentionally token-based.
5. **First custom panels:** (A) generic renderer for all 34 definitions first, then CHAOS/Sequencer/Macro (recommended); (B) custom CHAOS first; (C) all generic permanently. **Recommend A**.
6. **Legacy scope:** (A) keep ABI-1 variants as import/compatibility fixtures with native-descriptor fallback (recommended); (B) expose both ABI versions as distinct addable catalog variants; (C) remove legacy binaries. **Recommend A**, with no change to shipped sources during pause.
7. **Telemetry and configuration:** (A) a future control-thread snapshot API with explicit source/stale state (recommended); (B) CSS-estimated animations; (C) defer all indication. **Recommend A**, hiding indicators until truthful data exists.

---

## Appendix A — complete native descriptor data (JSON)

This is normalized **research data**, not a replacement for runtime enumeration. Each record has `abi_version`, `module_id`, `display_name`, `category_proposed`, `source`, `ports` (ID, direction, signal/event kind, hint, poly, stereo pairing, unpatched value, source line), `parameters` (ABI-2 string ID and legacy index, or ABI-1 numeric ID; name, bounds, unit, taper, default, step, flag list, modulatable, enum labels when verified, source), and `special_ui`. `null` means *not declared or not verified*, not zero/false. Source lines point to the declaration, including macro definitions for the expanded step array. For ABI-2 parameters, the manifest's numeric mapping corresponds to `legacy_index`; stable native parameter identity is the string ID (native/sdk/wirescape_sdk.h:20-25). Categories, section ideas and generic UI badges are proposals.

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
      "source": "native/modules2/adsr/dsp.h:L10"
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
      "source": "native/modules2/adsr/dsp.h:L10"
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
      "source": "native/modules2/adsr/dsp.h:L10"
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
      "source": "native/modules2/adsr/dsp.h:L10"
    }
  ],
  "special_ui": "Sustain knob, other sliders (native/ui/qml/ModuleCard.qml:98)"
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
      "source": "native/modules2/chaos_lite/dsp.h:L18"
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
      "source": "native/modules2/chaos_lite/dsp.h:L18"
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
      "source": "native/modules2/chaos_lite/dsp.h:L19"
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
      "source": "native/modules2/chaos_lite/dsp.h:L20"
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
      "source": "native/modules2/chaos_lite/dsp.h:L21"
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
      "source": "native/modules2/chaos_lite/dsp.h:L22"
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
      "source": "native/modules2/chaos_lite/dsp.h:L22"
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
      "source": "native/modules2/chaos_lite/dsp.h:L23"
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
      "source": "native/modules2/chaos_lite/dsp.h:L23"
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
      "enum_values": null,
      "source": "native/modules2/chaos_lite/dsp.h:L24"
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
      "source": "native/modules2/chaos_lite/dsp.h:L25"
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
      "source": "native/modules2/chaos_lite/dsp.h:L25"
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
      "source": "native/modules2/chaos_lite/dsp.h:L26"
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
      "source": "native/modules2/chaos_lite/dsp.h:L26"
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
      "source": "native/modules2/chaos_lite/dsp.h:L27"
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
      "source": "native/modules2/chaos_lite/dsp.h:L27"
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
      "source": "native/modules2/chaos_lite/dsp.h:L28"
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
      "source": "native/modules2/chaos_lite/dsp.h:L29"
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
      "source": "native/modules2/chaos_lite/dsp.h:L29"
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
      "source": "native/modules2/chaos_lite/dsp.h:L30"
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
      "source": "native/modules2/chaos_lite/dsp.h:L30"
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
      "source": "native/modules2/chaos_lite/dsp.h:L31"
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
      "enum_values": null,
      "source": "native/modules2/chaos_lite/dsp.h:L32"
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
      "source": "native/modules2/chaos_lite/dsp.h:L33"
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
      "source": "native/modules2/chaos_lite/dsp.h:L33"
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
      "source": "native/modules2/chaos_lite/dsp.h:L34"
    }
  ],
  "special_ui": "Generic card; stereo output pair is native (dsp.h:13-15)"
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
      "source": "native/modules2/clock/dsp.h:L8"
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
      "source": "native/modules2/clock/dsp.h:L8"
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
      "source": "native/modules2/clock/dsp.h:L8"
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
      "enum_values": null,
      "source": "native/modules2/clock/dsp.h:L8"
    }
  ],
  "special_ui": "Ticks stepper + run button (native/ui/qml/ModuleCard.qml:97,136-137; native/ui/qml/ParameterControl.qml:183-195)"
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
      "source": "native/modules2/function_envelope/dsp.h:L8"
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
      "source": "native/modules2/function_envelope/dsp.h:L8"
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
      "source": "native/modules2/function_envelope/dsp.h:L8"
    }
  ],
  "special_ui": "Shared QML generic module card"
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
      "source": "native/modules2/host_keyboard/dsp.h:L13"
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
      "enum_values": null,
      "source": "native/modules2/host_keyboard/dsp.h:L14"
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
      "source": "native/modules2/host_keyboard/dsp.h:L15"
    }
  ],
  "special_ui": "Global keyboard strip in Main.qml:430-441; generic card"
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
      "source": "native/modules2/ladder/dsp.h:L8"
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
      "source": "native/modules2/ladder/dsp.h:L8"
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
      "source": "native/modules2/ladder/dsp.h:L8"
    }
  ],
  "special_ui": "Knobs on shared ModuleCard.qml:102"
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
      "source": "native/modules2/lfo/dsp.h:L11"
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
      "source": "native/modules2/lfo/dsp.h:L12"
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
      "source": "native/modules2/lfo/dsp.h:L13"
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
      "enum_values": null,
      "source": "native/modules2/lfo/dsp.h:L14"
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
      "source": "native/modules2/lfo/dsp.h:L15"
    }
  ],
  "special_ui": "Rate/depth/offset knobs (native/ui/qml/ModuleCard.qml:100); no phase telemetry defined"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L21"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L22"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L23"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L24"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L25"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L26"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L27"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L28"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L29"
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
      "source": "native/modules2/macro_oscillator/dsp.h:L30"
    }
  ],
  "special_ui": "24-model selector numeric; no custom QML (dsp.h:19-30)"
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
  "special_ui": "Eight rows in non-parameter config.h:7-21; global MIDI dialog in Main.qml:166-187"
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
      "enum_values": null,
      "source": "native/modules2/midi_in/dsp.h:L10"
    }
  ],
  "special_ui": "Physical MIDI selection belongs to Main.qml:166-187"
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
      "source": "native/modules2/midi_to_cv/dsp.h:L12"
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
      "enum_values": null,
      "source": "native/modules2/midi_to_cv/dsp.h:L13"
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
      "enum_values": null,
      "source": "native/modules2/midi_to_cv/dsp.h:L14"
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
      "enum_values": null,
      "source": "native/modules2/midi_to_cv/dsp.h:L15"
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
      "enum_values": null,
      "source": "native/modules2/midi_to_cv/dsp.h:L16"
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
      "enum_values": null,
      "source": "native/modules2/midi_to_cv/dsp.h:L17"
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
      "source": "native/modules2/midi_to_cv/dsp.h:L18"
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
      "enum_values": null,
      "source": "native/modules2/midi_to_cv/dsp.h:L19"
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
      "enum_values": null,
      "source": "native/modules2/midi_to_cv/dsp.h:L20"
    }
  ],
  "special_ui": "No custom panel; mode/allocation flags are stepped (dsp.h:10-20)"
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
      "source": "native/modules2/mixer/dsp.h:L9"
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
      "source": "native/modules2/mixer/dsp.h:L9"
    }
  ],
  "special_ui": "Twin faders (native/ui/qml/ModuleCard.qml:96)"
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
      "source": "native/modules2/noise/dsp.h:L8"
    }
  ],
  "special_ui": "Shared QML generic module card"
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
  "special_ui": "Host output select button in ModuleCard.qml:141-147"
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
      "source": "native/modules2/scale_offset/dsp.h:L8"
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
      "source": "native/modules2/scale_offset/dsp.h:L8"
    }
  ],
  "special_ui": "Knob rule in ModuleCard.qml:105"
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
      "source": "native/modules2/slew/dsp.h:L8"
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
      "source": "native/modules2/slew/dsp.h:L8"
    }
  ],
  "special_ui": "Knob rule in ModuleCard.qml:104"
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
      "source": "native/modules2/vca/dsp.h:L9"
    }
  ],
  "special_ui": "Shared QML generic module card"
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
      "source": "native/modules2/vcf/dsp.h:L10"
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
      "source": "native/modules2/vcf/dsp.h:L10"
    }
  ],
  "special_ui": "Cutoff/resonance knobs in ModuleCard.qml:101"
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
      "source": "native/modules2/vco/dsp.h:L11"
    }
  ],
  "special_ui": "Frequency hero knob in ModuleCard.qml:99,112-114"
},
