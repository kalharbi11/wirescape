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
struct State { double sample_rate = 48000.0, level = 0.0, rise = 0.1, fall = 0.1; uint32_t max_frames = 0; };
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
int WS_CALL prepare(void *p, const ws_module_config *c) {
    if (!p || !valid_config(c)) return -1;
    auto &s = *static_cast<State *>(p); s.sample_rate=c->sample_rate;
    s.max_frames=c->max_block_frames; s.level=0.0; return 0;
}
void WS_CALL reset(void *p) { if (p) static_cast<State *>(p)->level = 0.0; }
void WS_CALL event(void *p, const ws_module_event *e) {
    if (!p || !e || e->type != WS_PARAMETER || !std::isfinite(e->value) ||
        e->value < 0.0f || e->value > 10.0f) return;
    auto &s = *static_cast<State *>(p);
    if (e->parameter_id == 0) s.rise = e->value;
    else if (e->parameter_id == 1) s.fall = e->value;
}
void WS_CALL process(void *p, const float *const *in, float *const *out, const ws_process_context *c) {
    auto *s = static_cast<State *>(p);
    if (!valid_process(p, out, c, 1, s ? s->max_frames : 0)) return;
    const float *target = in ? in[0] : nullptr;
    for (uint32_t i=0; i<c->frames; ++i) {
        const double x = target ? target[i] : 0.0;
        // Valid upstream controls are finite. Ignore a nonfinite sample, holding state.
        if (std::isfinite(x)) {
            const double time = x >= s->level ? s->rise : s->fall;
            if (time == 0.0) s->level = x;
            else {
                const double step = 1.0 / (time * s->sample_rate);
                if (x > s->level) s->level += std::min(step, x - s->level);
                else s->level -= std::min(step, s->level - x);
            }
        }
        out[0][i] = static_cast<float>(s->level);
    }
}
const ws_port_descriptor inputs[] = {{"control", "Control", WS_CONTROL}};
const ws_port_descriptor outputs[] = {{"control", "Control", WS_CONTROL}};
const ws_parameter_descriptor parameters[] = {{0, "Rise", 0.0f, 10.0f, 0.1f, "s/unit"}, {1, "Fall", 0.0f, 10.0f, 0.1f, "s/unit"}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "essentials.slew", "Slew", 1, 1, 2,
    inputs, outputs, parameters, create, destroy, prepare, reset, event, process};
}
#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() { return &descriptor; }
