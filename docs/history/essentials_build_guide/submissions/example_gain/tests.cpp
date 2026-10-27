#include "wirescape/engine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string &message) {
    if (!condition) throw std::runtime_error(message);
}
struct EngineDeleter { void operator()(ws_engine *p) const { if (p) ws_engine_destroy(p); } };
using Engine = std::unique_ptr<ws_engine, EngineDeleter>;
struct ModuleState {
    const ws_module_descriptor *descriptor;
    void *state;
    ~ModuleState() { if (state) descriptor->destroy(state); }
};
ModuleState make_state(const ws_module_descriptor *d, double rate) {
    void *p = d->create(rate);
    require(p != nullptr, "module create returned null");
    ws_module_config cfg{sizeof(cfg), 512, rate, 1};
    if (d->prepare(p, &cfg) != 0) {
        d->destroy(p);
        throw std::runtime_error("module prepare failed");
    }
    return {d, p};
}
void process(const ws_module_descriptor *d, void *state, const float *input, float *output,
             uint32_t frames, uint64_t position = 0) {
    const float *inputs[] = {input};
    float *outputs[] = {output};
    ws_process_context ctx{sizeof(ctx), frames, position, 1};
    d->process(state, inputs, outputs, &ctx);
}
void test_guards(const ws_module_descriptor *d) {
    auto state = make_state(d, 1000.0);
    float input[] = {0.25f, 0.5f};
    float guarded[] = {-77.0f, -9.0f, -78.0f};
    float *output = guarded + 1;
    ws_process_context ctx{sizeof(ctx), 0, 0, 1};
    const float *inputs[] = {input}; float *outputs[] = {output};
    d->process(state.state, inputs, outputs, &ctx);
    require(output[0] == -9.0f && guarded[0] == -77.0f && guarded[2] == -78.0f, "zero-frame call must not write output");
    ctx.frames = 1; ctx.struct_size = 0;
    d->process(state.state, inputs, outputs, &ctx);
    require(output[0] == -9.0f, "undersized process context must not write output");
    ctx.struct_size = sizeof(ctx);
    float *null_output[] = {nullptr};
    d->process(state.state, inputs, null_output, &ctx);
    d->process(state.state, inputs, nullptr, &ctx);
    d->process(nullptr, inputs, outputs, &ctx);
    d->process(state.state, nullptr, outputs, &ctx);
    require(output[0] == 0.0f, "missing input array must produce silence");
    output[0] = -9.0f;
    const float *null_input[] = {nullptr};
    d->process(state.state, null_input, outputs, &ctx);
    require(output[0] == 0.0f, "null input channel must produce silence");
    output[0] = -9.0f;
    ws_process_context bad{0, 1, 0, 1};
    d->process(state.state, inputs, outputs, &bad);
    require(output[0] == -9.0f, "invalid context header must not write output");
    ws_module_config invalid{0, 0, 0.0, 0};
    require(d->prepare(state.state, &invalid) != 0, "invalid prepare config must be rejected");
    ws_module_event bad_event{WS_PARAMETER, 999, 2.0f, 0, 0};
    d->event(state.state, &bad_event);
    bad_event = {WS_PARAMETER, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0};
    d->event(state.state, &bad_event);
    process(d, state.state, input, output, 1);
    require(std::abs(output[0] - 0.25f) < 1e-6f, "invalid events must not change gain");
    require(guarded[0] == -77.0f && guarded[2] == -78.0f, "one-frame processing overwrote an adjacent guard");
}

void test_partition_invariance(const ws_module_descriptor *d) {
    constexpr double rate = 1000.0;
    constexpr uint32_t count = 37;
    auto whole = make_state(d, rate);
    auto split = make_state(d, rate);
    std::vector<float> input(count), a(count), b(count);
    for (uint32_t i = 0; i < count; ++i) input[i] = 0.1f + 0.01f * i;
    process(d, whole.state, input.data(), a.data(), 1, 0);
    process(d, split.state, input.data(), b.data(), 1, 0);
    const ws_module_event event{WS_PARAMETER, 0, 2.0f, 0, 0};
    d->event(whole.state, &event); d->event(split.state, &event);
    process(d, whole.state, input.data() + 1, a.data() + 1, count - 1, 1);
    const uint32_t chunks[] = {1, 3, 2, 7, 1, 5, 4, 2, 6, 5};
    uint32_t offset = 1, part = 0;
    while (offset < count) {
        const uint32_t n = std::min(chunks[part++ % std::size(chunks)], count - offset);
        process(d, split.state, input.data() + offset, b.data() + offset, n, offset);
        offset += n;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const double gain = i == 0 ? 1.0 : 1.0 + std::min<uint32_t>(i, 5) / 5.0;
        const float expected = static_cast<float>(input[i] * gain);
        require(std::abs(a[i] - b[i]) < 1e-7f, "whole and uneven partitions differ");
        require(std::abs(a[i] - expected) < 1e-6f, "render differs from independent gain-ramp expectation");
    }
}

void test_ramp_duration(const ws_module_descriptor *d, double rate) {
    auto state = make_state(d, rate);
    const uint32_t duration = static_cast<uint32_t>(rate * 0.005);
    require(duration >= 1, "test rate must produce at least one ramp frame");
    float first_in = 0.5f, first_out = 0.0f;
    process(d, state.state, &first_in, &first_out, 1);
    require(std::abs(first_out - 0.5f) < 1e-6f, "initial gain should apply immediately");
    const ws_module_event event{WS_PARAMETER, 0, 2.0f, 0, 0}; d->event(state.state, &event);
    std::vector<float> in(duration + 1, 0.5f), out(duration + 1);
    process(d, state.state, in.data(), out.data(), duration + 1, 1);
    for (uint32_t i = 0; i < duration; ++i) {
        const double gain = 1.0 + static_cast<double>(i + 1) / duration;
        require(std::abs(out[i] - static_cast<float>(0.5 * gain)) < 1e-5f,
                "ramp sample does not match independently calculated duration");
    }
    require(std::abs(out[duration] - 1.0f) < 1e-6f, "ramp must hold target after its duration");
    d->reset(state.state);
    float reset_output = 0.0f;
    process(d, state.state, &first_in, &reset_output, 1);
    require(std::abs(reset_output - 1.0f) < 1e-6f, "reset must snap to the latest target and clear ramp progress");
}
}

int main(int argc, char **argv) {
    try {
        require(argc == 2, "expected built module library path as the only argument");
        ws_engine_config cfg{sizeof(cfg), 48000.0, 256, 8, 64};
        Engine engine(ws_engine_create(&cfg));
        require(engine != nullptr, "engine creation failed");
        require(ws_engine_load_module(engine.get(), argv[1]) == WS_OK, "engine could not dynamically load module");
        require(ws_engine_module_count(engine.get()) == 1, "unexpected module count after load");
        const auto *d = ws_engine_module(engine.get(), 0);
        require(d != nullptr, "loaded module descriptor is null");
        require(d->struct_size >= sizeof(ws_module_descriptor) && d->abi_version == WS_ABI_VERSION,
                "module ABI header mismatch");
        require(d->id && std::strcmp(d->id, "example.gain") == 0, "module ID mismatch");
        require(d->input_count == 1 && d->output_count == 1 && d->parameter_count == 1,
                "descriptor port or parameter count mismatch");
        require(d->inputs && d->outputs && d->parameters && d->inputs[0].kind == WS_AUDIO &&
                d->outputs[0].kind == WS_AUDIO, "descriptor must declare mono audio ports");
        require(d->parameters[0].minimum == 0.0f && d->parameters[0].maximum == 2.0f &&
                d->parameters[0].initial == 1.0f, "gain range descriptor mismatch");
        test_guards(d);
        test_partition_invariance(d);
        test_ramp_duration(d, 1000.0);
        test_ramp_duration(d, 2000.0);
        test_ramp_duration(d, 44100.0);
        test_ramp_duration(d, 96000.0);
        test_ramp_duration(d, 48000.0);
        std::puts("example_gain dynamic load, guards, partitions, reset, and ramp rates passed");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "example_gain test failure: %s\n", e.what());
        return 1;
    }
}
