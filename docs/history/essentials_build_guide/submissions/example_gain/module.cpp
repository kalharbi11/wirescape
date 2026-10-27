#include "wirescape/module.h"
#include "wirescape/linear_ramp.h"
#include <cmath>
#include <new>

namespace {
struct State { wirescape::LinearRamp gain{1.0}; };
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
int WS_CALL prepare(void *p, const ws_module_config *c) {
    if (!p || !c || c->struct_size < sizeof(ws_module_config) ||
        !std::isfinite(c->sample_rate) || c->sample_rate < 1.0 || c->sample_rate > 384000.0 || c->max_block_frames == 0) return -1;
    static_cast<State *>(p)->gain.prepare(c->sample_rate);
    return 0;
}
void WS_CALL reset(void *p) { if (p) static_cast<State *>(p)->gain.reset(); }
void WS_CALL event(void *p, const ws_module_event *e) {
    if (!p || !e || e->type != WS_PARAMETER || e->parameter_id != 0 ||
        !std::isfinite(e->value) || e->value < 0.0f || e->value > 2.0f) return;
    static_cast<State *>(p)->gain.set(e->value);
}
void WS_CALL process(void *p, const float *const *in, float *const *out, const ws_process_context *c) {
    if (!p || !out || !out[0] || !c || c->struct_size < sizeof(ws_process_context)) return;
    const float *audio = in ? in[0] : nullptr;
    auto &gain = static_cast<State *>(p)->gain;
    for (uint32_t i = 0; i < c->frames; ++i)
        out[0][i] = static_cast<float>((audio ? audio[i] : 0.0f) * gain.next());
}
const ws_port_descriptor inputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_port_descriptor outputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_parameter_descriptor parameters[] = {{0, "Gain", 0.0f, 2.0f, 1.0f, "x"}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "example.gain", "Example Gain", 1, 1, 1, inputs, outputs, parameters,
    create, destroy, prepare, reset, event, process};
}
#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() { return &descriptor; }
