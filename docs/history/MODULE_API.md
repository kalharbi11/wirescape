# Wirescape module API v1 — host 0.5

**Primary external developer reference:** `module_build_guide/` (separate downloadable kit). This document describes the installed host side.

## Why the sequencer agent had to edit the host

The previous registry used a final import statement in `wirescape/plugins.py` to register extra processors; `wirescape/graph.py` special-cased note/sequence traffic; `wirescape/server.py` special-cased sequence editing; `web/index.html` and `web/client.js` hardcoded the sequencer face/editor. That is *not a drop-in module system*. Also, the supplied integrated sequencer bundle was based on host code older than Wirescape 0.4 and would have overwritten newer keyboard/preset/deep-editor behavior.

## API v1 design

Module discovery now scans `modules/<id>/module.json` without executing code. New modules appear under **MODULES ↻ → SCAN** and can be explicitly **ENABLED** only with audio stopped. `wirescape/extensions.py` imports `modules.<id>.plugin` and calls `register()`. Enabled modules persist in `user_data/enabled_modules.json` and load automatically at later startups. Installing/updating an ordinary module requires *no per-module edits to main host or web files*.

`modules/<id>/plugin.py` calls `register_audio_module(id, definition, ProcessorClass, default_state=None, validate_state=None)`. The processor receives block-rate typed inputs and returns typed named outputs. Optional `module_state` is validated by module code, carried in patch JSON, queued to the audio thread, and dispatched via `configure()`. The browser loads module-owned `editor.js`/`editor.css` files declared in the manifest, served only for enabled plugins. Optional `window.WirescapeModuleUI.<id>` customizes the face and deep editor; otherwise Wirescape uses generic controls.

All modules declare typed `audio`, `mod`, `clock`, `gate`, or `notes` ports. `notes` payloads are block-rate `(MIDI_pitch, is_on)` and CHAOS now accepts a `NOTES IN` port plus CUTOFF modulation. Note ownership is per upstream module, preventing one disconnected sequencer from cancelling another sequencer's note. Hardware audio remains one shared sound-device stream.

## What is not promised

- Python module code is **not sandboxed**. Explicit ENABLE means user trusts the installed Python code. Do not download/extract arbitrary ZIPs or auto-enable unknown folders.
- Host can discover/activate a *new* Python module while audio is stopped. It **cannot safely hot-reload already imported Python/native DSP code**. Replace the module folder, bump manifest version, and restart Wirescape.
- Current reference DSP is Python-based; `process()` runs on the audio callback and is not hard realtime. Validate block size, allocations and CPU on Windows/Pi.
- No general data, image, video, asynchronous graph or sandboxed plugin API is implemented yet. Do not claim all future Wirescape domains are supported.
- State `module_state` is serialized by the editor; clients must keep schema stability or explicitly migrate it. Legacy integrated-sequencer `sequence` patch key is read as a compatibility fallback.

## Current folder arrangement

```
wirescape/
  wirescape/        host kernel (audio, graph, routing, server, plugin loader)
  modules/
    chaos/          current built-in synth adapter and source
    sequencer/      independent folder plugin (API v1)
    ...             future standalone module packages
  web/              common canvas + generic controls
  docs/
  tests/
  user_data/        local enabled_modules and user presets (never distribute)
```

Existing `modulator`, `interface`, and `gain` remain built-in adapters/definitions for backwards compatibility. Converting them to independent API v1 folders is a future migration. **Do not put the host kernel inside `modules/core/`**; the core owns and loads module packages, not vice versa.

## Minimal sample

See the separate `module_build_guide/template/modules/example_saturator/` package and the working installed `modules/sequencer/` for richer state/JS behavior. Existing integration/tests: `tests/test_extensions.py`, `tools/ui_module_smoke.py`.
