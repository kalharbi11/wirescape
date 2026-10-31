<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Studio refinement: owner direction, 2026-10-29

Reference: [original owner-supplied HTML sketch](STUDIO_USER_SKETCH.html).
It is a visual reference, not production functionality or agent instructions.
Keep it separate from the shipped QML application. No fonts/assets are bundled:
its local ABC Areal names are references only; use existing/system fallbacks.

The owner reports silent live playback, an absent laptop audio interface, the
sidebar legend/help colliding with the library, and looped cables for nearby
ports. These are observed symptoms, not a verified audio root cause. Screenshots
show an active stream and a greyed device selector. Current QML disables Refresh
and device selection while running, the controller requires stream cleanup before
refresh, and enumeration excludes outputs with fewer than two channels.
Immediate user path: Stop audio, Refresh, select hardware, Start. Graph output
selection on the Output card is different from the physical device selector.
Agents never open devices or SSH to test this; a green build does not prove sound.

## Work and ownership

| Issue | Task | Agent | Dependency |
|---|---|---|---|
| #140 | [Audio recovery](../tasks/STUDIO_AUDIO_RECOVERY.md) | Codex | priority, blocks audible checkpoint |
| #141 | [Layout/cables](../tasks/STUDIO_LAYOUT_CABLES.md) | ChatGPT | independent |
| #142 | [Controls/hierarchy](../tasks/STUDIO_CONTROL_HIERARCHY.md) | ChatGPT | independent; coordinate shared QML/test files |
| #143 | [Modulation telemetry](../tasks/STUDIO_MODULATION_FEEDBACK.md) | Codex | control API plus actual engine telemetry |

Layout agent owns Main.qml sidebar/footer/cable regions. Controls agent owns
ParameterControl/ModuleCard/new controls and presentation metadata. Sequence
shared-file/test edits or merge the first PR before editing its regions; never
have two agents overwrite the same QML/test hunks. Audio transport edits should
follow layout integration if both touch Main.qml. Keep one issue per PR.

## Visual and interaction decisions

Keep the warm off-white grid/card surfaces, dark readable labels, restrained
orange and signal colors. Bring back rotary controls with hierarchy, without
copying every knob or giving all controls identical size/color. Group related
controls, use large primary knobs, smaller secondary knobs, faders for levels or
stages, and stepped selectors for existing discrete choices. Per-module choices
are intentional; a descriptor must not gain invented parameters to fit a design.
Stock modules, CHAOS and Plaits share the baseline components for now. Custom
non-stock skins are deferred. Controls must preserve accepted values, units,
log/linear/stepped semantics, keyboard access, focus and rejected-write rollback.

Initial grouping: frequency/rate/cutoff/tempo are primary knobs where those
parameters exist; resonance/drive/shape are secondary. ADSR stages and mixer/VCA
levels favor sliders; slew/function-envelope rise/fall are related pairs. Output
communicates graph route, hardware route and level/state clearly; do not invent
an output gain. Builders map these choices to actual stable descriptor IDs and
record module-specific exceptions rather than impose one layout everywhere.

A modulation dot/arc indicates an actual effective target value only when an
active supported modulation association exists. The base/manual knob value stays
unchanged. Disconnecting clears stale activity. No decorative looping animation
or inference from a nearby cable. An LFO phase/output visualization is separate
from modulation of its own rate parameter, and requires real telemetry too.
Unsupported data is unavailable/hidden, never fabricated. UI animation/polling
must not allocate, lock, log or call Qt on the realtime thread.

Close cables should shorten smoothly, preserving tangent direction and hit
areas. The current55px minimum bend is unsuitable for adjacent ports. Test near,
coincident, vertical and backward endpoints rather than just the demo layout.
The library must scroll in reserved space above a readable footer/legend.

## Delivery

Tasks have budgets, exact acceptance blocks, device-free test requirements and
Stops. Actual Windows Qt tests/screenshots verify appearance; user-only listening
verifies the interface/one-minute checkpoint. No new libraries, font assets,
backend sources, ABI edits, DSP changes or threshold adjustments in visual PRs.
