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
// Xorshift32, explicit 32-bit unsigned wrap, seed is nonzero and fixed.
constexpr uint32_t kSeed = 0x12345678u;
struct State { wirescape::LinearRamp level{0.2}; uint32_t rng = kSeed; uint32_t max_frames = 0; };
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
int WS_CALL prepare(void *p, const ws_module_config *c) {
    if (!p || !valid_config(c)) return -1;
    auto &s = *static_cast<State *>(p); s.max_frames = c->max_block_frames;
    s.level.prepare(c->sample_rate); s.rng = kSeed; return 0;
}
void WS_CALL reset(void *p) {
    if (!p) return; auto &s = *static_cast<State *>(p); s.level.reset(); s.rng = kSeed;
}
void WS_CALL event(void *p, const ws_module_event *e) {
    if (!p || !e || e->type != WS_PARAMETER || e->parameter_id != 0 || !std::isfinite(e->value) ||
        e->value < 0.0f || e->value > 1.0f) return;
    static_cast<State *>(p)->level.set(e->value);
}
void WS_CALL process(void *p, const float *const *, float *const *out, const ws_process_context *c) {
    auto *s = static_cast<State *>(p);
    if (!valid_process(p, out, c, 1, s ? s->max_frames : 0)) return;
    for (uint32_t i=0; i<c->frames; ++i) {
        uint32_t x=s->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5; s->rng=x;
        // Use upper 24 bits, [0, 2^24 - 1] -> [-1, 1).
        const double bipolar = (static_cast<double>(x >> 8) * (1.0 / 8388608.0)) - 1.0;
        out[0][i] = static_cast<float>(bipolar * s->level.next());
    }
}
const ws_port_descriptor *inputs = nullptr;
const ws_port_descriptor outputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_parameter_descriptor parameters[] = {{0, "Level", 0.0f, 1.0f, 0.2f, ""}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "essentials.noise", "Noise", 0, 1, 1,
    inputs, outputs, parameters, create, destroy, prepare, reset, event, process};
}
#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() { return &descriptor; }
