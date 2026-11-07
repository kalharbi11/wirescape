<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Engine process ownership and bounds

`wirescape-engine` is Qt-free C++17. It reuses AppEngine's builtin catalogue,
`load_runtime_patch`, `midi::Host` and the dynamically loaded PortAudio service.
There are no new dependencies or sockets. Existing pinned nlohmann/json and
libremidi dependencies retain their existing component licences and revisions.
The three demo patches are runnable execution documents, never rewritten saves.

The main thread is the sole control executor. It owns the catalogue, execution
copy, generation, ID history, settings and MIDI host. The reader accepts UTF-8
NDJSON into at most 256 pending entries. It discards excess bytes through LF
without growing its 2 MiB buffer. Strict parsing rejects duplicate object keys,
BOMs, invalid UTF-8 and nesting deeper than 64. Saturated commands become ordered
`busy` markers; the reader waits for marker capacity, so overload cannot grow an
unbounded second queue or reorder terminal replies. The writer alone serializes
stdout. A non-realtime watchdog fails closed (exit 3) when a pipe write or
shutdown cannot finish within five seconds. This is an unavailable transport,
not successful EOF teardown. Replies have priority over single coalesced status/telemetry slots, with
at most 256 pending reliable messages. Slow output can block control but never
audio/MIDI callbacks. Diagnostic output uses stderr only. EOF discards a final
partial line, drains complete commands/replies, joins ingress then audio, exits 0.

The audio owner is either the control-side EngineOwner borrowed by a paced null
sink, or the Stream Context's exclusive EngineOwner. No no-op deleter or borrowed
unique_ptr is used. Stream allocates scratch/context before moving ownership.
Stop joins MIDI producers while the stream still owns its engine, then attempts
stop, abort if necessary, close and terminate. An unproved stop/close combined
with failed terminate deliberately quarantines the complete Context and engine
until process exit, matching play's existing lifetime policy. The PortAudio
callback body and callback_render seam remain unchanged. Quarantine is fatal
for the sidecar (exit 2), never interpreted as an EOF success or safe engine
return. Normal destructors do not propagate exceptions. A constructor/start
failure returns the engine only after safe retirement. PortAudio stays resident.

A candidate is fully validated and prepared before touching live ingress/audio.
Validation failures leave the active graph and generation unchanged. Graph
commits stop ingress, panic old notes, stop/join audio, replace ownership, restart
and then increment generation. Startup/device failures are separate from graph
validation: hardware can disappear during reopening, producing a no-device state
and retry, never a fabricated replacement device. A subscription startup failure
restores the previous graph and attempts its restart. Such a service failure may
interrupt sound and release held notes; no sample-continuity guarantee is made
for failed hardware. Settings writes use a same-directory temporary file plus
atomic replacement; a failed selection/persistence attempts the prior selection.

**D5 fallback (Keel ruling, #247):** AppEngine's internal master-output ramp
attenuates the final mixed audio linearly over at most 10 ms. The callback
retains sole ownership of current gain; a single atomic phase provides the
control request and full-silent-block acknowledgment. On a validated swap
the control executor waits for a zero block, stops/join sources, activates
the prepared candidate at zero gain and waits for fade-in to begin. A
150 ms deadline bounds the device-free swap path; service failure rolls
back to the old graph. The PortAudio callback body remains unchanged.

Successful loadGraph starts a new document ID namespace. Incremental removal
retires an ID until another successful loadGraph. Refused loads reset nothing.
Every terminal reply/push carries the process token and current decimal-string
generation. Outstanding correlation IDs are bounded by the command queue;
nonreuse after completion is the protocol client's session-wide obligation. Parameter events update the execution copy only after queue insertion
so the next topology candidate retains accepted knob values. Top-level
setModAmount amount takes precedence; cable identity excludes amount/unknown
metadata. The UI remains the editable-document owner.

Without confirmation no real stream or MIDI endpoint opens. Explicit null mode
overrides both flags, uses the paced sink and a fake M8 endpoint/USB Stereo
catalogue. These names are test fixtures, never fallbacks in the real catalogue.
The existing real PortAudio enumeration is restricted to stereo outputs and
selection uses exact unambiguous names. MIDI IDs are name|system_name. Its
bounded observer decorator forwards original MIDI packets to the existing
service while maintaining per-endpoint/channel held-note atomics. Source close
clears its notes; renderer keys are tracked by control. The existing MIDI worker
refreshes endpoint observations every 500 ms; Session::poll compares these cached
lists and emits the existing devices push without stopping render. Saved system
identity is authoritative, so a same-name surviving port cannot replace a missing
input; legacy name-only selections require an unambiguous name. Disconnect chokes
are drained with the old identity before reopening, including backends delivering
a first callback synchronously during open. Enable choices remain in the host's
saved selection vector across absence and return. MIDI 1 zero-velocity
NoteOn is a release; MIDI 2 zero-velocity NoteOn remains a NoteOn. No JSON,
allocation or locks occur in MIDI packet callbacks. Fake injection is available
only through the internal fake backend test surface, never a wire command.

Status is limited to 10 Hz and telemetry to below 30 Hz. Unsupported telemetry
is empty. Aggregate peak is published only when exactly one essentials.output
is identically routed to stereo buses 0 and 1, with no other routes; both levels
then equal that measured mono peak. Multiple/unequal output routes remain
unavailable. CPU/driver xruns come from the running stream; no stream reports
CPU 0. Latency is the specified calculated estimate. No scope/effective/phase or
MIDI activity values are invented. MIDI activity decays from actual packet
timestamps; idle sources have no activity entry. Tests exercise pipes, refused graph preservation,
1000 queued parameters, allocation-free rendering, backend note observation,
backpressure and EOF/restart with devices off. CTest names include abi2_ so the
existing ASan/UBSan/TSan jobs select them, and sidecar_ for task acceptance.

`--bench-summary` requires `--null-audio`. On clean EOF shutdown, after joining
the sink thread, it prints exactly one summary line to stderr (stdout is unchanged):
```
bench blocks=<u64> p50=<int %> p99=<int %> max=<float %> overruns=<u64>
```
Percentages measure render wall time only, as a percentage of the block period;
they exclude the pacing wait. `p50` and `p99` are lower edges of 1% histogram
bins (bin 200 includes all values >=200%); `max` retains the measured percentage.
`blocks` counts rendered blocks. `overruns` counts blocks finished after their
deadline: a stall followed by catch-up counts every late block. Deadlines keep
advancing by one block period; a late block does not reanchor the pacing clock.
The histogram spans the session, including sink restarts, and is read after join.
