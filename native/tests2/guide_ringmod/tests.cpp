// SPDX-License-Identifier: MIT
// Standalone C-callback test: compile this file alone; it includes module.cpp.
#include "module.cpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr, "ringmod check line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)

struct Pool { alignas(64) unsigned char bytes[8192]; size_t used = 0; };
static void* WS2_CALL allocate(void* ctx, size_t bytes, size_t alignment, uint32_t) {
    auto& pool = *static_cast<Pool*>(ctx);
    pool.used = (pool.used + alignment - 1) & ~(alignment - 1);
    if (pool.used + bytes > sizeof(pool.bytes)) return nullptr;
    void* result = pool.bytes + pool.used;
    pool.used += bytes;
    return result;
}

static void run_case(int channels, int frames, double sample_rate, bool constant, bool mixed = false) {
    Pool pool{};
    ws2_host host{sizeof(host), &pool, allocate};
    const auto* descriptor = ws::Module<RingMod>::descriptor();
    REQUIRE(descriptor);
    REQUIRE(descriptor->input_count == 2 && descriptor->output_count == 1 && descriptor->param_count == 1);
    void* state = descriptor->create(&host);
    REQUIRE(state);
    ws2_config config{sizeof(config), sample_rate, 256, 16, 42, nullptr, 0};
    REQUIRE(descriptor->prepare(state, &config, &host) == WS2_OK);

    alignas(32) float a[256]{}, b[16][256]{}, level[256]{}, output[16][256]{};
    const float* a_ptrs[]{a};
    const float* b_ptrs[16]{};
    const float* level_ptrs[]{level};
    float* output_ptrs[16]{};
    for (int c = 0; c < 16; ++c) { b_ptrs[c] = b[c]; output_ptrs[c] = output[c]; }
    for (int i = 0; i < frames; ++i) {
        a[i] = mixed && i > 0 ? 123.0f :
               (constant || mixed) ? .5f : static_cast<float>((i % 11) - 5) / 10.0f;
        level[i] = mixed && i > 0 ? 321.0f :
                   (constant || mixed) ? .75f : static_cast<float>((i % 7) + 1) / 8.0f;
        for (int c = 0; c < channels; ++c)
            b[c][i] = constant ? static_cast<float>(c + 1) / 16.0f
                               : static_cast<float>((c + 1) * ((i % 5) - 2)) / 32.0f;
    }
    const uint32_t a_flags = (constant || mixed) ? WS2_CONSTANT | WS2_CONNECTED : WS2_CONNECTED;
    const uint32_t b_flags = constant ? WS2_CONSTANT | WS2_CONNECTED : WS2_CONNECTED;
    ws2_signal inputs[2]{{sizeof(ws2_signal), a_ptrs, 1, a_flags},
                         {sizeof(ws2_signal), b_ptrs, static_cast<uint32_t>(channels), b_flags}};
    ws2_signal parameter{sizeof(parameter), level_ptrs, 1, (constant || mixed) ? WS2_CONSTANT : 0u};
    ws2_signal_out out{sizeof(out), output_ptrs, 1, 0, 16};
    ws2_process process{};
    process.struct_size = sizeof(process);
    process.frames = static_cast<uint32_t>(frames);
    process.channels = static_cast<uint32_t>(channels);
    process.sample_rate = sample_rate;
    process.input_count = 2;
    process.output_count = 1;
    process.param_count = 1;
    process.inputs = inputs;
    process.params = &parameter;
    process.outputs = &out;

    REQUIRE(descriptor->process(state, &process) == WS2_OK);
    REQUIRE(out.channels == static_cast<uint32_t>(channels));
    REQUIRE(bool(out.flags & WS2_CONSTANT) == constant);
    for (int c = 0; c < channels; ++c) {
        const int count = constant ? 1 : frames;
        for (int i = 0; i < count; ++i) {
            const float reference = a[(constant || mixed) ? 0 : i] * b[c][constant ? 0 : i] *
                                    level[(constant || mixed) ? 0 : i];
            REQUIRE(std::fabs(output[c][i] - reference) <= 1e-7f);
        }
    }
    const float first = output[0][0];
    descriptor->reset(state);
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < frames; ++i) output[c][i] = 999.0f;
    out.channels = 1;
    out.flags = 0;
    REQUIRE(descriptor->process(state, &process) == WS2_OK);
    REQUIRE(output[0][0] == first); // Stateless reset is reproducible.
    descriptor->destroy(state);
}

int main() {
    for (double rate : {44100.0, 48000.0, 96000.0})
        for (int channels : {1, 4, 16})
            for (int frames : {1, 17, 256}) {
                run_case(channels, frames, rate, false);
                run_case(channels, frames, rate, true);
                run_case(channels, frames, rate, false, true); // A/Level constant, B varies.
            }
    std::puts("guide ringmod callbacks passed");
}
