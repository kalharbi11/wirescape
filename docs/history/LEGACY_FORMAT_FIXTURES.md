# Legacy document fixtures

These fixtures are synthetic shape examples, not copied user documents and not claims of migration equivalence. They contain no private presets or Surge data.

## Local evidence

| Format/version | Evidence and shape |
| --- | --- |
| `wirescape.patch` v1 | Historical v1 shape is evidenced by `tests/test_graph.py:34-42`; no local v1 serializer was found. The current producer `web/index.html:500` emits v3, and its compatibility reader `web/index.html:503-524` checks the two arrays; `wirescape/graph.py:105-138` consumes modules/connections and `wirescape/schema.py:63-123` validates module IDs/types/state and cable addresses/ports. Test shape: `tests/test_graph.py:34-42`. |
| `wirescape.patch` v2 | Historical serialized shape is evidenced by `tests/test_sequencer.py:13-22`: sequencer module payload uses `sequence`, values, and typed cables. Active web reader `web/index.html:523` copies `sequence` to `module_state` if the latter is absent. This is evidence of compatibility handling, not a located v2 serializer. |
| `wirescape.patch` v3 | Active producer is `web/index.html:500`, including `module_state`, `advanced`, `raw`, `mode`, `monitor`, `keyboardArmed`, and `keyboardSolo`. Test payload is `tests/test_extensions.py:18-27`. Reader handles pre-v2 and pre-v3 position adjustments at `web/index.html:506-520`. |
| `wirescape.visual.study` v4 | Producer is the preserved original UI `design_reference/WIRESCAPE_Studio_Original.html:298` and its loader is at `:300`; host compatibility branch `wirescape/graph.py:128` identifies this format, and `tests/test_graph.py:167-173` verifies imported visual monitor state is not enabled physically. Its producer emits theme/motion, modules with id/type/x/y/values/mode/monitor and `connections` from/to. |

The schema validator shown in `wirescape/schema.py` validates graph structure and installed module types; it does not dispatch by document format/version. The active browser loader performs compatibility adjustments and writes the current v3 state after import. Historical v1/v2 producer implementations and original payloads are not present in the searched local source; v2 is evidenced by a test shape and the compatibility reader only. No claim is made about older formats beyond these local observations.

## Fixtures

- `native/tests/fixtures/legacy_documents/patch_v1.json`: synthetic known-module and audio/control cable shape from the v1 test.
- `native/tests/fixtures/legacy_documents/patch_v2.json`: synthetic sequencer `sequence` payload with typed connections.
- `native/tests/fixtures/legacy_documents/patch_v3_unknown.json`: synthetic future document/module/cable fields and unknown module type, for preservation-oriented future tests.
- `native/tests/fixtures/legacy_documents/visual_study_v4.json`: synthetic original visual-study fields, including visual monitor flags.
- `native/tests/fixtures/legacy_documents/future_patch_v99.json`: valid JSON with unsupported future version payload.
- `native/tests/fixtures/legacy_documents/malformed_patch.json`: valid JSON with malformed `modules` container type.

Unknown fields and cable endpoints in the preservation fixture are intentionally not accepted as an executable graph by the current validator. They only document payload shape for a future preservation seam.
