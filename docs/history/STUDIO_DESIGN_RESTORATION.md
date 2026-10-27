# Studio visual restoration

The native Qt Quick screens follow the original Studio reference in `${USERPROFILE}/Documents/<project-parent>/others/wirescape/design_reference/WIRESCAPE_Studio_Original.html` and the earlier browser view in `web/index.html` / `web/client.js`. Those files were inspected as visual and interaction evidence; their JavaScript was not executed or imported.

## Reference to native UI

- Kept the warm ground, rail, surface, ink, muted text, rust accent, and fine border palette used by the reference.
- Retained the compact library entries and circular signal ports. Refined the ports to the reference's 15 px, single-stroke jack and matched the module cards' 13 px corners and stronger title treatment.
- Kept the native patch canvas, cables, generic parameter sliders, transport controls, and current Qt interactions. Existing object names, focus handling, dialogs, and actions remain in place.

## Deferred

- The reference's bespoke circular knobs and specialized module bodies are deferred. Replacing generic parameter controls would change interaction behavior and needs dedicated keyboard, drag, precision and accessibility checks before replacement.
- Subtle CSS shadows, browser-only hover transitions, continuous waveform motion, and animated decorative details are omitted to keep this static native pass lightweight and predictable.
- ABC Areal typography is handled separately; this pass continues to use the current native font fallback.

No performance measurements were made as part of this visual pass.

Root verification: both window sizes passed; 1000x650 screenshots visually inspected. Unsupported font-dependent symbols were replaced with ASCII. Private local ABC Areal is now optionally loaded via --local-font and used by the shortcuts; it is excluded from Git and packages. The icon is deterministic black/white geometry, embedded in the Windows executable. No measured performance claim; live device work and benchmarks remain paused during Ableton use.
