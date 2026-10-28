// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include "wirescape/linear_ramp.h"
#include <cmath>
#include <new>

namespace {
constexpr double tau = 6.283185307179586476925286766559;
struct State {
    double sample_rate = 48000.0;
    double phase = 0.0;
    wirescape::LinearRamp rate{1.0};
    wirescape::LinearRamp depth{1.0};
    wirescape::LinearRamp offset{0.0};
};
void *WS_CALL create(double sample_rate) {
    auto *s = new (std::nothrow) State{};
    if (s) s->sample_rate = sample_rate;
    return s;
}
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void WS_CALL reset(void *p) {
    auto &s = *static_cast<State *>(p);
    s.phase = 0.0;
    s.rate.reset(); s.depth.reset(); s.offset.reset();
}
int WS_CALL prepare(void *p, const ws_module_config *c) {
    auto &s = *static_cast<State *>(p);
    s.sample_rate = c->sample_rate;
    s.rate.prepare(c->sample_rate);
    s.depth.prepare(c->sample_rate);
    s.offset.prepare(c->sample_rate);
    reset(p);
    return 0;
}
void WS_CALL event(void *p, const ws_module_event *e) {
    if (e->type != WS_PARAMETER) return;
    auto &s = *static_cast<State *>(p);
    if (e->parameter_id == 0) s.rate.set(e->value);
    else if (e->parameter_id == 1) s.depth.set(e->value);
    else if (e->parameter_id == 2) s.offset.set(e->value);
}
void WS_CALL process(void *p, const float *const *, float *const *out,
                     const ws_process_context *c) {
    auto &s = *static_cast<State *>(p);
    for (uint32_t i = 0; i < c->frames; ++i) {
        const double depth = s.depth.next();
        const double offset = s.offset.next();
        out[0][i] = static_cast<float>(std::sin(s.phase) * depth + offset);
        s.phase += tau * s.rate.next() / s.sample_rate;
        if (s.phase >= tau) s.phase = std::fmod(s.phase, tau);
    }
}
const ws_port_descriptor outputs[] = {{"control", "Control", WS_CONTROL}};
const ws_parameter_descriptor parameters[] = {{0, "Rate", .01f, 20, 1, "Hz"},
                                               {1, "Depth", 0, 1, 1, ""},
                                               {2, "Offset", -1, 1, 0, ""}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "essentials.lfo", "LFO", 0, 1, 3, nullptr, outputs, parameters,
    create, destroy, prepare, reset, event, process};
} // namespace

#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() {
    return &descriptor;
}
