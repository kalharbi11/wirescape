<!-- SPDX-License-Identifier: GPL-3.0-only -->
# OSC Out (`osc.out`), contract v1 (Keel, 2026-11-13)

Owner request 2026-11-13: send CV, gates and notes from a patch to a program on the **same machine**
(first target: a Python script on the Pi that forwards to a microcontroller). Part of v1.0.0 (PLAN D21).
This file is the contract for packets O1–O5 (#730–#734). A change to IDs, the config shape, the
threading model or the wire format below is a technical stop (Keel).

## 1. Module (IDs are permanent)
- Module id `osc.out`, name "OSC Out", category "I/O", library `ws2_osc_out`, ABI 2, GPL-3.0-only.
- Inputs: `row1` … `row8` (signal; `WS2_CV_BI` hint; **poly `FIRST`**, so only channel 0 is read, and the
  card says so), `notes` (event, `WS2_EV_NOTES`, poly). No outputs. No parameters (everything is
  config, applied at prepare through the normal replan).
- Not in the web/WASM catalogue (D9: only loadable modules appear). Studio greys it out on Web.

## 2. Patch config (`modules[i].config`, strict like `osc.in`)
```json
{ "version": 1, "port": 9001, "rate_hz": 100,
  "rows":  [ { "address": "/ws/row/1", "kind": "cv", "deadband": 0.001, "enabled": true } ],
  "notes": { "address": "/ws/note", "enabled": false } }
```
- `version`: absent or 1. `port`: 1..65535, default 9001. `rate_hz`: 1..500, default 100 (per row).
- `rows`: up to 8. Missing rows get defaults: address `/ws/row/<n>`, kind `cv`, deadband 0.001, and
  enabled `true`. `kind` is `cv` or `gate`. `deadband` is 0..1. The `address` must start with `/`, be 2..63
  bytes of printable ASCII and contain none of `#*,?[]{}` or spaces. `enabled` is absent or `true`, and writers emit it
  only when `false` (same as osc.in #612).
- The destination is always `127.0.0.1` in v1. There is **no `host`/`lan` key**. A future LAN
  destination bumps `version` (and needs `--confirm-lan`), so older engines refuse it.
- Unknown keys round-trip unchanged and are ignored with bounded diagnostics (same rule as osc.in).
  An invalid known value refuses activation of that module, not the whole patch.

## 3. Wire format (OSC 1.0, one message per datagram, no bundles in v1)
| Source | Address | Args | When |
|---|---|---|---|
| cv row | row address | `f` value | value moved more than `deadband` since the last send, at most `rate_hz` per row; the latest value is always sent eventually (trailing send) |
| gate row | row address | `f` 1.0 / 0.0 | every crossing of 0.5 (rise, fall). Never rate-limited away: a pulse shorter than a poll still sends 1 then 0, in order |
| notes | notes address | `i` key, `f` velocity (0 = off), `i` channel | each note on/off, in order |
On load, after the first rendered block, each enabled row sends its current value once.

## 4. Threading (realtime rules)
- **Render (module DSP):** per block, writes per enabled row into a host-owned `osc::OutBus`
  (fixed size, lock-free): `std::atomic<float> value` (channel 0, last sample of the block),
  `std::atomic<uint32_t> rises, falls` (gate crossings counted inside the block, sample by sample),
  and an SPSC ring of 64 note events (`key`, `velocity`, `channel`). A full ring increments
  `std::atomic<uint32_t> dropped`. No allocation, locks, syscalls, strings or logging.
- The bus pointer is delivered in the binary config struct (`osc::OutConfiguration::bus`), the same
  pattern as `osc::Configuration::statistics`. With `bus == nullptr` (unit tests, web), the module
  writes to a private bus and nothing is sent.
- **Exact shared types** (O1 lands them verbatim in `native/io/osc/out_bus.h` and
  `native/modules2/osc_out/config.h`; O2 codes against them from day one):
```cpp
namespace wirescape::osc {
struct OutRow {                                  // render writes, sender reads
    std::atomic<float> value{0};                 // channel 0, last sample of the latest block
    std::atomic<uint32_t> rises{0}, falls{0};    // gate rows: monotonic crossing counters (wrap ok)
    std::atomic<uint32_t> blocks{0};             // bumped after value/rises/falls are stored (release)
};
struct OutNote { int16_t key = 0; int16_t channel = 0; float velocity = 0; }; // velocity 0 = off
struct OutBus {
    std::array<OutRow, 8> rows{};
    std::array<OutNote, 64> notes{};
    std::atomic<uint32_t> note_head{0}, note_tail{0}; // SPSC: render pushes head, sender pops tail
    std::atomic<uint32_t> dropped{0};                // ring full on the render side
};
struct OutRowConfig { char address[64]{}; uint32_t kind = 0; /*0 cv, 1 gate*/ float deadband = 0.001f; uint32_t enabled = 1; };
struct OutConfiguration {
    uint32_t version = 1, port = 9001, rate_hz = 100;
    std::array<OutRowConfig, 8> rows{};          // defaults "/ws/row/<n>"
    OutRowConfig notes{};                        // defaults "/ws/note", enabled = 0; kind/deadband unused
    OutBus *bus = nullptr;                       // host-owned; outlives the module instance
};
bool out_configuration(const uint8_t *data, uint32_t bytes, OutConfiguration &c); // like configuration()
}
```
- **Control (sender):** one control-owned worker thread in the sidecar, beside the OSC In worker (it
  may share the `osc_service` worker loop). It polls each bus every 1 ms (up to 2 ms when idle),
  encodes with `native/io/osc/codec.h` `encode`, and calls `sendto` on one unconnected non-blocking UDP socket bound to
  `127.0.0.1:0`. `EWOULDBLOCK`/`ECONNREFUSED` count as `dropped`; nothing blocks. It stops and
  joins before graph retirement, replan or audio teardown, exactly like OSC In.
- Sending to loopback is allowed in every mode, including `--null-audio` tests. Nothing ever sends
  beyond loopback in v1.

## 5. Protocol (additive; ENGINE_PROTOCOL §status)
- `hello.capabilities` gains `"osc.out"` when the sender is installed.
- `status.osc.out?: [{module: string, port: integer, state: "sending"|"disabled"|"error",
  sent: integer, dropped: integer, last_error?: string}]`, at most 32 entries, no payloads.
  `disabled` = all rows and notes disabled (no socket). Readers ignore unknown fields.

## 6. Tests (device-off; loopback only)
- Module: rows, deadband, gate crossings within one block, ring overflow → `dropped`, no-bus mode,
  config validation (every invalid field), and that unknown keys are preserved.
- Sender: a localhost UDP receiver on an OS-assigned port. LFO → row1 arrives at ≤ `rate_hz` (+1 per second),
  a 1-sample gate pulse arrives as exactly 1 then 0, notes arrive in order, replan/EOF joins the worker,
  no listener → `dropped` grows and the engine keeps running.
- Pi first: the O2 PR (#731) carries a `pi:` line and `ci/pi`. On the Pi runner the receiver counts messages
  for 60 s at 8 rows × 100 Hz with CPU and dropouts recorded (O5, #734).
