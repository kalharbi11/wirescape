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
struct Pole {
    double z=0.0;
    double process(double x, double a) noexcept {
        const double v=(x-z)*a;
        const double y=z+v;
        z=y+v;
        if (std::abs(z)<1.0e-20) z=0.0;  // explicit denormal tail cut
        return y;
    }
};
// Inputs beyond ~120 dBFS are numerical-fault territory; finite floats within normal audio pass unchanged.
double safe_input(double x) noexcept { return std::max(-1.0e6, std::min(1.0e6,x)); }
double coefficient(double hz, double rate) noexcept {
    const double cutoff=std::min(hz,0.45*rate);
    const double g=std::tan(3.14159265358979323846*cutoff/rate);
    return g/(1.0+g);
}
struct State {
    double sample_rate=48000.0;
    wirescape::LinearRamp cutoff{1000.0}, resonance{0.0}, drive{0.0};
    Pole poles[4]{};
    double previous=0.0;
    uint32_t max_frames=0;
};
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void clear_filter(State &s) noexcept { for (auto &p:s.poles) p.z=0.0; s.previous=0.0; }
int WS_CALL prepare(void *p, const ws_module_config *c) {
    if (!p || !valid_config(c)) return -1;
    auto &s=*static_cast<State *>(p); s.sample_rate=c->sample_rate;
    s.max_frames=c->max_block_frames; s.cutoff.prepare(c->sample_rate);
    s.resonance.prepare(c->sample_rate); s.drive.prepare(c->sample_rate);
    clear_filter(s); return 0;
}
void WS_CALL reset(void *p) {
    if (!p) return; auto &s=*static_cast<State *>(p);
    clear_filter(s); s.cutoff.reset(); s.resonance.reset(); s.drive.reset();
}
void WS_CALL event(void *p, const ws_module_event *e) {
    if (!p || !e || e->type != WS_PARAMETER || !std::isfinite(e->value)) return;
    auto &s=*static_cast<State *>(p);
    if (e->parameter_id==0 && e->value>=20.0f && e->value<=20000.0f) s.cutoff.set(e->value);
    if (e->parameter_id==1 && e->value>=0.0f && e->value<=1.0f) s.resonance.set(e->value);
    if (e->parameter_id==2 && e->value>=0.0f && e->value<=2.0f) s.drive.set(e->value);
}
void WS_CALL process(void *p, const float *const *in, float *const *out, const ws_process_context *c) {
    auto *s=static_cast<State *>(p);
    if (!valid_process(p, out, c, 1, s ? s->max_frames : 0)) return;
    const float *audio=in ? in[0] : nullptr;
    for (uint32_t i=0; i<c->frames; ++i) {
        const double x=safe_input(audio && std::isfinite(audio[i]) ? audio[i] : 0.0);
        const double a=coefficient(s->cutoff.next(),s->sample_rate);
        const double k=0.85*s->resonance.next(); // bounded small-gain feedback <1
        const double d=s->drive.next();
        const double feedback=(1.0+k)*x-k*s->previous;
        // Drive=0 is strictly linear; nonzero Drive applies a bounded waveshaper
        // with unit small-signal derivative, continuous limit at Drive=0.
        const double g=3.0*d;
        double stage=d == 0.0 ? feedback : std::tanh(g*feedback)/g;
        for (auto &pole:s->poles) stage=pole.process(stage,a);
        s->previous=stage;
        out[0][i]=static_cast<float>(stage);
    }
}
const ws_port_descriptor inputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_port_descriptor outputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_parameter_descriptor parameters[] = {{0, "Cutoff", 20.0f, 20000.0f, 1000.0f, "Hz"}, {1, "Resonance", 0.0f, 1.0f, 0.0f, ""}, {2, "Drive", 0.0f, 2.0f, 0.0f, ""}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "essentials.ladder", "Ladder Filter", 1, 1, 3,
    inputs, outputs, parameters, create, destroy, prepare, reset, event, process};
}
#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() { return &descriptor; }
