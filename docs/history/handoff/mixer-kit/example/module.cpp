#include "wirescape/module.h"
#include <new>
namespace {
struct State {
    float gain = 1;
};
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void WS_CALL reset(void *) {}
int WS_CALL prepare(void *, const ws_module_config *) { return 0; }
void WS_CALL event(void *p, const ws_module_event *e) {
    if (e->type == WS_PARAMETER && e->parameter_id == 0)
        static_cast<State *>(p)->gain = e->value;
}
void WS_CALL process(void *p, const float *const *in, float *const *out,
                     const ws_process_context *c) {
    auto gain = static_cast<State *>(p)->gain;
    for (uint32_t i = 0; i < c->frames; ++i)
        out[0][i] = (in[0] ? in[0][i] : 0) * (in[1] ? in[1][i] : 1) * gain;
}
const ws_port_descriptor inputs[] = {{"audio", "Audio", WS_AUDIO},
                                     {"gain_cv", "Gain CV", WS_CONTROL}},
                         outputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_parameter_descriptor parameters[] = {{0, "Gain", 0, 2, 1, ""}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor),
                                         WS_ABI_VERSION,
                                         "essentials.vca",
                                         "VCA",
                                         2,
                                         1,
                                         1,
                                         inputs,
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
