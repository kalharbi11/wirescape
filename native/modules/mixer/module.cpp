// SPDX-License-Identifier: GPL-3.0-only
#include "wirescape/module.h"
#include "wirescape/linear_ramp.h"
#include <new>

namespace {

struct State {
    wirescape::LinearRamp level_a{.5};
    wirescape::LinearRamp level_b{.5};
    double sample_rate = 0.0;
    uint32_t max_block_frames = 0;
};

void *WS_CALL create(double) {
    return new (std::nothrow) State{};
}

void WS_CALL destroy(void *p) {
    delete static_cast<State *>(p);
}

int WS_CALL prepare(void *p, const ws_module_config *config) {
    if (!p || !config || config->struct_size < sizeof(ws_module_config) ||
        config->sample_rate <= 0.0 || config->max_block_frames == 0)
        return -1;

    auto *state = static_cast<State *>(p);
    state->level_a.prepare(config->sample_rate); state->level_b.prepare(config->sample_rate);
    state->sample_rate = config->sample_rate;
    state->max_block_frames = config->max_block_frames;
    return 0;
}

// Reset leaves both user-adjustable levels unchanged.
void WS_CALL reset(void *p) { auto &s = *static_cast<State *>(p); s.level_a.reset(); s.level_b.reset(); }

void WS_CALL event(void *p, const ws_module_event *e) {
    if (!p || !e || e->type != WS_PARAMETER)
        return;

    auto *state = static_cast<State *>(p);
    if (e->parameter_id == 0)
        state->level_a.set(e->value);
    else if (e->parameter_id == 1)
        state->level_b.set(e->value);
}

void WS_CALL process(void *p, const float *const *in, float *const *out,
                     const ws_process_context *c) {
    auto *state = static_cast<State *>(p);
    const float *a = in[0];
    const float *b = in[1];
    float *audio = out[0];

    for (uint32_t i = 0; i < c->frames; ++i) {
        const float sample_a = a ? a[i] : 0.0f;
        const float sample_b = b ? b[i] : 0.0f;
        audio[i] = static_cast<float>(sample_a * state->level_a.next() + sample_b * state->level_b.next());
    }
}

const ws_port_descriptor inputs[] = {
    {"a", "A", WS_AUDIO},
    {"b", "B", WS_AUDIO},
};
const ws_port_descriptor outputs[] = {
    {"audio", "Audio", WS_AUDIO},
};
const ws_parameter_descriptor parameters[] = {
    {0, "Level A", 0.0f, 2.0f, 0.5f, ""},
    {1, "Level B", 0.0f, 2.0f, 0.5f, ""},
};
const ws_module_descriptor descriptor = {
    sizeof(ws_module_descriptor),
    WS_ABI_VERSION,
    "essentials.mixer",
    "Mixer",
    2,
    1,
    2,
    inputs,
    outputs,
    parameters,
    create,
    destroy,
    prepare,
    reset,
    event,
    process,
};

} // namespace

#ifndef WS_MODULE_ENTRY_NAME
#define WS_MODULE_ENTRY_NAME ws_module_entry
#endif
extern "C" WS_EXPORT const ws_module_descriptor *WS_CALL WS_MODULE_ENTRY_NAME() {
    return &descriptor;
}
