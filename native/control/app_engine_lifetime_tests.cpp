// SPDX-License-Identifier: GPL-3.0-only
#include "app_engine.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
static int live;
static void *WS_CALL create(double) {
    ++live;
    return new int(1);
}
static void WS_CALL destroy(void *p) {
    --live;
    delete static_cast<int *>(p);
}
static int WS_CALL prepare(void *, const ws_module_config *) { return WS_OK; }
static void WS_CALL reset(void *) {}
static void WS_CALL event(void *, const ws_module_event *) {}
static void WS_CALL process(void *, const float *const *, float *const *out, const ws_process_context *c) {
    std::fill_n(out[0], c->frames, .25f);
}
int main() {
    static const ws_port_descriptor outputs[] = {{"audio", "Audio", WS_AUDIO}};
    static const ws_module_descriptor descriptor{sizeof(descriptor),
                                                 1,
                                                 "test.lifetime",
                                                 "Lifetime",
                                                 0,
                                                 1,
                                                 0,
                                                 nullptr,
                                                 outputs,
                                                 nullptr,
                                                 create,
                                                 destroy,
                                                 prepare,
                                                 reset,
                                                 event,
                                                 process};
    ws_engine_config config{sizeof(config), 48000, 128, 4, 2048};
    auto *engine = app_engine_create(&config);
    if (!engine || app_engine_register_module(engine, &descriptor) || app_engine_add_node(engine, 1, descriptor.id) ||
        app_engine_prepare(engine, 1, 0) || live != 1)
        std::abort();
    float data[128];
    if (app_engine_render(engine, data, 128) != WS_OK || data[0] != .25f) std::abort();
    app_engine_destroy(engine);
    if (live) std::abort();
    std::puts("PASS shim destruction retains borrowed metadata and library owners");
}
