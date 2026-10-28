// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include "wirescape/linear_ramp.h"
#include <algorithm>
#include <cmath>
#include <new>
namespace {
struct State {
    double rate;
    float attack = .01f, decay = .1f, release = .2f, level = 0, start = 0;
    wirescape::LinearRamp sustain{.7f};
    uint64_t elapsed = 0, duration = 1;
    int stage = 0;
    uint32_t source = 0, note = 0;
    bool gated = false;
};
void *WS_CALL create(double rate) { return new (std::nothrow) State{rate}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void WS_CALL reset(void *p) {
    auto &s = *static_cast<State *>(p);
    s.level = 0;
    s.stage = 0;
    s.elapsed = 0;
    s.gated = false;
    s.sustain.reset();
}
int WS_CALL prepare(void *p, const ws_module_config *c) {
    static_cast<State *>(p)->rate = c->sample_rate;
    static_cast<State *>(p)->sustain.prepare(c->sample_rate);
    reset(p);
    return 0;
}
void stage(State &s, int value, float seconds) {
    s.stage = value;
    s.start = s.level;
    s.elapsed = 0;
    s.duration = std::max<uint64_t>(1, static_cast<uint64_t>(std::llround(seconds * s.rate)));
}
void WS_CALL event(void *p, const ws_module_event *e) {
    auto &s = *static_cast<State *>(p);
    if (e->type == WS_PARAMETER) {
        if (e->parameter_id == 0)
            s.attack = e->value;
        if (e->parameter_id == 1)
            s.decay = e->value;
        if (e->parameter_id == 2)
            s.sustain.set(e->value);
        if (e->parameter_id == 3)
            s.release = e->value;
    }
    bool owned = e->source_id == s.source && e->note_id == s.note;
    if (e->type == WS_NOTE_ON || (e->type == WS_GATE && e->value > 0 && (!s.gated || !owned))) {
        s.source = e->source_id;
        s.note = e->note_id;
        s.gated = true;
        stage(s, 1, s.attack);
    }
    if ((e->type == WS_NOTE_OFF || (e->type == WS_GATE && e->value <= 0)) && owned && s.gated) {
        s.gated = false;
        stage(s, 4, s.release);
    }
}
void WS_CALL process(void *p, const float *const *, float *const *out,
                     const ws_process_context *c) {
    auto &s = *static_cast<State *>(p);
    for (uint32_t i = 0; i < c->frames; ++i) {
        const float sustain = static_cast<float>(s.sustain.next());
        if (s.stage == 1) {
            s.level = s.start + (1 - s.start) * static_cast<float>(++s.elapsed) /
                                    static_cast<float>(s.duration);
            if (s.elapsed >= s.duration) {
                s.level = 1;
                stage(s, 2, s.decay);
            }
        } else if (s.stage == 2) {
            s.level = s.start + (sustain - s.start) * static_cast<float>(++s.elapsed) /
                                    static_cast<float>(s.duration);
            if (s.elapsed >= s.duration) {
                s.level = sustain;
                s.stage = 3;
            }
        } else if (s.stage == 3)
            s.level = sustain;
        else if (s.stage == 4) {
            s.level =
                s.start * (1 - static_cast<float>(++s.elapsed) / static_cast<float>(s.duration));
            if (s.elapsed >= s.duration) {
                s.level = 0;
                s.stage = 0;
            }
        }
        out[0][i] = s.level;
    }
}
const ws_port_descriptor outputs[] = {{"envelope", "Envelope", WS_CONTROL}};
const ws_parameter_descriptor parameters[] = {{0, "Attack", .0001f, 30, .01f, "s"},
                                              {1, "Decay", .0001f, 30, .1f, "s"},
                                              {2, "Sustain", 0, 1, .7f, ""},
                                              {3, "Release", .0001f, 30, .2f, "s"}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor),
                                         WS_ABI_VERSION,
                                         "essentials.adsr",
                                         "ADSR",
                                         0,
                                         1,
                                         4,
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
