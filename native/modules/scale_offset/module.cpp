// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include "wirescape/linear_ramp.h"
#include <new>

namespace {
struct State {
    wirescape::LinearRamp scale{1.0};
    wirescape::LinearRamp offset{0.0};
};

void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *state) { delete static_cast<State *>(state); }
void WS_CALL reset(void *state) {
    auto &s = *static_cast<State *>(state);
    s.scale.reset();
    s.offset.reset();
}
int WS_CALL prepare(void *state, const ws_module_config *config) {
    auto &s = *static_cast<State *>(state);
    s.scale.prepare(config->sample_rate);
    s.offset.prepare(config->sample_rate);
    return 0;
}
void WS_CALL event(void *state, const ws_module_event *event) {
    if (event->type != WS_PARAMETER) return;
    auto &s = *static_cast<State *>(state);
    if (event->parameter_id == 0) s.scale.set(event->value);
    else if (event->parameter_id == 1) s.offset.set(event->value);
}
void WS_CALL process(void *state, const float *const *inputs, float *const *outputs,
                     const ws_process_context *context) {
    auto &s = *static_cast<State *>(state);
    const float *input = inputs ? inputs[0] : nullptr;
    for (uint32_t i = 0; i < context->frames; ++i)
        outputs[0][i] = static_cast<float>((input ? input[i] : 0.0f) * s.scale.next() + s.offset.next());
}

const ws_port_descriptor inputs[] = {{"control", "Control", WS_CONTROL}};
const ws_port_descriptor outputs[] = {{"control", "Control", WS_CONTROL}};
const ws_parameter_descriptor parameters[] = {{0, "Scale", -2, 2, 1, ""},
                                               {1, "Offset", -1, 1, 0, ""}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor), WS_ABI_VERSION,
    "essentials.scale_offset", "Scale/Offset", 1, 1, 2, inputs, outputs, parameters,
    create, destroy, prepare, reset, event, process};
} // namespace

#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() {
    return &descriptor;
}
