# Wirescape Studio — UI implementation reference

Reference basis: `WIRESCAPE_Studio_Final.html` and the running Wirescape Studio interface. Follow it instead of inventing a separate plugin theme. For design iteration, aesthetic references may be supplied separately.

## Palette and tokens

Copy from `reference/STYLE_TOKENS.css`. Important existing tokens include:

| Token | Value | Use |
|---|---|---|
| `--ground` | `#eeede9` | Canvas background |
| `--rail` | `#f5f4f0` | Sidebar and app chrome |
| `--surface` | `#faf9f6` | Module panel |
| `--surface-2` | `#f3f1ed` | Inset controls and mode backgrounds |
| `--ink` | `#393b39` | Standard foreground |
| `--sub` | `#858681` | Labels and secondary text |
| `--line` | `#d9d8d2` | Panel lines |
| `--accent` | `#c5774c` | Active state, selected paths |
| `--wire-a` | `#777e79` | Audio patch cable |
| `--wire-m` | `#c17853` | Modulation cable |
| `--wire-g` | `#abb0a7` | Gate/clock cable |

Font stack in the original template: `ABC Areal`, `ABCAreal`, Arial, Helvetica, sans-serif. **Do not bundle or redistribute the ABC Areal font files**; rely on local availability and fallbacks.

## UI hierarchy

Application: sidebar module browser → workspace canvas → movable independent module objects → module header → controls + input/output ports → optional deep editor. Existing module UI uses an 82px header, grouped sections, circular knobs with pointer ticks, short uppercase section labels, segmented mode buttons, and labeled ports along left/right sides. Reuse exposed `.control`, `.knob`, `.port`, `.section-label`, `.mode-row`, and `.deep-*` classes when appropriate rather than embedding a standalone website inside a module.

UI behaviors: moving header drags module; double-click module focuses canvas; output ports on the right connect to input ports on the left; controls must be keyboard accessible; small contextual descriptions use `data-help` and/or `title`; don't hijack keyboard notes when user edits non-text controls. Text input must not trigger synthesizer notes while typing. A separate deep editor is appropriate for more than ~6–12 continuous controls. Never hide advanced synthesis parameters entirely if their functionality is important.

## Custom module scope

Never style global elements, change CSS root tokens, replace the host `client.js`, or rely on another module's selectors. Prefix your classes (`.my-module-grid`, `.my-module-step`). Do not inject unsanitized HTML from patches/preset names. Favor `textContent`, `createElement` and safe attribute values. Use Wirescape's current control vocabulary: macro/knob 0..100, discrete `modes`, typed ports, and state-based deeper editing.

The host browser is a visualization and control client only. DSP executes in Python and is not sent as audio through WebSocket.
