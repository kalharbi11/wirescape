# Acceptance tests and release checklist

## Deliverables

A single ZIP rooted at `modules/<id>/` containing `module.json`, `plugin.py`, `engine.py` as needed, optional namespaced `editor.js`/`editor.css`, README, and isolated tests. Do **not** include another `wirescape/` package, `web/`, `node_modules/`, Python virtual environments, huge source trees, or compiled Windows-only Surge binaries. Never overwrite existing modules.

## Local test commands

From your Wirescape root:

```powershell
py -3.12 tools\verify.py
py -3.12 -m unittest discover -s tests -q
```

On Pi 5:

```sh
python3 tools/verify.py
```

Additional test script for custom UIs (if Playwright Chromium installed): `python tools/ui_module_smoke.py` (reference sequencer UI; adjust tests for your module).

Minimum module tests: import/manifest; `register()` called once; each port's direction/type; output shapes and finite values at 64, 128, 512, and 1024 frames; silence, rapid parameter changes and long duration; multiple instances; malformed state; save/load fidelity; disconnect/reconnect with outstanding notes; gate/trigger rising edges; CPU block budget relative to a 10.67ms deadline; platform-specific dependencies, if any.

## Host integration contract

A module author may change **only `modules/<id>/`**. For stateful modules, the editor must use `module_state` and `changed()`; for note sequencers, output `notes` events; for generic DSP processors, output mono float32 blocks. Never patch Wirescape's central graph, patch schema, server, or root HTML for a module release. If a needed feature is unsupported, file a `HOST_CHANGE_REQUEST.md` describing the required host API rather than silently adding ad-hoc code.

## Version and safety

Increment the manifest version when shipping code changes. Review any Python dependency or native binary added by an agent. Require an explicit user trust action before enabling modules installed from unknown sources. Module enabling is only allowed while audio is stopped; restart to update loaded modules. Never run arbitrary Python shipped inside a patch JSON. Keep patch formats backwards-compatible. Test on Windows, then Pi—mock tests do not establish realtime audio performance.
