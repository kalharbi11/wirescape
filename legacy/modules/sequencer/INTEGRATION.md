# Modified Wirescape host/editor files

Relative to your uploaded `wirescape(1).zip`, put these files in the main project at **exactly** these paths:

| Path | Reason |
|---|---|
| `wirescape/plugins.py` | Explicitly imports/registers `modules/sequencer/plugin.py`; no arbitrary dynamic imports. |
| `wirescape/schema.py` | Validates sequencer pattern data; adds `CHAOS.notes` event and `CHAOS.cutoff` modulation inputs. |
| `wirescape/graph.py` | Routes typed polyphonic note events, manages note ownership/releases, runs the sequencer, supports live `sequence` changes, updates filter CV and playhead telemetry. |
| `wirescape/server.py` | Sends sequencer defaults to the browser, accepts sequence edits and serves module JS/CSS from fixed routes. |
| `web/index.html` | New notes/cutoff jacks, 16-step sequencer face and patch serialization, loads the sequencer UI stylesheet/script. |
| `web/client.js` | Deep-editor integration, sequence WebSocket messages and playhead display. |
| `tests/test_web.py` | Updates expected built-in module registry; original assertion otherwise fails. |
| `tests/test_sequencer.py` | Added sequencer behavior and safety tests. |
| `README.md`, `docs/MODULE_API.md`, `docs/ARCHITECTURE.md` | Installation and new note-event API docs. |
| `tools/verify.py`, `tools/ui_smoke.py` | Sequencer syntax checks and browser integration regression tests. |
| `examples/03_chord_sequencer.wscape.json` | A prewired example patch. |

**The existing `modules/chaos/` folder is unchanged.** Keep your binary, presets, engine and other local modifications. `modules/sequencer/` is the only newly added module folder.

Recommended: save a copy of your project, extract the **Changed main files** archive at the project root, and extract **Sequencer module** into `modules/`. Or use **Integrated Wirescape** as the complete working reference tree. Restart the Python host and hard-refresh the browser.
