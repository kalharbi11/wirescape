> Legacy Python/browser kit for the preserved old build. For the current standalone Wirescape Essentials, use [the updated kit](../essentials_build_guide/README.md). Do not use this legacy API for new Essentials modules.

# WIRESCAPE — Module Builder Kit (API v1)

**Give this whole folder to an agent** when asking it to build a new module. It contains the relevant host contract, UI conventions, working module template, validation requirements and version rules. **The complete Wirescape application is not required for an ordinary module.**

Wirescape is a general-purpose patching environment. Its current executable runtime supports **audio blocks, scalar modulation, gates/clocks and block-rate polyphonic note events**. Data, video and GPU processing are future interfaces, not implemented plugin APIs today.

## Assignment template for a new agent

> Build `modules/<lowercase_id>/` for **Wirescape module API v1**. Read all files in `module_build_guide/`, including `PLUGIN_API_V1.md`, `STYLE_GUIDE.md`, and `RELEASE_CHECKLIST.md`. Deliver **only your self-contained module folder**, tests and a short integration report. Do not modify `wirescape/`, `web/`, app launch scripts, global requirements or existing modules. If a feature is not possible with the published API, write a `HOST_CHANGE_REQUEST.md` describing the missing capability; do not patch the host. Avoid pretending future APIs exist. Your module must be usable as `modules/<id>/` plus `MODULES ↻ → SCAN → ENABLE` in Wirescape 0.5 or later. Use namespaced classes/JS/CSS. Respect Windows Python 3.12 and Raspberry Pi 5 portability. Document CPU cost, latency, state format and edge cases.

## Drop-in installation

1. Extract `example_saturator` or the new module folder into `wirescape/modules/<id>/`.
2. Launch Wirescape (or leave it running with **audio stopped**).
3. Click **MODULES ↻**, then **SCAN MODULES**.
4. Inspect the module name/version, click **ENABLE MODULE** only if the code is trusted.
5. Add the module from the sidebar. Newly installed Python modules can be enabled while the audio service is stopped. **Updates to a previously loaded Python module require restarting Wirescape**; rescan is not hot reloading of active DSP.
6. Run `py -3.12 tools\verify.py` (Windows) or `python3 tools/verify.py` (Pi). Start audio only after validation.

No source edits in `web/client.js`, `wirescape/graph.py`, `wirescape/plugins.py`, or `wirescape/server.py` should be required for a module conforming to this API.

## Folder deliverable

```
modules/my_module/
  module.json           # REQUIRED: discovery, supported API, optional UI assets
  __init__.py           # recommended; may be empty
  plugin.py             # REQUIRED: register()
  engine.py             # optional: audio/control processor
  editor.js             # optional: custom face and deep editor
  editor.css            # optional: scoped custom styles
  README.md             # REQUIRED in submissions
  tests/                # recommended isolated tests
```

A new agent should normally be given **only this kit**, your sound/behavior specification, and your aesthetic references. Give the *full* Wirescape project only if debugging an actual host incompatibility, extending the core signal/event API, or maintaining an existing complex module such as CHAOS.

## Modules versus core

`wirescape/` is the **host kernel** (graph, scheduling, audio devices, patch schema, plugin loader, server). `modules/` contains instruments, control sources, effects and interfaces—**not** another folder called `core`. Moving the host kernel inside `modules/core/` would blur ownership and introduce a circular dependency. Some legacy built-ins (CHAOS, MODULATOR, AUDIO I/O, GAIN) still have host-specific adapters; they can be migrated to the standard API incrementally without breaking existing patches.
