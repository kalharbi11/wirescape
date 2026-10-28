// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "../../engine2/engine2.h"
#include <vector>
#include <chrono>
#include <string>

struct Arena {
    std::vector<unsigned char> bytes = std::vector<unsigned char>(4 * 1024 * 1024);
    size_t used = 0;
    static void* WS2_CALL alloc(void* context, size_t bytes, size_t alignment, uint32_t) {
        auto& a = *static_cast<Arena*>(context);
        const uintptr_t base = reinterpret_cast<uintptr_t>(a.bytes.data());
        const uintptr_t next = (base + a.used + alignment - 1) & ~(uintptr_t(alignment) - 1);
        const size_t end = next - base + bytes;
        if (end > a.bytes.size()) return nullptr;
        a.used = end;
        return reinterpret_cast<void*>(next);
    }
};
struct Rig {
    Arena arena;
    ws2_host host{sizeof(host), &arena, Arena::alloc};
    const ws2_descriptor* descriptor = ws::Module<chaos_lite::ChaosLite>::descriptor();
    void* instance = nullptr;
    static constexpr uint32_t max_frames = 256, max_channels = 16, parameters = 26;
    struct Buffers { float out[2][max_channels][max_frames]{}; };
    std::unique_ptr<Buffers> buffers = std::make_unique<Buffers>();
    float input_values[4][max_channels]{};
    const float* input_rows[4][max_channels]{};
    ws2_signal inputs[4]{};
    float values[parameters]{};
    const float* value_rows[parameters]{};
    ws2_signal params[parameters]{};
    float* output_rows[2][max_channels]{};
    ws2_signal_out outputs[2]{};
    ws2_process context{};
    explicit Rig(double rate = 48000, uint32_t channels = 4, uint64_t seed = 42) {
        const ws2_config config{sizeof(config), rate, max_frames, max_channels, seed, nullptr, 0};
        instance = descriptor->create(&host);
        REQUIRE(instance && descriptor->prepare(instance, &config, &host) == WS2_OK);
        for (uint32_t p = 0; p < 4; ++p) {
            for (uint32_t c = 0; c < max_channels; ++c) {
                input_values[p][c] = p == 1 || p == 2 ? 1.f : 0.f;
                input_rows[p][c] = &input_values[p][c];
            }
            inputs[p] = {sizeof(ws2_signal), input_rows[p], channels, WS2_CONSTANT | WS2_CONNECTED};
        }
        for (uint32_t p = 0; p < parameters; ++p) {
            values[p] = chaos_lite::ChaosLite::params[p].initial;
            value_rows[p] = &values[p]; params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
        }
        for (uint32_t p = 0; p < 2; ++p) {
            for (uint32_t c = 0; c < max_channels; ++c) output_rows[p][c] = buffers->out[p][c];
            outputs[p] = {sizeof(ws2_signal_out), output_rows[p], channels, 0, max_channels};
        }
        context.struct_size = sizeof(context); context.sample_rate = rate; context.channels = channels;
        context.input_count = 4; context.output_count = 2; context.param_count = parameters;
        context.inputs = inputs; context.outputs = outputs; context.params = params;
    }
    ~Rig() { descriptor->destroy(instance); }
    void run(uint32_t frames) {
        context.frames = frames;
        int result;
        { allocation_watch::Scope watch; result = descriptor->process(instance, &context); REQUIRE(watch.allocations() == 0); }
        REQUIRE(result == WS2_OK);
        REQUIRE(!(outputs[0].flags & WS2_CONSTANT) && !(outputs[1].flags & WS2_CONSTANT));
        context.frame_position += frames;
        for (uint32_t p = 0; p < 2; ++p) for (uint32_t c = 0; c < context.channels; ++c)
            for (uint32_t i = 0; i < frames; ++i) {
                const float value = buffers->out[p][c][i];
                REQUIRE(std::isfinite(value) && std::abs(value) <= .8912509f);
                if (c >= 8) REQUIRE(value == 0);
            }
    }
    Bytes save() { Bytes bytes; ws2_writer writer{sizeof(writer), &bytes, sink}; REQUIRE(descriptor->save_state(instance, &writer) == WS2_OK); return bytes; }
    int load(const Bytes& bytes) { Source source_context{&bytes, 0}; ws2_reader reader{sizeof(reader), &source_context, source, static_cast<uint32_t>(bytes.size())}; return descriptor->load_state(instance, &reader); }
};
static std::vector<float> render(Rig& r, uint32_t block) {
    std::vector<float> result(2 * r.context.channels * 256);
    for (uint32_t first = 0; first < 256; first += block) {
        const uint32_t n = std::min(block, 256u - first); r.run(n);
        for (uint32_t p = 0; p < 2; ++p) for (uint32_t c = 0; c < r.context.channels; ++c)
            for (uint32_t i = 0; i < n; ++i) result[(p * r.context.channels + c) * 256 + first + i] = r.buffers->out[p][c][i];
    }
    return result;
}
static void matrix() {
    for (double rate : {44100., 48000., 96000.}) for (uint32_t channels : {1u, 4u, 16u}) {
        Rig whole(rate, channels); const auto reference = render(whole, 256);
        REQUIRE(*std::max_element(reference.begin(), reference.end()) > 0);
        for (uint32_t block : {1u, 17u, 64u}) { Rig cut(rate, channels); REQUIRE(render(cut, block) == reference); }
        whole.descriptor->reset(whole.instance); REQUIRE(render(whole, 17) == reference);
        if (channels > 1) REQUIRE(!std::equal(reference.begin(), reference.begin() + 256, reference.begin() + 256));
    }
}
static void state_and_extremes() {
    Rig a, b; a.run(256); const auto bytes = a.save(); REQUIRE(b.load(bytes) == WS2_OK); REQUIRE(b.save() == bytes);
    Bytes bad = bytes; bad[0] = 99; REQUIRE(b.load(bad) == WS2_UNSUPPORTED); REQUIRE(b.save() == bytes);
    bad = bytes; bad.pop_back(); REQUIRE(b.load(bad) == WS2_INVALID); REQUIRE(b.save() == bytes);
    for (uint32_t mode = 0; mode < 3; ++mode) for (uint32_t extreme = 0; extreme < 2; ++extreme) {
        Rig r(96000, 16);
        for (uint32_t p = 0; p < Rig::parameters; ++p) r.values[p] = extreme ? chaos_lite::ChaosLite::params[p].maximum : chaos_lite::ChaosLite::params[p].minimum;
        r.values[9] = static_cast<float>(mode);
        for (uint32_t c = 0; c < 16; ++c) { r.input_values[0][c] = c & 1 ? 1e6f : -1e6f; r.input_values[3][c] = 1e6f; }
        for (uint32_t block = 0; block < 32; ++block) r.run(256);
    }
    Rig muted; for (auto& x : muted.input_values[1]) x = 0; muted.run(256);
    for (float x : muted.buffers->out[0][0]) REQUIRE(x == 0);
    Rig mono(48000, 4); mono.inputs[0].channels = 1; mono.input_values[0][0] = .5f; mono.run(256);
    REQUIRE(std::strcmp(chaos_lite::ChaosLite::outputs[0].stereo_pair, "stereo") == 0);
    REQUIRE(std::strcmp(chaos_lite::ChaosLite::outputs[1].stereo_pair, "stereo") == 0);
}
static void components() {
    // Parallel coupling is a plain sum at every depth; zero-depth AM preserves it.
    float memory[512]{}; chaos_lite::Coupler coupler; coupler.attach(memory, 512); coupler.reset();
    REQUIRE(coupler.process(.25f, -.5f, 0, 1, 48000) == -.25f);
    REQUIRE(coupler.process(.25f, -.5f, 1, 0, 48000) == -.25f);
    REQUIRE(coupler.process(.25f, -.5f, 1, 1, 48000) == -.625f);
    // A zero-feedback impulse has no response before the shortest comb delay,
    // and eventually leaves the buffer entirely (no self-sustaining feedback).
    float comb_memory[3 * 128]{}; chaos_lite::CombBody body; body.attach(comb_memory, 128); body.reset();
    REQUIRE(body.process(1, 60, 0, .5f) == 0);
    for (int i = 1; i < 29; ++i) REQUIRE(body.process(0, 60, 0, .5f) == 0);
    bool heard = false;
    for (int i = 29; i < 120; ++i) heard = heard || body.process(0, 60, 0, .5f) != 0;
    REQUIRE(heard);
    for (int i = 0; i < 256; ++i) body.process(0, 60, 0, .5f);
    REQUIRE(body.process(0, 60, 0, .5f) == 0);
    Rig r; const auto prior = r.save(); Bytes bad = prior;
    bad[20] = 0; bad[21] = 0; bad[22] = 0xc0; bad[23] = 0x7f;
    REQUIRE(r.load(bad) == WS2_INVALID); REQUIRE(r.save() == prior);
}
static int benchmark() {
    using namespace wirescape::engine2;
    Config config; config.max_block = 128; config.max_channels = 4; config.seed = 42;
    Engine engine(config);
    for (uint32_t node : {1u, 2u}) {
        REQUIRE(engine.add_node(node, ws::Module<chaos_lite::ChaosLite>::descriptor()));
        REQUIRE(engine.set_channels(node, 4)); REQUIRE(engine.output(node, 0, (node - 1) * 2)); REQUIRE(engine.output(node, 1, (node - 1) * 2 + 1));
    }
    REQUIRE(engine.prepare());
    float samples[4][128]{}; float* rows[]{samples[0], samples[1], samples[2], samples[3]};
    for (int i = 0; i < 256; ++i) REQUIRE(engine.render(rows, 4, 128) == WS2_OK);
    std::vector<double> times(10000);
    for (auto& time : times) {
        const auto before = std::chrono::steady_clock::now();
        { allocation_watch::Scope watch; REQUIRE(engine.render(rows, 4, 128) == WS2_OK); REQUIRE(watch.allocations() == 0); }
        time = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - before).count();
    }
    REQUIRE(engine.meter().faults == 0); std::sort(times.begin(), times.end());
    std::printf("{\"graph\":\"chaos_lite_2x4\",\"rate\":48000,\"block\":128,\"blocks\":10000,\"median_us\":%.3f,\"p95_us\":%.3f,\"worst_us\":%.3f,\"setup_bytes\":%zu}\n", (times[4999] + times[5000]) * .5, times[9499], times.back(), engine.meter().setup_bytes);
    return times.back() < 1333.333333 ? 0 : 1;
}
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--benchmark") return benchmark();
    matrix(); state_and_extremes(); components(); std::puts("CHAOS-lite callback matrix/state/extremes/allocation checks passed");
}
