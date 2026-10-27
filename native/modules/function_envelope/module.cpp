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
    double sample_rate=48000.0, level=0.0, start=0.0, endpoint=0.0;
    double rise=0.01, fall=0.1, curve=0.0, latched_curve=0.0;
    uint64_t length=1, elapsed=1;
    bool high=false;
    uint32_t max_frames=0;
};
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void clear_segment(State &s) noexcept { s.level=s.start=s.endpoint=0.0; s.elapsed=s.length=1; s.high=false; }
int WS_CALL prepare(void *p, const ws_module_config *c) {
    if (!p || !valid_config(c)) return -1;
    auto &s = *static_cast<State *>(p); s.sample_rate=c->sample_rate;
    s.max_frames=c->max_block_frames; clear_segment(s); return 0;
}
void WS_CALL reset(void *p) { if (p) clear_segment(*static_cast<State *>(p)); }
void WS_CALL event(void *p, const ws_module_event *e) {
    if (!p || !e || e->type != WS_PARAMETER || !std::isfinite(e->value)) return;
    auto &s = *static_cast<State *>(p);
    switch (e->parameter_id) {
      case 0: if (e->value >= 0.0001f && e->value <= 30.0f) s.rise=e->value; break;
      case 1: if (e->value >= 0.0001f && e->value <= 30.0f) s.fall=e->value; break;
      case 2: if (e->value >= -1.0f && e->value <= 1.0f) s.curve=e->value; break;
      default: break;
    }
}
void begin_segment(State &s, bool now_high) noexcept {
    s.high=now_high; s.start=s.level; s.endpoint=now_high ? 1.0 : 0.0;
    s.latched_curve=s.curve;
    const double seconds=now_high ? s.rise : s.fall;
    s.length=std::max<uint64_t>(1, static_cast<uint64_t>(std::llround(seconds*s.sample_rate)));
    s.elapsed=0;
}
void WS_CALL process(void *p, const float *const *in, float *const *out, const ws_process_context *c) {
    auto *s=static_cast<State *>(p);
    if (!valid_process(p, out, c, 1, s ? s->max_frames : 0)) return;
    const float *gate=in ? in[0] : nullptr;
    for (uint32_t i=0; i<c->frames; ++i) {
        const bool high=gate && std::isfinite(gate[i]) && gate[i]>=0.5f;
        if (high != s->high) begin_segment(*s,high);
        if (s->elapsed < s->length) {
            ++s->elapsed;
            if (s->elapsed == s->length) s->level=s->endpoint;
            else {
                const double u=static_cast<double>(s->elapsed)/static_cast<double>(s->length);
                const double k=4.0*s->latched_curve;
                const double shaped=k == 0.0 ? u : std::expm1(k*u)/std::expm1(k);
                s->level=s->start+(s->endpoint-s->start)*shaped;
            }
        }
        out[0][i]=static_cast<float>(s->level);
    }
}
const ws_port_descriptor inputs[] = {{"gate", "Gate", WS_CONTROL}};
const ws_port_descriptor outputs[] = {{"envelope", "Envelope", WS_CONTROL}};
const ws_parameter_descriptor parameters[] = {{0, "Rise", 0.0001f, 30.0f, 0.01f, "s"}, {1, "Fall", 0.0001f, 30.0f, 0.1f, "s"}, {2, "Curve", -1.0f, 1.0f, 0.0f, ""}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "essentials.function_envelope", "Function Envelope", 1, 1, 3,
    inputs, outputs, parameters, create, destroy, prepare, reset, event, process};
}
#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() { return &descriptor; }
