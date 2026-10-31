<!-- SPDX-License-Identifier: GPL-3.0-only -->
# WIRESCAPE — FULL MODULE AUTHORING GUIDE
## One-file, independent module-building reference for people and AI agents

**Snapshot:** 2026-10-30, public main at **4bd78d57f197c686dad20a6a0367467ad58e118d**. This is a source-audited documentation snapshot, not a build or hardware certification. Status on an unmerged PR is **not** a feature on main. When updating, compare the latest main with the source pointers at the end. For the maintained step-by-step guide, see [docs/SDK_AUTHOR_GUIDE.md](docs/SDK_AUTHOR_GUIDE.md), especially its [worked examples](native/sdk/examples/); this file is the single-file, extractable handoff for sharing the SDK source payload.

**Current catalogue check (2026-10-31):** main at **b0d19461e9a4fc9b873ae0f82aa0bdd8d4627efa** has 19 registrations in `native/control/module_catalog.inc`, including `ports.macro_oscillator`. PR #171 merged after the pinned snapshot, so the historical table below still describes 4bd78d5 while the macro-oscillator row's former “not in catalogue / open PR” status is no longer current. This source check does not claim that the embedded snapshot compiled or certify hardware behavior.

**Scope:** Author a new C++17 **ABI 2 native DSP module** independently, build and test its binary without the entire Wirescape application, then submit it for Wirescape integration. This one Markdown document contains the SDK source headers, a working starter module, a manifest, callback tests, a standalone CMake project, and its extraction instructions. It is **one text file, not a ZIP**. The compiler, CMake, Qt application, real hardware backends, and the full Wirescape host implementation are not embedded.

**Never use:** the legacy Python/browser module.json + plugin.py/register() kit or its scan/enable workflow for current native modules. Those files are archived under legacy/ and do not implement the current ABI 2 system. Do not confuse old native ABI 1 under native/modules/ with ABI 2 under native/modules2/.

## 1. Read this first: what is actually available

This matrix means **in main at the snapshot**, not "implemented on somebody's open branch" and not "device-certified."

| Feature / module | State | Implication |
|---|---|---|
| **13 Essentials**: VCO, LFO, VCF, ADSR, VCA, Clock, Function Envelope, Ladder, Mixer, Noise, Output, Scale/Offset, Slew | **Source-built and registered in Studio** | Suitable DSP implementation patterns, with stable essentials.* identities |
| **host.keyboard** | **Registered** | Polyphonic keyboard/note-to-CV path, shared allocator |
| **essentials.chaos_lite** | **Registered** | Stereo-pair outputs, up to 8 voices, two-instance offline Pi workload tested |
| **midi.in, midi.to_cv, midi.cc_to_cv** | **Registered** | MIDI service and ported modules exist; inbound USB MIDI uses bounded host backend; synthetic coverage; real hardware still needs owner checks |
| **ports.macro_oscillator** | **Built/tested SDK DSP; absent from the app catalogue at this snapshot** | 24 models; **48 kHz only**, up to 8 voices, 12-frame internal buffering; PR #171 subsequently registered it in main (see current catalogue check above) |
| **OSC In + Monitor** | **Open PR #82, not merged** | Do not assume working Studio/host services |
| **CLI playback and native v1/v2 patch loading** | **Code present** | Offline playback / explicit device gating; patch and module compatibility still need testing |
| **Qt Studio on Pi 5** | **Owner-reported snapshot** | On 2026-10-30, issue #174 records a Release UI build, 14/14 focused tests and fullscreen session; audible output and controller functioning were **not certified** |
| **ABI2 1–16 channel signal/event engine, transport, feedback scheduling, parameter modulation** | **Engine/SDK implemented and tested to documented scope** | Not all engine capabilities have Studio authoring controls |
| **Studio CV-to-knob modulation wires, friendly stereo paired wiring, manual feedback edges** | **Not complete in Studio** | Current open design/implementation tasks #177, #178, #179 |
| **MIDI Coordinator / CC Learn and patch mapping service** | **Not implemented as proposed** | #163 planned; existing MIDI-to-CV and eight-slot CC-to-CV are not that coordinator/Learn service |
| **MIDI Out / MIDI Clock Out, OSC Out, UDP Send, DMX, serial, scripts, MPE** | **Not general production host capabilities** | Requires host/IO service work; an SDK event descriptor alone does not provide a socket or device |
| **CLAP/VST3 plugins and Daisy hardware adapters** | **Design targets, not turnkey exports** | Pure DSP portability does not imply built/tested adapters |

The source of truth for what appears in the app is `native/control/module_catalog.inc` together with `native/control/module_catalog.h` and `native/control/app_engine.cpp`. At the pinned snapshot that catalogue has **18** registrations (13+keyboard+CHAOS-lite+3 MIDI); current main has 19 after #171 added the macro oscillator. The older docs/STATUS.md and docs/ARCHITECTURE.md describe pre-Jam architecture/availability and must not override newer compiled source and merged PRs.

## 2. What module types can you build independently?

| Category | Independent ABI2 module? | Boundary |
|---|---|---|
| Oscillators, additive/wavetable synths, noise, ring mod, filters, waveshapers, drive, VCA, mixers, crossfades, utilities | **Yes** | Plain DSP, finite audio, no external host services |
| Envelope/function generators, LFOs, clocks, logic, slew, comparators, quantizers, random, sample-and-hold | **Yes** | Signal-level modules; enforce agreed pitch/trigger conventions |
| Delay, chorus, flanger, reverb, distortion, freeze and loop buffers | **Yes, with budgets** | Allocate delay memory in prepare, report requirements, verify latency/CPU/Pi limits; filesystem recording is separate |
| Step sequencers, arpeggiators, chord tools, MIDI/CV logic and event transformers | **Yes, with care** | Use sample-offset events, transport, stable note IDs, bounded queues and shared voice allocator for note-to-CV |
| Poly synths, macro instruments, 8-voice CHAOS-like instruments | **Yes, with CPU/licence limits** | Prove worst-case 1/4/8/16 voice cost; third-party source must be pinned and licensed |
| Oscilloscope, FFT spectrum, graphical meters | **DSP analysis core only** | New live visual UI/worker/tap API requires host + Qt changes; don't perform FFT or logging in realtime callback |
| MIDI In/Out, OSC/UDP networking, USB, serial, DMX/Art-Net, hardware controller mapping | **Algorithm/transform module only** | Actual endpoints, permission, device discovery, scheduling, reconnection and networking require host services |
| Arbitrary bespoke module faceplates, dynamic QML editors, sample browsers, file dialogs, waveform galleries | **Not as a DSP-only module** | The generic parameter interface is available; a new custom GUI or filesystem service requires host/UI work |
| CLAP/VST3 export, Daisy builds, standalone executable products | **Not automatically** | Adapter + toolchains + platform tests are separate work |
| Full MPE, user-facing feedback editor, modulation-cable editor, learn system | **Not safe to promise** | ABI structures may support primitives; complete application behavior is not yet established |

**Decision rule:** if it reads signal/parameter/event buffers and emits signal/event buffers with bounded preallocated state, the DSP can normally be built independently. If it needs to operate the OS, filesystem, audio/MIDI device, network socket, persistent UI widget or graph editor, submit a **host change request** instead of faking support. Never change the global ABI to solve a local module problem.

## 3. The ABI2 contract to follow

- **Language & ABI:** portable C++17 class wrapped by native/sdk/wirescape_sdk.h; exported C ABI version **2**, defined in native/include/wirescape/module2.h. Do not edit those headers in a module PR. SDK/ABI source is **MIT**; app/Essentials code is GPL-3.0-only; upstream copyleft obligations still apply.
- **IDs:** use a unique stable dotted module ID such as author.module, stable lowercase port/parameter IDs, and version your descriptor. Never renumber, recycle or rename IDs after publication. The module.json **parameters** map IDs to zero-based parameter **slot indices**, not default values.
- **Cables:** planar float32 signals at audio rate; **1–16 channels represent voices, NOT stereo**. Left/right are separate ports optionally sharing a stereo_pair hint. Up to 32 input/output ports and 32 parameters are supported; max 1,472 message bytes. A mono input broadcasts in poly use; WS2_POLY, WS2_SUM and WS2_FIRST are the channel policies.
- **Units:** nominal audio -1..+1; pitch 1.0/octave with 0 = C4/MIDI 60 (261.6256 Hz); bipolar CV -1..+1; unipolar CV 0..1; gates/trigger 0/1 with Schmitt high above 0.5, low below 0.25. Trigger width >=1ms. Parameter frequency in Hz, time in seconds.
- **Ports:** signal ports carry audio, CV, pitch or gates; event ports carry notes, MIDI and bounded messages. Hints describe meaning but do not prohibit signal connections. For event output, report zero channels and use b.emit(index, event); output port index includes signal outputs.
- **Parameters:** ws::param declares min/max/default/unit/flags/taper/step. Host presents already-modulated values via b.param(slot); use WS2_STEPPED|WS2_AUTOMATABLE and step 1 for switches and counts. Use WS2_PER_VOICE for per-voice modulation. A cable to a parameter may be supported by the engine but is not a finished Studio gesture.
- **CONSTANT:** if WS2_CONSTANT, **only sample zero** is valid for the whole block. ws::Signal::ch(channel)[sample] handles this; ws::Signal::row(channel,scratch,frames) produces a pointer when needed. Set output constant only if the output truly does not change. Oscillators/noise are not constant.
- **Lifecycle:** construct/create, query, memory_requirements, and prepare occur quiescently; allocate only through ws::Setup::alloc in prepare, with an explicit size budget. reset, process, and event handlers must never allocate, lock, perform IO, log, throw, sleep or wait. Every loop has bounded cost. Avoid Qt, exceptions, STL growth, platform device code in DSP.
- **Timing:** frames from 1 up to the prepared maximum, including 17 and odd sizes. Split across arbitrary blocks with identical output. Use b.frame_position(), b.transport() (nullable) and event offsets for sample-accurate behavior. Do not assume physical timing or a running transport.
- **Events:** use b.events_in(port) or b.events() and b.split(range,event); ws2_event has struct_size, type, offset, note_id, source_id, channel, key and values. Return from b.emit can be false on saturation. Pending note-off/choke must be handled without busy-waiting; preserve source/note identity on steals/disconnect/panic. Never keep borrowed message data past process.
- **State:** host saves parameters; save only non-parameter state if needed, with a version and ws::Writer::u32/f32 and corresponding Reader methods. Stage reads, reject truncated states without mutating current state; return WS2_UNSUPPORTED for unknown versions. Avoid raw packed structs and host-endian dumps.
- **Memory:** ws2_memory reports FAST/LARGE allocation requests. Use fixed arrays for per-voice state (maximum 16 channels) and prepare-time scratch sized to max_block_frames. Arrays that hold 16 voices must not go on a tiny Windows test thread stack.
- **Limits:** no realtime malloc/new, direct socket/device/file work, unbounded payloads, guaranteed zero-overflow or general automatic latency compensation claims. The wrapper validates finite output samples and silences invalid blocks, which is an error path, **not** a substitute for numerical stability.

## 4. Starter project in this document: create source files

The **single Markdown handoff** below embeds an extractable minimal SDK+module project. The extracted project uses multiple ordinary source files **at build time**; that does not change this guide's one-file distribution. It has no runtime dependency on Qt or the Wirescape app. You still need a local C++17 compiler and CMake >=3.20.

1. Save this document as **WIRESCAPE_FULL_MODULE_GUIDE.md**.
2. From its directory, run the standard-library-only Python extractor below (Python 3.9+). It writes into a fresh **wirescape_starter/** directory, refusing to overwrite existing source. Read the generated files before compiling untrusted code.
3. Build/test the starter; its test runs via the real ABI descriptor callbacks:
   - Linux/macOS: **cmake -S wirescape_starter -B wirescape_starter/build && cmake --build wirescape_starter/build && ctest --test-dir wirescape_starter/build --output-on-failure**
   - Windows Developer PowerShell (VS/CMake installed): the same three commands.
4. Change module/starter_gain/dsp.h and module.json for the requested module. Keep module.cpp the registration shim, write behavioral tests in tests.cpp, and add README.md and DESIGN.md when delivering it. Preserve SPDX headers and record pinned third-party sources.
5. For **app integration**, a maintainer brings module source into native/modules2/<id_slug>/, registers a **ws2_<name>** library and module test in appropriate native CMake, includes its class header in native/control/module_catalog.h, adds one WS_APP_MODULE(Class) in native/control/module_catalog.inc, updates catalogue assertions, and runs full project CI. Copying just a binary or module.json to the app **does not auto-install an ABI2 module**.

~~~~python
# Extract from the one Markdown file into the independently buildable source tree.
from pathlib import Path
import re
import sys

source = Path(sys.argv[1] if len(sys.argv) > 1 else "WIRESCAPE_FULL_MODULE_GUIDE.md")
text = source.read_text(encoding="utf-8")
pattern = re.compile(
    r"^<!-- BUNDLE_FILE: ([A-Za-z0-9_./-]+) -->\n~~~[A-Za-z0-9+]*\n(.*?)\n~~~(?=\n|$)",
    re.MULTILINE | re.DOTALL,
)
matches = pattern.findall(text)
assert len(matches) == 10, f"expected 10 embedded files, found {len(matches)}"
root = Path("wirescape_starter").resolve()
for rel, contents in matches:
    dest = (root / rel).resolve()
    assert dest.is_relative_to(root), rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    if dest.exists():
        raise SystemExit(f"Refusing to overwrite: {dest}")
    dest.write_text(contents + "\n", encoding="utf-8")
    print(dest)
~~~~

**No hidden dependency:** the next section includes all five SDK/ABI headers used by the starter plus module code, JSON, tests, and CMake. The exact headers were copied from the public source at the snapshot SHA. If the SDK later changes, regenerate them from the current main rather than mixing ABI versions.

## 5. Embedded extractable project sources

The following blocks are intentionally long because they make this reference self-contained. Do not edit an SDK header inside an individual module to add an API that the current host does not have.

<!-- BUNDLE_FILE: include/wirescape/module2.h -->
~~~c
/* SPDX-License-Identifier: MIT */
#ifndef WIRESCAPE_MODULE2_H
#define WIRESCAPE_MODULE2_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#define WS2_CALL __cdecl
#define WS2_EXPORT __declspec(dllexport)
#else
#define WS2_CALL
#define WS2_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define WS_ABI2_VERSION 2u
#define WS2_MAX_CHANNELS 16u
#define WS2_MAX_PORTS 32u
#define WS2_MAX_PARAMS 32u
#define WS2_MAX_PAYLOAD 1472u
#define WS2_DIRECT_PORT UINT32_MAX
enum ws2_result { WS2_OK=0, WS2_INVALID=1, WS2_CAPACITY=2, WS2_UNSUPPORTED=3, WS2_EXCEPTION=4, WS2_STATE=5 };
enum ws2_port_kind { WS2_SIGNAL=1, WS2_EVENT=2 };
enum ws2_hint { WS2_ANY, WS2_AUDIO, WS2_PITCH, WS2_CV_BI, WS2_CV_UNI, WS2_GATE, WS2_TRIGGER, WS2_EV_NOTES, WS2_EV_MIDI, WS2_EV_MESSAGE };
enum ws2_poly { WS2_POLY, WS2_SUM, WS2_FIRST };
enum ws2_signal_flags { WS2_CONSTANT=1u, WS2_CONNECTED=2u };
enum ws2_param_flags { WS2_MODULATABLE=1u, WS2_PER_VOICE=2u, WS2_AUDIO_RATE_OK=4u, WS2_STEPPED=8u, WS2_AUTOMATABLE=16u, WS2_HIDDEN=32u };
enum ws2_taper { WS2_LINEAR, WS2_LOG, WS2_EXP };
enum ws2_memory_hint { WS2_FAST=1, WS2_LARGE=2 };
enum ws2_event_type { WS2_NOTE_ON=1, WS2_NOTE_OFF, WS2_NOTE_CHOKE, WS2_NOTE_EXPRESSION, WS2_PARAM, WS2_PARAM_MOD, WS2_TRANSPORT_EVENT, WS2_MIDI, WS2_MESSAGE };
enum ws2_expression { WS2_EX_PITCH, WS2_EX_PRESSURE, WS2_EX_TIMBRE, WS2_EX_PAN };
enum ws2_message_kind { WS2_MSG_OSC=1, WS2_MSG_UDP, WS2_MSG_TEXT, WS2_MSG_BYTES, WS2_MSG_STRUCT };
enum ws2_transport_flags { WS2_PLAYING=1u, WS2_RECORDING=2u, WS2_LOOPING=4u };

typedef struct ws2_signal { uint32_t struct_size; const float *const *data; uint32_t channels,flags; } ws2_signal;
typedef struct ws2_signal_out { uint32_t struct_size; float *const *data; uint32_t channels,flags,max_channels; } ws2_signal_out;
typedef struct ws2_port {
    uint32_t struct_size; const char *id,*name; uint32_t kind,hint,poly;
    const char *stereo_pair; float unpatched_value;
} ws2_port;
typedef struct ws2_param {
    uint32_t struct_size; const char *id,*name,*unit;
    float minimum,maximum,initial; uint32_t legacy_index,taper; float curve,step;
    uint32_t flags,option_count; const char *const *options; uint32_t decimals;
    int (WS2_CALL *value_to_text)(float,char*,uint32_t);
} ws2_param;
/* An event belongs to one process call. MESSAGE data is borrowed from that
   call's bounded arena. Never retain it after process returns. */
typedef struct ws2_event {
    uint32_t struct_size,type,offset,port,flags;
    int32_t note_id,channel,key; uint32_t source_id,index;
    float value,value2; uint32_t expression,message_kind,bytes;
    const uint8_t *data; uint32_t ump[4],ump_words;
} ws2_event;
typedef struct ws2_event_list { uint32_t struct_size,count; const ws2_event *items; } ws2_event_list;
typedef struct ws2_event_out {
    uint32_t struct_size; void *ctx; uint32_t port;
    int (WS2_CALL *push)(void*,uint32_t,const ws2_event*);
} ws2_event_out;
typedef struct ws2_transport {
    uint32_t struct_size,flags; double bpm,beat,bar_start;
    uint32_t ts_num,ts_den; double loop_start,loop_end;
} ws2_transport;
typedef struct ws2_host {
    uint32_t struct_size; void *ctx;
    void *(WS2_CALL *alloc)(void*,size_t,size_t,uint32_t);
} ws2_host;
typedef struct ws2_config {
    uint32_t struct_size; double sample_rate;
    uint32_t max_block_frames,max_channels; uint64_t seed;
    const uint8_t *data; uint32_t bytes;
} ws2_config;
typedef struct ws2_memory { uint32_t struct_size; size_t fast_bytes,large_bytes,alignment; } ws2_memory;
typedef struct ws2_ports {
    uint32_t struct_size,input_count,output_count;
    const ws2_port *inputs,*outputs;
} ws2_ports;
typedef struct ws2_writer {
    uint32_t struct_size; void *ctx;
    int (WS2_CALL *write)(void*,const void*,uint32_t);
} ws2_writer;
typedef struct ws2_reader {
    uint32_t struct_size; void *ctx;
    int (WS2_CALL *read)(void*,void*,uint32_t);
    uint32_t bytes;
} ws2_reader;
typedef struct ws2_process {
    uint32_t struct_size,frames; uint64_t frame_position,generation; double sample_rate;
    uint32_t channels,input_count,output_count,param_count;
    const ws2_signal *inputs; ws2_signal_out *outputs; const ws2_signal *params;
    const ws2_event_list *event_inputs; const ws2_event_out *event_outputs;
    ws2_event_list events; const ws2_transport *transport;
} ws2_process;
typedef struct ws2_descriptor {
    uint32_t struct_size,abi_version; const char *id,*name; uint32_t version;
    uint32_t input_count,output_count,param_count;
    const ws2_port *inputs,*outputs; const ws2_param *params;
    void *(WS2_CALL *create)(const ws2_host*);
    void (WS2_CALL *destroy)(void*);
    int (WS2_CALL *query_ports)(void*,const ws2_config*,ws2_ports*);
    int (WS2_CALL *memory_requirements)(const ws2_config*,ws2_memory*);
    int (WS2_CALL *prepare)(void*,const ws2_config*,const ws2_host*);
    void (WS2_CALL *reset)(void*);
    int (WS2_CALL *process)(void*,ws2_process*);
    int (WS2_CALL *save_state)(void*,const ws2_writer*);
    int (WS2_CALL *load_state)(void*,const ws2_reader*);
    uint32_t (WS2_CALL *latency_frames)(void*);
} ws2_descriptor;
typedef const ws2_descriptor *(WS2_CALL *ws2_entry_fn)(void);
#ifndef WS_MODULE2_VERSION_NAME
#define WS_MODULE2_VERSION_NAME ws_module2_abi_version
#endif
WS2_EXPORT uint32_t WS2_CALL WS_MODULE2_VERSION_NAME(void);
#ifndef WS_MODULE2_ENTRY_NAME
#define WS_MODULE2_ENTRY_NAME ws_module2_entry
#endif
WS2_EXPORT const ws2_descriptor *WS2_CALL WS_MODULE2_ENTRY_NAME(void);
#ifdef __cplusplus
}
#endif
#endif
~~~

<!-- BUNDLE_FILE: sdk/wirescape_sdk.h -->
~~~cpp
// SPDX-License-Identifier: MIT
#pragma once
#include "wirescape/module2.h"
#include "signals.h"
#include "simd.h"
#include "fastmath.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace ws {
struct Info {const char *id,*name; uint32_t version;};
constexpr ws2_port port(const char*id,const char*name,uint32_t hint=WS2_ANY,uint32_t poly=WS2_POLY,
                       float unpatched=0,const char*pair=nullptr,uint32_t kind=WS2_SIGNAL) {
    return {sizeof(ws2_port),id,name,kind,hint,poly,pair,unpatched};
}
constexpr ws2_param param(const char*id,const char*name,float low,float high,float initial,
                         const char*unit="",uint32_t flags=WS2_MODULATABLE|WS2_AUTOMATABLE,
                         uint32_t taper=WS2_LINEAR,uint32_t legacy=0,float curve=4,float step=0) {
    return {sizeof(ws2_param),id,name,unit,low,high,initial,legacy,taper,curve,step,flags,0,nullptr,3,nullptr};
}
inline double to_plain(const ws2_param&p,double x) noexcept {
    x=std::clamp(x,0.,1.);
    double y=x;
    if(p.taper==WS2_LOG) y=std::exp(std::log(static_cast<double>(p.minimum))+x*std::log(static_cast<double>(p.maximum)/p.minimum));
    if(p.taper==WS2_EXP&&std::abs(p.curve)>1.e-8) y=std::expm1(p.curve*x)/std::expm1(static_cast<double>(p.curve));
    double value=p.taper==WS2_LOG?y:p.minimum+(p.maximum-p.minimum)*y;
    if(p.step>0) value=p.minimum+std::round((value-p.minimum)/p.step)*p.step;
    return std::clamp(value,static_cast<double>(p.minimum),static_cast<double>(p.maximum));
}
inline double to_normal(const ws2_param&p,double value) noexcept {
    value=std::clamp(value,static_cast<double>(p.minimum),static_cast<double>(p.maximum));
    if(p.maximum==p.minimum)return 0;
    if(p.taper==WS2_LOG)return std::log(value/p.minimum)/std::log(static_cast<double>(p.maximum)/p.minimum);
    double x=(value-p.minimum)/(p.maximum-p.minimum);
    if(p.taper==WS2_EXP&&std::abs(p.curve)>1.e-8)x=std::log1p(x*std::expm1(static_cast<double>(p.curve)))/p.curve;
    return x;
}
inline bool finite_samples(const float*data,uint32_t count) noexcept {
    uint32_t exponent=0;
    for(uint32_t i=0;i<count;++i){uint32_t bits;std::memcpy(&bits,data+i,sizeof(bits));exponent|=bits&UINT32_C(0x7f800000);}
    // Mixed finite exponents can also OR to all ones; only the exact rescan rejects.
    if(exponent==UINT32_C(0x7f800000))for(uint32_t i=0;i<count;++i)if(!std::isfinite(data[i]))return false;
    return true;
}
inline bool valid_event(const ws2_event&e) noexcept {
    return e.struct_size>=sizeof(ws2_event)&&e.type>=WS2_NOTE_ON&&e.type<=WS2_MESSAGE&&
        std::isfinite(e.value)&&std::isfinite(e.value2)&&e.bytes<=WS2_MAX_PAYLOAD&&(!e.bytes||e.data)&&e.ump_words<=4;
}
inline void silence_outputs(ws2_process*c,uint32_t max_channels=WS2_MAX_CHANNELS) noexcept {
            if(!c||c->struct_size<sizeof(ws2_process)||!c->outputs||c->output_count>WS2_MAX_PORTS)return;
            for(uint32_t j=0;j<c->output_count;++j){auto&o=c->outputs[j];
                if(o.struct_size<sizeof(ws2_signal_out)||!o.data||!o.max_channels||o.max_channels>max_channels)continue;
                bool valid=true;for(uint32_t k=0;k<o.max_channels;++k)valid&=o.data[k]!=nullptr;if(!valid)continue;
                o.flags=WS2_CONSTANT;o.channels=std::clamp<uint32_t>(c->channels,1u,o.max_channels);for(uint32_t k=0;k<o.channels;++k)o.data[k][0]=0;
            }
}
struct Setup {
    const ws2_config& config; const ws2_host& host;
    double sample_rate()const{return config.sample_rate;}
    uint32_t max_channels()const{return config.max_channels;}
    uint32_t max_block()const{return config.max_block_frames;}
    void*alloc(size_t bytes,size_t alignment=32,uint32_t hint=WS2_FAST)const {
        return host.alloc?host.alloc(host.ctx,bytes,alignment,hint):nullptr;
    }
};
struct Channel {
    const float *p=nullptr; bool is_constant=true;
    float operator[](uint32_t i)const noexcept{return p?p[is_constant?0:i]:0;}
};
class Signal {
    const ws2_signal*s_;
public:
    explicit Signal(const ws2_signal*s):s_(s){}
    uint32_t channels()const noexcept{return s_?s_->channels:0;}
    bool constant()const noexcept{return !s_||(s_->flags&WS2_CONSTANT);}
    bool connected()const noexcept{return s_&&(s_->flags&WS2_CONNECTED);}
    Channel ch(uint32_t c=0)const noexcept {
        if(!s_||!s_->channels||!s_->data)return {};
        if(s_->channels==1)c=0;
        return {c<s_->channels?s_->data[c]:nullptr,constant()};
    }
    float value(uint32_t c=0,uint32_t i=0)const noexcept{return ch(c)[i];}
    const float*row(uint32_t c,float*scratch,uint32_t frames)const noexcept {
        auto channel=ch(c);if(channel.p&&!channel.is_constant)return channel.p;
        std::fill_n(scratch,frames,channel[0]);return scratch;
    }
};
class Output {
    ws2_signal_out*s_;
public:
    explicit Output(ws2_signal_out*s):s_(s){}
    bool set_channels(uint32_t n)noexcept{if(!s_||n>s_->max_channels)return false;s_->channels=n;return true;}
    void mark_constant(bool value=true)noexcept{if(s_)s_->flags=value?WS2_CONSTANT:0;}
    float*ch(uint32_t c=0)const noexcept{return s_&&c<s_->max_channels&&s_->data?s_->data[c]:nullptr;}
    uint32_t channels()const noexcept{return s_?s_->channels:0;}
};
class Events {
    const ws2_event_list*list_;
public:
    explicit Events(const ws2_event_list*l):list_(l){}
    const ws2_event*begin()const{return list_?list_->items:nullptr;}
    const ws2_event*end()const{return list_&&list_->count?list_->items+list_->count:begin();}
    uint32_t size()const{return list_?list_->count:0;}
};
class Block {
    ws2_process&c_;
public:
    explicit Block(ws2_process&c):c_(c){}
    uint32_t frames()const{return c_.frames;}
    uint32_t channels()const{return c_.channels;}
    double sample_rate()const{return c_.sample_rate;}
    uint64_t frame_position()const{return c_.frame_position;}
    Signal in(uint32_t i)const{return Signal(i<c_.input_count?c_.inputs+i:nullptr);}
    Signal param(uint32_t i)const{return Signal(i<c_.param_count?c_.params+i:nullptr);}
    Output out(uint32_t i)const{return Output(i<c_.output_count?c_.outputs+i:nullptr);}
    Events events()const{return Events(&c_.events);}
    Events events_in(uint32_t port)const{return Events(port<c_.input_count&&c_.event_inputs?c_.event_inputs+port:nullptr);}
    bool emit(uint32_t port,const ws2_event&e)const {
        return port<c_.output_count&&c_.event_outputs&&c_.event_outputs[port].push&&
            c_.event_outputs[port].push(c_.event_outputs[port].ctx,port,&e)==WS2_OK;
    }
    const ws2_transport*transport()const{return c_.transport;}
    template<class Range,class Event>void split(Range range,Event event)const {
        uint32_t cursor=0;
        for(const auto&e:events()) {uint32_t at=std::min(e.offset,c_.frames);if(at>cursor)range(cursor,at);event(e);cursor=at;}
        if(cursor<c_.frames)range(cursor,c_.frames);
    }
};
class Writer {
    const ws2_writer*w_;
public:
    explicit Writer(const ws2_writer*w):w_(w){}
    bool bytes(const void*p,uint32_t n)const{return w_&&w_->write&&w_->write(w_->ctx,p,n)==WS2_OK;}
    template<class T>bool value(const T&x)const{static_assert(std::is_trivially_copyable<T>::value,"state value");return bytes(&x,sizeof(x));}
    bool u32(uint32_t value)const{uint8_t data[4];for(int i=0;i<4;++i)data[i]=static_cast<uint8_t>(value>>(i*8));return bytes(data,4);}
    bool f32(float value)const{uint32_t bits;static_assert(sizeof(float)==4,"float32");std::memcpy(&bits,&value,4);return u32(bits);}
};
class Reader {
    const ws2_reader*r_;
public:
    explicit Reader(const ws2_reader*r):r_(r){}
    bool bytes(void*p,uint32_t n)const{return r_&&r_->read&&r_->read(r_->ctx,p,n)==WS2_OK;}
    template<class T>bool value(T&x)const{static_assert(std::is_trivially_copyable<T>::value,"state value");return bytes(&x,sizeof(x));}
    bool u32(uint32_t&value)const{uint8_t data[4];if(!bytes(data,4))return false;value=0;for(int i=0;i<4;++i)value|=uint32_t(data[i])<<(i*8);return true;}
    bool f32(float&value)const{uint32_t bits;if(!u32(bits))return false;std::memcpy(&value,&bits,4);return true;}
};
template<class T,class=void>struct HasQuery:std::false_type{};
template<class T>struct HasQuery<T,std::void_t<decltype(std::declval<T>().query_ports(std::declval<const ws2_config&>(),std::declval<ws2_ports&>()))>>:std::true_type{};
template<class T,class=void>struct HasState:std::false_type{};
template<class T>struct HasState<T,std::void_t<decltype(std::declval<T>().save_state(std::declval<Writer>())),decltype(std::declval<T>().load_state(std::declval<Reader>()))>>:std::true_type{};
template<class T,class=void>struct HasMemory:std::false_type{};
template<class T>struct HasMemory<T,std::void_t<decltype(T::memory_requirements(std::declval<const ws2_config&>()))>>:std::true_type{};
template<class T,class=void>struct HasLatency:std::false_type{};
template<class T>struct HasLatency<T,std::void_t<decltype(std::declval<T>().latency_frames())>>:std::true_type{};

template<class T>class Module {
    struct Instance {T dsp;ws2_config config{};uint32_t inputs=0,outputs=0;};
    template<class F>static int guarded(F f)noexcept {
#if defined(__cpp_exceptions)||defined(_CPPUNWIND)
        try{return f();}catch(...){return WS2_EXCEPTION;}
#else
        return f();
#endif
    }
    static void*WS2_CALL create(const ws2_host*h) {
        if(!h||h->struct_size<sizeof(ws2_host)||!h->alloc)return nullptr;
        void*p=h->alloc(h->ctx,sizeof(Instance),alignof(Instance),WS2_FAST);
        if(!p)return nullptr;
        Instance*instance=nullptr;int result=guarded([&](){instance=new(p)Instance{};return WS2_OK;});
        return result==WS2_OK?instance:nullptr;
    }
    static void WS2_CALL destroy(void*p){if(p)guarded([&](){static_cast<Instance*>(p)->~Instance();return WS2_OK;});}
    static int WS2_CALL query(void*p,const ws2_config*c,ws2_ports*ports) {
        if(!p||!c||c->struct_size<sizeof(ws2_config)||!ports||ports->struct_size<sizeof(ws2_ports))return WS2_INVALID;
        return guarded([&](){
            if constexpr(HasQuery<T>::value)return static_cast<Instance*>(p)->dsp.query_ports(*c,*ports);
            else {*ports={sizeof(ws2_ports),static_cast<uint32_t>(T::inputs.size()),static_cast<uint32_t>(T::outputs.size()),T::inputs.data(),T::outputs.data()};return WS2_OK;}
        });
    }
    static int WS2_CALL memory(const ws2_config*c,ws2_memory*m) {
        if(!c||!m||c->struct_size<sizeof(ws2_config)||m->struct_size<sizeof(ws2_memory))return WS2_INVALID;
        return guarded([&](){*m={sizeof(ws2_memory),sizeof(Instance),0,32};
            if constexpr(HasMemory<T>::value){auto extra=T::memory_requirements(*c);m->fast_bytes+=extra.fast_bytes;m->large_bytes+=extra.large_bytes;}
            return int(WS2_OK);});
    }
    static int WS2_CALL prepare(void*p,const ws2_config*c,const ws2_host*h) {
        if(!p||!c||!h||c->struct_size<sizeof(ws2_config)||h->struct_size<sizeof(ws2_host)||
           !std::isfinite(c->sample_rate)||c->sample_rate<1||c->sample_rate>384000||!c->max_block_frames||
           !c->max_channels||c->max_channels>WS2_MAX_CHANNELS||T::params.size()>WS2_MAX_PARAMS)return WS2_INVALID;
        for(const auto&v:T::params)if(v.struct_size<sizeof(ws2_param)||!v.id||!std::isfinite(v.minimum)||!std::isfinite(v.maximum)||
            v.maximum<v.minimum||!std::isfinite(v.initial)||v.initial<v.minimum||v.initial>v.maximum||
            !std::isfinite(v.curve)||!std::isfinite(v.step)||v.step<0||v.taper>WS2_EXP||
            (v.taper==WS2_LOG&&v.minimum<=0))return WS2_INVALID;
        auto&x=*static_cast<Instance*>(p);x.config=*c;
        return guarded([&](){ws2_ports ports{sizeof(ports),0,0,nullptr,nullptr};int r=query(p,c,&ports);if(r!=WS2_OK)return r;
            if(ports.input_count>WS2_MAX_PORTS||ports.output_count>WS2_MAX_PORTS)return int(WS2_INVALID);
            x.inputs=ports.input_count;x.outputs=ports.output_count;
            if constexpr(std::is_same_v<decltype(x.dsp.prepare(Setup{*c,*h})),bool>){if(!x.dsp.prepare(Setup{*c,*h}))return int(WS2_INVALID);}
            else x.dsp.prepare(Setup{*c,*h});
            x.dsp.reset();return int(WS2_OK);});
    }
    static void WS2_CALL reset(void*p){if(p)guarded([&](){static_cast<Instance*>(p)->dsp.reset();return WS2_OK;});}
    static int WS2_CALL process(void*p,ws2_process*c) {
        if(!p||!c||c->struct_size<sizeof(ws2_process)){silence_outputs(c);return WS2_INVALID;}
        auto&x=*static_cast<Instance*>(p);
        if(!c->frames)return WS2_OK;
        auto silence=[&](){silence_outputs(c,x.config.max_channels);};
        auto invalid=[&](){silence();return int(WS2_INVALID);};
        if(c->frames>x.config.max_block_frames||!c->channels||c->channels>x.config.max_channels||
           c->input_count!=x.inputs||c->output_count!=x.outputs||c->param_count!=T::params.size()||
           c->sample_rate!=x.config.sample_rate||
           (c->input_count&&!c->inputs)||(c->output_count&&!c->outputs)||(c->param_count&&!c->params))return invalid();
        for(uint32_t j=0;j<c->output_count;++j) {
            const auto&o=c->outputs[j];
            if(o.struct_size<sizeof(ws2_signal_out)||o.max_channels!=x.config.max_channels||!o.data)return invalid();
            for(uint32_t k=0;k<o.max_channels;++k)if(!o.data[k])return invalid();
        }
        auto valid=[&](const ws2_signal&s){
            if(s.struct_size<sizeof(ws2_signal)||s.channels>x.config.max_channels||(s.channels&&!s.data))return false;
            for(uint32_t k=0;k<s.channels;++k)if(!s.data[k])return false;
            return true;
        };
        for(uint32_t i=0;i<c->input_count;++i)if(!valid(c->inputs[i])){return invalid();}
        for(uint32_t i=0;i<c->param_count;++i)if(!valid(c->params[i])){return invalid();}
        if(c->events.count&&(!c->events.items||c->events.struct_size<sizeof(ws2_event_list)))return invalid();
        uint32_t previous=0;
        for(uint32_t i=0;i<c->events.count;++i){const auto&e=c->events.items[i];if(!valid_event(e)||e.offset>=c->frames||(i&&e.offset<previous))return invalid();previous=e.offset;}
        int result=guarded([&](){Block block(*c);x.dsp.process(block);return WS2_OK;});
        if(result!=WS2_OK){silence();return result;}
        for(uint32_t j=0;j<c->output_count;++j){const auto&o=c->outputs[j];
            if(o.channels>o.max_channels){return invalid();}
            for(uint32_t k=0;k<o.channels;++k)if(!finite_samples(o.data[k],(o.flags&WS2_CONSTANT)?1u:c->frames)){return invalid();}
        }
        return WS2_OK;
    }
    static int WS2_CALL save(void*p,const ws2_writer*w) {
        if(!p||!w||w->struct_size<sizeof(ws2_writer))return WS2_INVALID;
        if constexpr(HasState<T>::value)return guarded([&](){return static_cast<Instance*>(p)->dsp.save_state(Writer(w));});
        else return WS2_UNSUPPORTED;
    }
    static int WS2_CALL load(void*p,const ws2_reader*r) {
        if(!p||!r||r->struct_size<sizeof(ws2_reader))return WS2_INVALID;
        if constexpr(HasState<T>::value)return guarded([&](){return static_cast<Instance*>(p)->dsp.load_state(Reader(r));});
        else return WS2_UNSUPPORTED;
    }
    static uint32_t WS2_CALL latency(void*p) {uint32_t frames=0;if constexpr(HasLatency<T>::value){if(p)guarded([&](){frames=static_cast<Instance*>(p)->dsp.latency_frames();return int(WS2_OK);});}return frames;}
public:
    static const ws2_descriptor*descriptor() {
        static const ws2_descriptor d={sizeof(ws2_descriptor),WS_ABI2_VERSION,T::info.id,T::info.name,T::info.version,
            static_cast<uint32_t>(T::inputs.size()),static_cast<uint32_t>(T::outputs.size()),static_cast<uint32_t>(T::params.size()),
            T::inputs.data(),T::outputs.data(),T::params.data(),create,destroy,query,memory,prepare,reset,process,save,load,latency};
        return &d;
    }
};
}
#define WS_REGISTER_MODULE(T) \
extern "C" WS2_EXPORT uint32_t WS2_CALL WS_MODULE2_VERSION_NAME(void){return WS_ABI2_VERSION;} \
extern "C" WS2_EXPORT const ws2_descriptor*WS2_CALL WS_MODULE2_ENTRY_NAME(void){return ws::Module<T>::descriptor();}
~~~

<!-- BUNDLE_FILE: sdk/signals.h -->
~~~cpp
// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace ws {
// Independently written sample-counted ramp; doubles retain the ABI1
// accumulator precision. Its public samples are still float32 signals.
class LinearRamp {
    double present_, destination_, increment_=0;
    uint32_t length_=240, left_=0;
    bool started_=false;
public:
    explicit LinearRamp(double value=0):present_(value),destination_(value){}
    void prepare(double rate) noexcept { length_=std::max<uint32_t>(1u,static_cast<uint32_t>(rate*.005)); reset(); }
    void reset() noexcept { present_=destination_; left_=0; started_=false; }
    void jump(double value) noexcept { present_=destination_=value; left_=0; }
    void set(double value) noexcept {
        if(value==destination_) return;
        destination_=value;
        if(!started_) {present_=value;left_=0;return;}
        left_=length_; increment_=(destination_-present_)/length_;
    }
    double next() noexcept {
        started_=true;
        if(left_) { --left_; if(left_) present_+=increment_; else present_=destination_; }
        return present_;
    }
    bool constant() const noexcept {return left_==0;}
    double value() const noexcept {return present_;}
};
class Schmitt {
    bool high_=false;
public:
    bool process(float value) noexcept { if(value>.5f)high_=true; else if(value<.25f)high_=false; return high_; }
    void reset() noexcept {high_=false;}
};
class Trigger {
    uint32_t length_=48,left_=0;
public:
    void prepare(double rate) noexcept {length_=std::max<uint32_t>(1u,static_cast<uint32_t>(std::ceil(rate*.001)));left_=0;}
    void fire() noexcept {left_=length_;}
    float next() noexcept {if(!left_)return 0;--left_;return 1;}
    void reset() noexcept {left_=0;}
};
class Random {
    uint64_t state_=0,sequence_=1;
public:
    void seed(uint64_t instance,uint32_t channel=0) noexcept {
        state_=0;sequence_=(static_cast<uint64_t>(channel)<<1)|1;next();state_+=instance;next();
    }
    uint32_t next() noexcept {
        uint64_t old=state_;state_=old*6364136223846793005ULL+sequence_;
        uint32_t x=static_cast<uint32_t>(((old>>18)^old)>>27),r=static_cast<uint32_t>(old>>59);
        return (x>>r)|(x<<((-r)&31));
    }
    float bipolar() noexcept {return static_cast<float>(next()>>8)*(1.f/8388608.f)-1.f;}
    // Additive state accessor so modules can persist the generator. `sequence` is always odd
    // for a seeded generator; restore() trusts its input, so validate before calling it.
    struct State {uint64_t state, sequence;};
    State save() const noexcept {return {state_,sequence_};}
    void restore(const State& s) noexcept {state_=s.state;sequence_=s.sequence;}
};
}
~~~

<!-- BUNDLE_FILE: sdk/simd.h -->
~~~cpp
// SPDX-License-Identifier: MIT
#pragma once
#if !defined(WS_SIMD_SCALAR) && (defined(__SSE2__) || defined(_M_X64))
#include <emmintrin.h>
#define WS_SIMD_SSE2 1
#elif !defined(WS_SIMD_SCALAR) && defined(__ARM_NEON)
#include <arm_neon.h>
#define WS_SIMD_NEON 1
#endif
namespace ws::simd {
struct f4 {
#if defined(WS_SIMD_SSE2)
    __m128 v;
    f4():v(_mm_setzero_ps()){} explicit f4(float x):v(_mm_set1_ps(x)){}
    explicit f4(__m128 x):v(x){}
    static f4 load(const float*p){return f4(_mm_loadu_ps(p));}
    void store(float*p)const{_mm_storeu_ps(p,v);}
    friend f4 operator+(f4 a,f4 b){return f4(_mm_add_ps(a.v,b.v));}
    friend f4 operator-(f4 a,f4 b){return f4(_mm_sub_ps(a.v,b.v));}
    friend f4 operator*(f4 a,f4 b){return f4(_mm_mul_ps(a.v,b.v));}
#elif defined(WS_SIMD_NEON)
    float32x4_t v;
    f4():v(vdupq_n_f32(0)){} explicit f4(float x):v(vdupq_n_f32(x)){}
    explicit f4(float32x4_t x):v(x){}
    static f4 load(const float*p){return f4(vld1q_f32(p));}
    void store(float*p)const{vst1q_f32(p,v);}
    friend f4 operator+(f4 a,f4 b){return f4(vaddq_f32(a.v,b.v));}
    friend f4 operator-(f4 a,f4 b){return f4(vsubq_f32(a.v,b.v));}
    friend f4 operator*(f4 a,f4 b){return f4(vmulq_f32(a.v,b.v));}
#else
    float v[4]{};
    f4()=default; explicit f4(float x){for(auto&y:v)y=x;}
    static f4 load(const float*p){f4 x;for(int i=0;i<4;++i)x.v[i]=p[i];return x;}
    void store(float*p)const{for(int i=0;i<4;++i)p[i]=v[i];}
    friend f4 operator+(f4 a,f4 b){for(int i=0;i<4;++i)a.v[i]+=b.v[i];return a;}
    friend f4 operator-(f4 a,f4 b){for(int i=0;i<4;++i)a.v[i]-=b.v[i];return a;}
    friend f4 operator*(f4 a,f4 b){for(int i=0;i<4;++i)a.v[i]*=b.v[i];return a;}
#endif
};
}
~~~

<!-- BUNDLE_FILE: sdk/fastmath.h -->
~~~cpp
// SPDX-License-Identifier: MIT
#pragma once
namespace ws::fastmath {
// Opt-in only. Range |x|<=0.25 radians, tested absolute error <6e-8.
// Deliberately not used by the VCF port: its range reaches .45*pi.
inline float tan_small(float x) noexcept {
    const double y=x,z=y*y;
    return static_cast<float>(y*(1+z*(1.0/3+z*(2.0/15+z*(17.0/315+z*(62.0/2835))))));
}
}
~~~

<!-- BUNDLE_FILE: module/starter_gain/dsp.h -->
~~~cpp
// SPDX-License-Identifier: MIT
#pragma once
#include "wirescape_sdk.h"
struct StarterGain {
 static constexpr ws::Info info{"example.starter_gain","Starter Gain",1};
 static constexpr std::array inputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array params{ws::param("gain","Gain",0,2,1)};
 float*scratch=nullptr;uint32_t stride=0;
 static ws2_memory memory_requirements(const ws2_config&c){return {sizeof(ws2_memory),2*sizeof(float)*c.max_block_frames,0,32};}
 bool prepare(const ws::Setup&s){stride=s.max_block();scratch=static_cast<float*>(s.alloc(2*sizeof(float)*stride));return scratch!=nullptr;}
 void reset() {}
 void process(ws::Block&b) {
  b.out(0).set_channels(b.channels());
  bool constant=b.in(0).constant()&&b.param(0).constant(); b.out(0).mark_constant(constant);
  const uint32_t count=constant?1:b.frames();
  for(uint32_t c=0;c<b.channels();++c){
   const float*audio=b.in(0).row(c,scratch,count),*gain=b.param(0).row(c,scratch+stride,count);
   for(uint32_t i=0;i<count;++i)b.out(0).ch(c)[i]=audio[i]*gain[i];
  }
 }
};
~~~

<!-- BUNDLE_FILE: module/starter_gain/module.cpp -->
~~~cpp
// SPDX-License-Identifier: MIT
#include "dsp.h"
WS_REGISTER_MODULE(StarterGain)
~~~

<!-- BUNDLE_FILE: module/starter_gain/module.json -->
~~~json
{
  "schema_version": 2,
  "abi_version": 2,
  "id": "example.starter_gain",
  "name": "Starter Gain",
  "library": "ws2_starter_gain",
  "parameters": {
    "gain": 0
  },
  "inputs": {
    "audio": "signal"
  },
  "outputs": {
    "audio": "signal"
  },
  "license": "MIT",
  "description": "Independent ABI 2 gain example with CONSTANT fast path and allocation-aware scratch"
}
~~~

<!-- BUNDLE_FILE: module/starter_gain/tests.cpp -->
~~~cpp
// SPDX-License-Identifier: MIT
// Standalone callback test; compile this file alone (it includes the module).
#include "module.cpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"template check line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
struct Pool {alignas(64) unsigned char bytes[8192];size_t used=0;};
void* WS2_CALL allocate(void*ctx,size_t bytes,size_t alignment,uint32_t){auto&p=*static_cast<Pool*>(ctx);p.used=(p.used+alignment-1)&~(alignment-1);if(p.used+bytes>sizeof(p.bytes))return nullptr;void*r=p.bytes+p.used;p.used+=bytes;return r;}
int main(){
 Pool pool;ws2_host host{sizeof(host),&pool,allocate};auto*d=ws::Module<StarterGain>::descriptor();void*state=d->create(&host);REQUIRE(state);
 ws2_config cfg{sizeof(cfg),48000,256,16,42,nullptr,0};REQUIRE(d->prepare(state,&cfg,&host)==WS2_OK);
 alignas(32) float data[16][256]{};float*dst[16];for(int c=0;c<16;++c)dst[c]=data[c];
 alignas(32) float input[256]={.25f},gain[256]={1};const float*ip[]={input};const float*pp[]={gain};
 ws2_signal in{sizeof(in),ip,1,WS2_CONSTANT|WS2_CONNECTED},param{sizeof(param),pp,1,WS2_CONSTANT};
 ws2_signal_out out{sizeof(out),dst,1,0,16};
 ws2_process ctx{};ctx.struct_size=sizeof(ctx);ctx.frames=17;ctx.channels=4;ctx.sample_rate=48000;ctx.input_count=1;ctx.output_count=1;ctx.param_count=1;ctx.inputs=&in;ctx.params=&param;ctx.outputs=&out;
 REQUIRE(d->process(state,&ctx)==WS2_OK);REQUIRE(out.channels==4);REQUIRE(out.flags&WS2_CONSTANT);for(int c=0;c<4;++c)REQUIRE(data[c][0]==.25f);
 ws2_memory memory{sizeof(memory),0,0,0};REQUIRE(d->memory_requirements(&cfg,&memory)==WS2_OK);REQUIRE(memory.fast_bytes>=2*sizeof(float)*cfg.max_block_frames);
 host.alloc=[](void*,size_t,size_t,uint32_t)->void*{return nullptr;};REQUIRE(d->prepare(state,&cfg,&host)==WS2_INVALID);
 d->reset(state);d->destroy(state);std::puts("starter gain callbacks passed");
}
~~~

<!-- BUNDLE_FILE: CMakeLists.txt -->
~~~cmake
cmake_minimum_required(VERSION 3.20)
project(WirescapeStarterGain LANGUAGES C CXX)
enable_testing()
add_library(wirescape_sdk2 INTERFACE)
target_include_directories(wirescape_sdk2 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/sdk ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(wirescape_sdk2 INTERFACE cxx_std_17)
add_library(ws2_starter_gain SHARED module/starter_gain/module.cpp)
target_link_libraries(ws2_starter_gain PRIVATE wirescape_sdk2)
add_executable(abi2_starter_gain_tests module/starter_gain/tests.cpp)
target_link_libraries(abi2_starter_gain_tests PRIVATE wirescape_sdk2)
add_test(NAME abi2_starter_gain_tests COMMAND abi2_starter_gain_tests)
~~~

## 6. From a working starter to an integrated module

**Folder to submit (not a ZIP requirement):** dsp.h, module.cpp, module.json, tests.cpp, README.md, DESIGN.md; add narrowly scoped resources only if required by the design. A CMake target and app catalogue registration happen in the integration PR. The starter's self-contained CMakeLists.txt is for **independent compilation**, not a replacement for Wirescape's root build.

**Required README:** purpose; audio/control/event input/output IDs and units; parameter IDs/ranges/defaults; algorithm and reset semantics; output levels and clipping; latency and CPU/memory cost; expected platforms; limitations. **Required DESIGN:** implementation decisions, source/provenance and licence, pinned upstream version, denormals/numerics, parameter and state migration policy, test matrix, any host integration request.

**Host integration example (illustrative only, adapt to existing module targets and dependencies):**
~~~~cmake
# Add in native/engine2/CMakeLists.txt or the correctly scoped CMake directory:
add_library(ws2_starter_gain SHARED ../modules2/starter_gain/module.cpp)
target_link_libraries(ws2_starter_gain PRIVATE wirescape_sdk2)
add_executable(abi2_starter_gain_tests ../modules2/starter_gain/tests.cpp)
target_link_libraries(abi2_starter_gain_tests PRIVATE wirescape_sdk2)
add_test(NAME abi2_starter_gain_tests COMMAND abi2_starter_gain_tests)
~~~~
Also add a header include for the type in native/control/module_catalog.h and a WS_APP_MODULE(StarterGain) line in native/control/module_catalog.inc. Existing app and tests may need explicit library-ownership/link context. Adding a CMake target is **not sufficient** for Studio availability; currently the catalogue is explicit. Treat module.json as saved-patch metadata; never assume generic dynamic ABI2 DLL scanning exists.

**Project checks after integration:**
- python tools/dev.py build --headless
- python tools/dev.py test
- ctest --test-dir build/native -R abi2_starter_gain_tests --output-on-failure (adjust real build directory and target)
- python tools/dev.py check --package (when that task requires packaging)
- Windows x64 and Linux ARM64 CI; for DSP/performance-sensitive modules get exact-head Pi **offline** evidence through the task-specific runner acceptance flow in docs/PI_RUNNER.md.
- Only a human operator explicitly confirms physical audio/MIDI/device tests. A passing fake-backend test is not proof that a USB controller or soundcard works end-to-end.

**Acceptance matrix** for any substantial module: 44.1/48/96 kHz **unless the module explicitly rejects unsupported rates**, frame sizes 1/17/64/256 and preparation max, channels 1/4/16 **or documented lower cap**; mono-broadcast and poly SUM/FIRST correctness; reset/seed repeatability; same signal when blocks are partitioned differently; CONSTANT correctness; finite output at extreme input/parameters; output bounds; event identity and overflow recovery; state round trip and bad-state rejection; no callback allocations/locks/IO; bounded CPU worst-case and memory requirements; actual loader/app patch save/reload/render test after integration. Do not claim a test passed unless run. Recording 48k-only restrictions is better than claiming unsupported 96k works.

## 7. Host change requests and hard boundaries

If a feature needs an API/host capability not in the committed SDK or currently registered host, return a short **HOST_CHANGE_REQUEST.md** or issue section instead of patching global engine/ABI/UI to make it "look finished." State: requested user behavior; minimal new service/API; affected host source; security/permission model; realtime handoff/queues; state migration; CI/fake device tests; platform and licence risks. Examples:
- A live OSC Out module needs a worker-owned, permission-aware UDP socket, rate/backpressure handling, and message encoding. An event-output port does **not** open a socket.
- A spectrum display needs a bounded audio tap, decimated analysis off the audio thread, and QML; a DSP-only FFT module cannot create UI.
- A new plugin format requires the adapter, lifecycle and transport mapping, build toolchain, and host-specific validation.
- Adding dynamic/learnable CC rows requires stable configured identities and an implemented coordinator/learn API, not an undocumented field in module.json.
- Runtime changing an arbitrary number of ports needs descriptor/query_ports semantics and patch migration; the host must support and test them.

**Known SDK gaps at snapshot:** ws::Schmitt lacks high() accessor; no stepped-parameter helper; no direct 64-bit Writer/Reader helpers; Reader cannot report remaining bytes; reset cannot emit events, so the host handles note choke; some rich UI controls and host service APIs remain unfinished. ABI2 is under an additive-change soft freeze, **not** a formal stable release.

## 8. A task prompt that works with this one file

Give another developer/agent this entire document and the following assignment, filling only the bracketed fields:

> Build one independent Wirescape **ABI 2** C++17 module: **[ID and name]**. Behavior: **[precise DSP or event behavior]**. Inputs/outputs (IDs, signal versus event, units, poly policies): **[...]**. Parameters (IDs, bounds, defaults, flags): **[...]**. Reset/overflow/state/latency rules: **[...]**. CPU, memory, channels, rate and platform budgets: **[...]**. Use this document's included SDK source, existing helper APIs and starter build to compile and test offline. Return the complete portable module source folder (dsp.h/module.cpp/module.json/tests.cpp/README.md/DESIGN.md, plus required original/licensed resources) **without altering core SDK, host or other modules**. Keep stable IDs, add deterministic callback tests including allocation watching and state/partition checks, and state exactly what was tested. If host/UI/device/SDK work is necessary, give a separate HOST_CHANGE_REQUEST rather than pretending it already exists. Do not open real audio/MIDI/network devices in tests.

**Source links for future refreshing:** GitHub repository https://github.com/kalharbi11/wirescape ; module guide docs/SDK_AUTHOR_GUIDE.md ; core native/include/wirescape/module2.h ; SDK native/sdk/ ; native/voice/ ; registered catalogue native/control/module_catalog.inc and module_catalog.h ; engine native/engine2/ ; MIDI native/io/midi/ and native/modules2/midi_*/ ; built modules native/modules2/ ; latest issues #162, #163, #174, #177, #178, #179 ; integration PRs #82 and #171 at this snapshot. Recheck main/PR merge state before calling a pending feature "built in Studio."

**License and scope:** This guide's prose is GPL-3.0-only consistent with repository documentation. The embedded SDK/ABI headers and starter code are separately marked MIT, as in their original source. Do not strip notices or infer that GPL-derived third-party DSP became MIT. This guide records known behavior from repository source; it is neither a guarantee that every third-party module will compile unchanged nor physical device certification.
