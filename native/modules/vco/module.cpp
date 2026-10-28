// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include "wirescape/linear_ramp.h"
#include <algorithm>
#include <cmath>
#include <new>
namespace {
constexpr double tau = 6.283185307179586476925286766559;
struct State {
    double rate, phase = 0;
    wirescape::LinearRamp frequency{220};
};
void *WS_CALL create(double rate) { return new (std::nothrow) State{rate}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void WS_CALL reset(void *p) {
    auto &s = *static_cast<State *>(p);
    s.phase = 0; s.frequency.reset();
}
int WS_CALL prepare(void *p, const ws_module_config *c) {
    static_cast<State *>(p)->rate = c->sample_rate;
    static_cast<State *>(p)->frequency.prepare(c->sample_rate);
    reset(p);
    return 0;
}
void WS_CALL event(void *p, const ws_module_event *e) {
    auto &s = *static_cast<State *>(p);
    if (e->type == WS_PARAMETER && e->parameter_id == 0) {
        s.frequency.set(e->value);
    }
    if (e->type == WS_NOTE_ON) {
        s.frequency.jump(static_cast<float>(440 * std::pow(2., (e->value - 69.) / 12.)));
    }
}
void WS_CALL process(void *p, const float *const *, float *const *out,
                     const ws_process_context *c) {
    auto &s = *static_cast<State *>(p);
    for (uint32_t i = 0; i < c->frames; ++i) {
        out[0][i] = static_cast<float>(std::sin(s.phase));
        s.phase += tau * std::min<double>(s.frequency.next(), s.rate * .49) / s.rate;
        if (s.phase >= tau)
            s.phase -= tau;
    }
}
const ws_port_descriptor outputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_parameter_descriptor parameters[] = {{0, "Frequency", .01f, 20000, 220, "Hz"}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor),
                                         WS_ABI_VERSION,
                                         "essentials.vco",
                                         "VCO",
                                         0,
                                         1,
                                         1,
                                         nullptr,
                                         outputs,
                                         parameters,
                                         create,
                                         destroy,
                                         prepare,
                                         reset,
                                         event,
                                         process};
} // namespace
#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() {
    return &descriptor;
}
