// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include "wirescape/linear_ramp.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <new>
#include <limits>

namespace {
constexpr double kMaxSampleRate = 384000.0;
bool valid_config(const ws_module_config *c) noexcept {
    return c && c->struct_size >= sizeof(ws_module_config) && std::isfinite(c->sample_rate) &&
           c->sample_rate >= 1.0 && c->sample_rate <= kMaxSampleRate && c->max_block_frames > 0;
}
bool valid_process(const void *p, float *const *out, const ws_process_context *c, uint32_t count, uint32_t max_frames) noexcept {
    if (!p || !out || !c || c->struct_size < sizeof(ws_process_context) || c->frames > max_frames) return false;
    for (uint32_t i = 0; i < count; ++i) if (!out[i]) return false;
    return true;
}
struct State {
    double sample_rate=48000.0, phase=0.0, ticks=1.0, duty=0.5, run=1.0;
    wirescape::LinearRamp bpm{120.0};
    bool reset_high=false;
    uint32_t max_frames=0;
};
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void clear_clock(State &s) noexcept { s.phase=0.0; s.reset_high=false; }
int WS_CALL prepare(void *p, const ws_module_config *c) {
    if (!p || !valid_config(c)) return -1;
    auto &s = *static_cast<State *>(p); s.sample_rate=c->sample_rate;
    s.max_frames=c->max_block_frames; s.bpm.prepare(c->sample_rate);
    clear_clock(s); return 0;
}
void WS_CALL reset(void *p) {
    if (!p) return; auto &s=*static_cast<State *>(p);
    clear_clock(s); s.bpm.reset();
}
void WS_CALL event(void *p, const ws_module_event *e) {
    if (!p || !e || e->type != WS_PARAMETER || !std::isfinite(e->value)) return;
    auto &s = *static_cast<State *>(p);
    switch (e->parameter_id) {
      case 0: if (e->value >= 20.0f && e->value <= 300.0f) s.bpm.set(e->value); break;
      case 1: if (e->value >= 1.0f && e->value <= 24.0f) s.ticks=std::floor(static_cast<double>(e->value)+0.5); break;
      case 2: if (e->value >= 0.01f && e->value <= 0.99f) s.duty=e->value; break;
      case 3: if (e->value >= 0.0f && e->value <= 1.0f) s.run=e->value; break;
      default: break;
    }
}
void WS_CALL process(void *p, const float *const *in, float *const *out, const ws_process_context *c) {
    auto *s=static_cast<State *>(p);
    if (!valid_process(p, out, c, 1, s ? s->max_frames : 0)) return;
    const float *reset=in ? in[0] : nullptr;
    for (uint32_t i=0; i<c->frames; ++i) {
        const bool now_high=reset && std::isfinite(reset[i]) && reset[i]>=0.5f;
        if (now_high && !s->reset_high) s->phase=0.0;
        s->reset_high=now_high;
        const double bpm=s->bpm.next();
        if (s->run >= 0.5) {
            out[0][i]=s->phase + 1.0e-12 < s->duty ? 1.0f : 0.0f;
            s->phase += bpm*s->ticks/(60.0*s->sample_rate);
            if (s->phase >= 1.0) s->phase -= std::floor(s->phase);
        } else out[0][i]=0.0f;
    }
}
const ws_port_descriptor inputs[] = {{"reset", "Reset", WS_CONTROL}};
const ws_port_descriptor outputs[] = {{"clock", "Clock", WS_CONTROL}};
const ws_parameter_descriptor parameters[] = {{0, "BPM", 20.0f, 300.0f, 120.0f, "BPM"}, {1, "Ticks per beat", 1.0f, 24.0f, 1.0f, ""}, {2, "Duty", 0.01f, 0.99f, 0.5f, ""}, {3, "Run", 0.0f, 1.0f, 1.0f, ""}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "essentials.clock", "Clock", 1, 1, 4,
    inputs, outputs, parameters, create, destroy, prepare, reset, event, process};
}
#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() { return &descriptor; }
