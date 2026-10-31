<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Parameter modulation edges — proposal for #177

Status: design only, pending explicit planner approval. This document authorizes
no patch-format, ABI, engine or Studio implementation change. It follows
[ABI2 D7](../specs/ABI2_PLAN.md#d7-cv--parameter-modulation-host-side-non-destructive)
and addresses the [M3 readiness gap](M3_READINESS_GAPS.md).

## Reuse the existing patch v2 representation

The document validator and runtime loader already recognize `kind: modulation`.
Keep schema version 2 and reuse that representation; do not introduce a second
parameter-edge array, numeric parameter indexes or new module/parameter IDs.
A proposed cable from module 12's `control` output to module 34's `cutoff` is:

```json
{
  "from": {"module": 12, "port": "control"},
  "to": {"module": 34, "port": "cutoff"},
  "kind": "modulation",
  "amount": -0.25
}
```

For this kind only, `to.port` is the target descriptor's stable **parameter ID**,
not a signal-input ID. The module IDs are the existing patch-instance IDs.
The example assumes those actual descriptors declare these IDs; labels and
array positions never substitute for them. `amount` is finite and in [-1, 1].
Its sign records polarity: negative inverts, positive preserves; zero disables
contribution without deleting the cable. Display magnitude and polarity as views
of this one signed value. Do not add a redundant serialized `polarity` field.
Unipolar source values remain unipolar; there is no implicit centering or rectifier.

Several sources may target one parameter. Preserve their saved order and distinct
entries; do not deduplicate, combine or normalize their amounts during a round-trip.
Normal Studio creation rejects an exact duplicate source/target pair and selects
its existing amount control instead. Existing duplicate entries remain preserved
and individually editable; this UI convenience does not redefine loader semantics.

## Compatibility and lossless editing

No migration rewrites existing signal/event cables, base parameter values,
module state, IDs or unknown properties. Patches without modulation retain their
current behavior. The v2 document retains original objects; an editor operation
updates only the recognized fields it owns and preserves all other members,
including nested `from`/`to` extension fields. Save/load/save is compared as JSON
values, not byte formatting or property order.

Existing native loading already resolves modulation edges. Older Studio versions
may reject their editor view: do not promise that every older UI can edit them.
A future Studio must report unsupported/unavailable endpoints and retain their
raw saved data. It must not silently drop, convert to a signal cable, or execute
an unresolved modulation edge. Lossless storage and executable-graph validation
are separate responsibilities; this proposal does not relax runtime rejection.
Undo restores the original whole connection object, including unknown members.
Removing a module removes its referencing edges only through an explicit edit
transaction with undo; loading an unavailable module never implies that deletion.

## Studio gestures and presentation

Drag an existing signal-output jack onto a MODULATABLE parameter control. A
highlight identifies the target and parameter label before release. Event outputs,
read-only controls and non-modulatable parameters are not drop targets. The same
validation runs for mouse, keyboard-accessible actions and programmatic edits.
The initial signed amount is +0.25; a focused amount control permits precise
entry in [-1, 1], horizontal drag, polarity inversion and reset to zero. Escape
cancels a preview without adding a cable or changing the base value.

Dragging the knob normally still changes its base value. Select the cable's
amount badge to edit modulation depth; do not overload an ordinary knob drag or
require an undiscoverable modifier. A context action and accessible name expose
amount, source, target, polarity and disconnect. Multiple incoming edges each
have their own amount badge; selecting one must not edit another.

The knob pointer/value remains the accepted base value. Show an effective-value
indicator only when a modulation connection exists and effective telemetry is
available. Its movement represents measured post-sum/clamp modulation, not a
free-running decorative animation. For polyphonic values, show a bounded range
or explicitly selected voice; never imply one arbitrary voice represents all.
On stale telemetry retain the base and mark the effective indicator unavailable.
The existing UI styling agent can choose the visual form after this model is
approved; this document does not assign or change its QML files.

## Engine mapping and edit transaction

Resolve source output and destination parameter **strings** against descriptors
on every candidate graph build. Then call:

`Engine::modulate(sourceNode, sourceOutputIndex, targetNode, parameterIndex, amount)`

The indexes are transient engine bindings, never saved IDs or legacy parameter
numbers. Reject missing endpoints, event outputs, absent MODULATABLE, invalid
amounts and an engine rejection. Check all edges before publishing a candidate;
any failure leaves the accepted document, graph and selected cable unchanged.
Current engine authoring is quiescent. Running edits depend on #186's separately
verified publication/retirement mechanism; never call `modulate()` concurrently
with render or rebuild DSP in a process callback. No public ABI extension is needed.

Retain D7's normalized-space calculation per sample/channel:

`effective = clamp(base_normalized + sum(amount * source_cv), 0, 1)`

Apply the declared parameter taper after summation/clamping, retaining existing
engine broadcasting, parameter smoothing and finite-value rules. Base automation
and modulation remain independent. Dedicated pitch and FM ports retain their
musical units; normalized knob modulation does not replace 1 V/oct routing.
Feedback/cycle authoring remains a separate approved task; do not lift Studio's
cycle restrictions as part of this work.

## Acceptance matrix for the later implementation issue

| Area | Required evidence |
|---|---|
| Storage | Positive, negative and zero amounts; several edges; save/load/save equality of every known and unknown nested field; unrelated signal/event cables unchanged. |
| Stable bindings | Reorder parameter descriptors and preserve stable-ID resolution; colliding display labels do not redirect an edge; unavailable IDs remain stored and are rejected for execution. |
| Rejected edits | Invalid target/source, non-modulatable target, NaN/infinite/out-of-range amount and candidate-build failure leave the document, base values, graph and selection unchanged. |
| UI | Actual drop-to-control, preview cancellation, accessible amount entry/inversion/disconnect, independent badges, base-value retention and telemetry stale/poly behavior. |
| Engine | Match existing D7 normalized sum/clamp/taper semantics for mono/poly sources; zero amount does not change base or delete the edge; deterministic within-build offline renders. |
| Realtime | Zero allocations/locks/I/O in process/event/reset; graph preparation/retirement off callback; running edit evidence follows #186 rather than introducing a new swap path. |
| Compatibility | Existing v1 migration and v2 unknown-data preservation stay green; no stable ID, ABI, state-version or device-authority change. |

Approval checkpoint: Claude explicitly approves this representation and the
initial amount/gesture choices on #177 before any implementation issue begins.
That later task must name scope/budget, real descriptor fixtures and executable
acceptance commands. No device, MIDI backend or audio-output changes belong here.

Design-PR acceptance: at most 200 changed lines in this one file;
`python -m unittest tools/foundation/tests/test_doc_links.py` passes; planner
comment approving the representation remains pending. Tier T2-Claude.
