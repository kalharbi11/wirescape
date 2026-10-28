// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include <new>
namespace {
struct State {};
void *WS_CALL create(double) { return new (std::nothrow) State{}; }
void WS_CALL destroy(void *p) { delete static_cast<State *>(p); }
void WS_CALL reset(void *) {}
int WS_CALL prepare(void *, const ws_module_config *) { return 0; }
void WS_CALL event(void *, const ws_module_event *) {}
void WS_CALL process(void *, const float *const *in, float *const *out,
                     const ws_process_context *c) {
    for (uint32_t i = 0; i < c->frames; ++i)
        out[0][i] = in[0] ? in[0][i] : 0;
}
const ws_port_descriptor inputs[] = {{"audio", "Audio", WS_AUDIO}},
                         outputs[] = {{"audio", "Audio", WS_AUDIO}};
const ws_module_descriptor descriptor = {sizeof(ws_module_descriptor),
                                         WS_ABI_VERSION,
                                         "essentials.output",
                                         "Output",
                                         1,
                                         1,
                                         0,
                                         inputs,
                                         outputs,
                                         nullptr,
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
