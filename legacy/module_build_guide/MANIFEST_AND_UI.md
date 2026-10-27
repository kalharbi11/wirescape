# Manifest and optional custom UI

## `module.json`

```json
{
  "api_version": 1,
  "id": "my_module",
  "name": "MY MODULE",
  "version": "0.1.0",
  "description": "One-line purpose",
  "entry": "plugin:register",
  "assets": ["editor.js", "editor.css"]
}
```

`assets` is optional. It may list only local `.js` and `.css` names, no path separators. The host serves **only enabled modules' declared assets** at `/module-assets/<id>/<filename>`. Do not add scripts in global `web/index.html`. Never require the agent to edit host HTML to add a module.

If no UI assets are declared, Wirescape supplies a generic module front and CONTROLS / PORTS deep editor using the registered definition. This is the **preferred first iteration** for simple processors.

For a special interface, `editor.js` can register:

```js
window.WirescapeModuleUI ??= {};
window.WirescapeModuleUI.my_module = {
  tabs: ['PATTERN', 'SETTINGS', 'PORTS'],
  face(moduleInstance, coreElement, openEditor) {
    // Append DOM elements to coreElement. Call openEditor(index?) as needed.
  },
  renderEditor(deepBody, moduleInstance, tab, changed, changedControl, rerender) {
    // Edit moduleInstance.module_state and call changed() to save & send to host.
    // For exposed 0..100 controls, call changedControl(key, number).
    // Call rerender() when switches need the editor to redraw.
  }
};
```

Functions must not assume a specific module instance ID. DOM event listeners should stop propagation on interactive controls so dragging/cable gestures do not conflict. Use semantic labels, explicit focus/keyboard semantics, and help text/`data-help` for unfamiliar synthesis terms. Scope CSS under a module-specific prefix like `.my-module-*` and do not redefine `.module`, `.knob`, `:root`, `.app`, or global tags.

The agent who built the sequencer initially added `<script src>` and special cases to global HTML/client. This new SDK removes that requirement: its UI registration and state are fully encapsulated under `modules/sequencer/`.

### What the host can and cannot auto-generate

- **Can**: module header/name, generic knobs for 0–100 controls, mode buttons, port labels and cable colors; deep controls; persistence; typed routing; safe asset serving.
- **Cannot yet**: generate rich oscilloscopes/step grids without a custom JS asset; create user-defined port data types or visual/video schedulers; use sample-accurate note events; automatically install Python pip dependencies or native libraries; truly unload native Python modules without restart.

### Updating a module

Increment `version` in the module manifest, replace **only its folder** while Wirescape is stopped (recommended), then restart Wirescape. Press MODULES ↻ → SCAN to verify. Code already imported by Python cannot safely be reloaded by an editor button; the UI explicitly labels changed versions as **RESTART TO UPDATE**. Existing patch state must remain valid.
