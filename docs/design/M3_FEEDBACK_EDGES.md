<!-- SPDX-License-Identifier: GPL-3.0-only -->
# M3-3: authoring delayed feedback and marking actual back-edges

**Status:** design proposal for #179; no Studio or engine implementation, schema
amendment, timing change, or M3 release claim. Planner approval is required before
the editor authoring and engine metadata interfaces become tasks. In particular,
a visual back-edge is **not** a new kind of patch cable.

## Verified boundary (source at main `eef2608`)

- ABI2 graph preparation already computes strongly connected components, sorts
  nodes and edges by stable IDs, and designates deterministic DFS back-edges:
  `native/engine2/engine2.cpp` (`Engine::Impl::topology`). For signal and
  modulation back-edges, the engine allocates a delay ring of
  `config.feedback_block` frames. Its event routing also offsets back-edge
  deliveries by `feedback_block`. Tests in
  `native/engine2/tests/feedback_tests.cpp` cover 1, 32, and 256 frames,
  node-insertion permutations, mixed components, and callback boundaries.
- `native/engine2/engine2.h` exposes `feedback_edges()` (a **count**) and
  `feedback_loops()` (node sets and delay frames); neither identifies an
  individual saved port-to-port connection as delayed.
- The native v2 reader `native/document/patch_document.cpp` accepts ordinary
  signal, event and modulation connections. It validates optional
  `connections[].feedback` as a boolean and
  `settings.feedback_block` as a positive integer no greater than
  `max_block_frames`. The runtime loader
  `native/document/runtime_patch.cpp` passes the saved feedback block
  into the internal engine configuration, but does **not** use the optional
  per-connection `feedback` field to choose delayed edges.
- Studio currently edits an ABI1-like view:
  `native/ui/studio_controller.cpp::validateDocument` rejects every graph
  cycle with “Feedback cycles require an explicit delay and are unsupported.”
  `Main.qml` draws ordinary cables and has no designation/delay metadata.
  Consequently a v2 file which the headless loader can render with feedback
  must not be described as supported or editable in Studio yet.

## Proposed user flow (audio stopped)

1. Connect output to input using existing click/drag/keyboard affordances. If
   the proposed connection closes a cycle, keep the edit **pending** and show
   an explicit “Create delayed feedback” confirmation instead of accepting
   silently or throwing away the candidate. Display the proposed patch-wide
   delay in *frames* and approximate milliseconds at the configured sample
   rate. Warn that the delay belongs to the engine-selected graph back-edge,
   not necessarily the cable the user clicked.
2. The confirmation lets the user choose a valid `feedback_block` from 1 to
   `max_block_frames` (default 32 if not otherwise saved), then Cancel or
   Create. Keyboard: a focusable choice, Enter to commit, Escape to cancel.
   Touch: large labelled confirmation controls; never require hover. The
   confirmation must not audition a live/device graph.
3. Validate and prepare the **whole candidate** while the accepted document
   remains unchanged. On failure (occupied input, invalid signal/event kind,
   incompatible port, oversized feedback ring, unavailable module, resource
   limit or engine rejection), show the actual reason, retain prior routing
   and delay, and offer a safe retry. No half-connected cycle is persisted.
   On success, replace the accepted patch once; one undo restores both edge
   and any delay-setting change. A later delay edit is its own undo step.
4. A user may remove either forward or delayed cables normally; removing
   a cycle can reclassify previously designated edges, so repaint all
   markers from the newly prepared graph, not from remembered gesture state.
   Showing a confirmation is an *intentional* editing action, not evidence
   that the clicked wire is the engine-selected back-edge.

## Persistence: use the existing native v2 document

The minimal representation is an ordinary `connections[]` endpoint pair
with its already-supported `kind` and the existing patch-wide setting:

```json
{
  "settings": {"max_block_frames": 128, "feedback_block": 32},
  "connections": [
    {"kind": "signal",
     "from": {"module": 3, "port": "audio"},
     "to": {"module": 2, "port": "audio"}}
  ]
}
```

This fragment is **illustrative**, not a complete runnable patch. Module IDs
and port IDs must be real saved identities. Never invent `kind:"feedback"`,
insert a delay node as an undocumented substitute, rename existing endpoints,
or encode a visual dash/arrow as persistent DSP state. An optional
`feedback:true` already passes the document validator and can be preserved
as an author-intent annotation **if the planner elects to use it**. It is not
currently a runtime directive; marking a connection `feedback:true` must
never be interpreted as a verified one-edge delay. Likewise the setting
applies at graph level, not independently per cable. Preserve unknown fields
and inactive modules through v1/v2 migration and save/reload.

## Visual and accessible feedback

- Show the **engine-designated** back-edge with a dotted/dashed segment,
  a distinct small delay icon and a readable “Feedback · 32 frames”
  label. Colour alone is insufficient. Non-delayed edges in the same
  strongly connected component stay ordinary.
- The selected-edge inspector reads “A audio → B audio; delayed 32 frames”
  and optionally shows milliseconds computed using the *accepted* sample
  rate. Announce the same via accessible name and tooltip; keyboard Tab/Enter
  and touch tap expose it without hover. At 1 frame use the singular label.
- A loop inspector may show the engine's SCC node set and delay. If there are
  multiple back-edges, mark **each actual designated connection**, not all
  edges in the component. If metadata is unavailable or stale, say
  “Feedback delay unavailable”; never guess the marker from relative module
  x/y position, cable curve or connection creation order.
- One visible cable remains one document connection. Readable back-edge
  symbols and selection targets must scale with the existing zoom/pan
  transform and avoid covering port hit zones or the sidebar.

## Required internal integration before implementation

1. Add a bounded, immutable, **non-realtime** graph snapshot at successful
   preparation: stable from-node/output → to-node/input identities, connection
   kind, `is_back_edge`, effective delay frames, topology generation and
   optionally SCC ID. The current `feedback_edges()` count and
   `feedback_loops()` node sets cannot replace per-edge provenance.
   The internal source of truth is the already chosen `Edge.back` in
   `Engine::Impl::topology`. Do not sample mutable DSP buffers from QML.
2. Introduce a Studio candidate-edit/validation API for cycle authoring and
   settings changes; it must stop rejecting all cycles **only when** the
   validated/prepared ABI2 graph can be staged and accepted transactionally.
   Carry the existing event/signal/modulation kinds without collapsing
   event into control. This is not an ABI-header or patch-schema request.
3. Provide a UI-safe snapshot publication and identity/generation check.
   Recompute on load, edit, feedback-block change and save/reload; clear
   stale markers on failed preparation. Keep the existing stopped-audio
   restriction until a separate live-topology contract is approved.
4. Provide an undoable transaction boundary before exposing compound
   feedback edits. A failed edit or Cancel must not mark the patch dirty,
   change the current title or drop active connection selection.

## Offscreen acceptance scenarios for a later implementation

1. Close a three-node loop, confirm 32 frames, save/reload: the ordinary
   cable endpoints and `feedback_block:32` round-trip; the prepared engine
   reports its actual marked edge(s). Cancel leaves document bytes/dirty
   status unchanged. No device/backend is opened.
2. Feed the UI a deterministic engine metadata snapshot with one marked
   edge in a three-node SCC. Only that edge shows the 32-frame marker.
   An insertion-order permutation yields the same engine-designated marker.
   With two back-edges, exactly two markers appear.
3. Change delay 32 → 1 → undo: labels follow actual prepared generations,
   read “1 frame” at 1, and undo restores 32 and routing. Invalid 0,
   greater-than-max, and failed preparation must preserve the accepted graph.
4. Keyboard-only and simulated touch complete/cancel the confirmation and
   inspect the same effective delay. Zoom/minimum-window layout retains
   visible marker, unoccluded ports and accessible hit targets.
5. Feed the test a graph whose user-confirmed closing cable is **not** the
   engine-designated delayed edge: the confirmation remains an intent
   record, while actual metadata controls the displayed marker.
6. Legacy and v2 saved patches, mixed signal/event connection validation,
   unknown fields and regular non-cyclic editing remain lossless. No
   weakening of the existing fullscreen, focus or controller tests.

## Planner decisions still needed

- Whether to use the **existing** optional `feedback:true` solely to
  remember a user's confirmation, or leave authoring intent ephemeral.
  Neither choice may override the engine's deterministic back-edge selection.
- The internal per-edge snapshot owner, bounded size and UI publication
  cadence; how changes to `feedback_block` interact with audio-thread
  retirement once live editing is designed.
- The undo model and whether a feedback-block change should apply to all
  existing loops after an explicit patch-wide warning.

This note does not authorize editing the Studio, controller, engine, ABI
headers or patch format. Related design: [M3 interaction study](M3_INTERACTION_STUDY.md)
and [M3 readiness audit](M3_READINESS_GAPS.md).
