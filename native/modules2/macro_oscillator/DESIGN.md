<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Pinned DSP adapter and support audit

Macro Oscillator: DSP port of Mutable Instruments Plaits by Emilie Gillet,
`pichenettes/eurorack` commit `f4cfa7697095a28d292bff758beb51deb49b5a86`;
stmlib commit `e3bd7c9cc00e4364166f9905c0509b6ffd0535ec`.
Original code under MIT; Wirescape adapter under GPL-3.0-only. Sources are fetched
by immutable archive URLs plus SHA256, not vendored. Every compiled upstream DSP
translation unit and generated resource retains its original MIT notice; the
stmlib MIT licence applies to `dsp/units.cc` and utility/DSP headers. STM firmware,
programming tools, serial code, VCV code and graphics are excluded.

## Source closure and hardware boundary

Compile `plaits/dsp/**/*.cc`, `plaits/resources.cc` and `stmlib/dsp/units.cc`.
Upstream DSP source text is unchanged. `TEST` selects upstream UserData::ptr's
null-returning implementation, so the factory preset path never reads a hardware
flash address. Firmware/device initialisation and UserData::Save are never called.
All 24 engine entries remain selectable; no engine is excluded.

## Random state isolation

Upstream `stmlib::Random` has process-global mutable state. `rng_scope.h` includes
that original declaration under an unused private compatibility name and supplies
an API-compatible storage wrapper with the same 32-bit LCG arithmetic. This is an
instance-state boundary, not a synthesis rewrite: each voice saves/restores its
own RNG register around Init/Render; the temporary register is thread-local.
GNU builds reserve static TLS at load time, preventing lazy TLS allocation on the
render thread. No lock serialises engines. Concurrent and interleaved noise-engine
tests compare separate instances against a standalone stream; different seeds must
differ. Global constant lookup tables are shared read-only.

## Timing, state and memory

Each prepared channel owns a Voice object (FAST) and one 16,384-byte workspace
(LARGE), reused by its engines just as upstream does. There are eight maximum
prepared channels and a fixed 12-frame FIFO each. Controls are sampled at quantum
start; Render runs only when twelve input frames have accumulated. Output starts
with twelve zero frames; both streams use upstream signed-16-bit/32768 scaling.
All allocation is in prepare; process and reset are allocation-free. Reset rebuilds
the Voice object in already reserved storage and initialises its workspace with a
seed mixed by channel. No upstream object pointers are serialized. State v1 is a
version marker; the host stores parameters, and loading valid state resets DSP.
This follows the planner's explicit patch-state decision, not mid-note continuation.

Direct reference tests instantiate pinned Voice and compare models 0, 8, 10 and 13
bit-for-bit after the 12-frame delay. All 24 models cover the host-block matrix,
reset, finite output and constant-flag checks. Tests reject unsupported rates and
insufficient prepare storage. ARM/Daisy support is reported from actual CI/probes;
there is no unmeasured bare-metal compatibility claim.
