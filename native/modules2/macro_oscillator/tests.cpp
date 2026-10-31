// SPDX-License-Identifier: GPL-3.0-only
#include "../../engine2/engine2.h"
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <atomic>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace stmlib {
uint32_t WirescapeUnusedGlobalRandom::rng_state_ = 0x21;
}
struct Arena {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(1024 * 1024);
    size_t used = 0, limit = bytes.size();
    static void *WS2_CALL alloc(void *ctx, size_t bytes, size_t align, uint32_t) {
        auto &a = *static_cast<Arena *>(ctx);
        const uintptr_t base = reinterpret_cast<uintptr_t>(a.bytes.data());
        const uintptr_t next = (base + a.used + align - 1) & ~(uintptr_t(align) - 1);
        if (next - base + bytes > a.limit) return nullptr;
        a.used = next - base + bytes;
        return reinterpret_cast<void *>(next);
    }
};
struct Rig {
    Arena arena;
    const ws2_descriptor *d = ws::Module<macro_oscillator::MacroOscillator>::descriptor();
    ws2_host host{sizeof(host), &arena, Arena::alloc};
    void *instance = nullptr;
    struct Buffers {
        float output[2][16][256]{};
        float input[8][16][256]{};
    };
    std::unique_ptr<Buffers> buffer = std::make_unique<Buffers>();
    const float *input_rows[8][16]{};
    ws2_signal inputs[8]{};
    float values[10]{};
    const float *value_rows[10]{};
    ws2_signal params[10]{};
    float *output_rows[2][16]{};
    ws2_signal_out outputs[2]{};
    ws2_process ctx{};
    explicit Rig(uint32_t count = 1, uint64_t seed = 42, double rate = 48000, bool expect_ok = true) {
        ws2_config config{sizeof(config), rate, 256, 16, seed, nullptr, 0};
        instance = d->create(&host);
        REQUIRE(instance);
        const int prepared = d->prepare(instance, &config, &host);
        if (!expect_ok) {
            REQUIRE(prepared != WS2_OK);
            return;
        }
        REQUIRE(prepared == WS2_OK);
        for (uint32_t p = 0; p < 8; ++p) {
            for (uint32_t c = 0; c < 16; ++c) input_rows[p][c] = buffer->input[p][c];
            inputs[p] = {sizeof(ws2_signal), input_rows[p], count, 0};
        }
        for (uint32_t p = 0; p < 10; ++p) {
            values[p] = macro_oscillator::MacroOscillator::params[p].initial;
            value_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &value_rows[p], 1, WS2_CONSTANT};
        }
        for (uint32_t p = 0; p < 2; ++p) {
            for (uint32_t c = 0; c < 16; ++c) output_rows[p][c] = buffer->output[p][c];
            outputs[p] = {sizeof(ws2_signal_out), output_rows[p], count, 0, 16};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = count;
        ctx.input_count = 8;
        ctx.output_count = 2;
        ctx.param_count = 10;
        ctx.inputs = inputs;
        ctx.outputs = outputs;
        ctx.params = params;
    }
    ~Rig() { d->destroy(instance); }
    void run(uint32_t frames, bool watch = true) {
        ctx.frames = frames;
        int result;
        if (watch) {
            allocation_watch::Scope scope;
            result = d->process(instance, &ctx);
            REQUIRE(scope.allocations() == 0);
        } else result = d->process(instance, &ctx);
        REQUIRE(result == WS2_OK);
        ctx.frame_position += frames;
        for (auto &out : outputs) REQUIRE(!(out.flags & WS2_CONSTANT));
        for (uint32_t p = 0; p < 2; ++p)
            for (uint32_t c = 0; c < ctx.channels; ++c)
                for (uint32_t i = 0; i < frames; ++i) {
                    REQUIRE(std::isfinite(buffer->output[p][c][i]));
                    if (c >= 8) REQUIRE(buffer->output[p][c][i] == 0);
                }
    }
};
static bool checked_multiply(size_t a, size_t b, size_t &result) {
    if (a && b > std::numeric_limits<size_t>::max() / a) return false;
    result = a * b;
    return true;
}
static void checked_sizes() {
    size_t n = 0;
    REQUIRE(checked_multiply(16, 48000, n) && n == 768000);
    REQUIRE(checked_multiply(0, std::numeric_limits<size_t>::max(), n) && n == 0);
    REQUIRE(!checked_multiply(std::numeric_limits<size_t>::max(), 2, n));
    REQUIRE(!checked_multiply(2, std::numeric_limits<size_t>::max(), n));
}
static std::vector<float> render(Rig &r, uint32_t block, uint32_t total = 1024) {
    size_t streams, count;
    if (!block || block > 256 || !checked_multiply(2, r.ctx.channels, streams) ||
        !checked_multiply(streams, total, count) || count > std::vector<float>().max_size())
        throw std::length_error("render sample count overflow or invalid block");
    std::vector<float> result(count);
    for (uint32_t first = 0; first < total; first += block) {
        const uint32_t n = std::min(block, total - first);
        r.run(n);
        for (uint32_t p = 0; p < 2; ++p)
            for (uint32_t c = 0; c < r.ctx.channels; ++c)
                for (uint32_t i = 0; i < n; ++i)
                    result[(size_t(p) * r.ctx.channels + c) * total + first + i] = r.buffer->output[p][c][i];
    }
    return result;
}
struct Reference {
    alignas(plaits::Voice) uint8_t object[sizeof(plaits::Voice)]{};
    uint8_t workspace[16384]{};
    plaits::Voice *voice;
    uint32_t rng = uint32_t(42 ^ 0x9e3779b9u);
    Reference() {
        voice = new (object) plaits::Voice();
        macro_oscillator::RngScope scope(rng);
        stmlib::BufferAllocator allocator(workspace, sizeof(workspace));
        voice->Init(&allocator);
    }
    Reference(const Reference &) = delete;
    Reference &operator=(const Reference &) = delete;
    ~Reference() { voice->~Voice(); }
};
static void direct_reference() {
    for (int model : {0, 8, 10, 13}) {
        Rig adapter;
        adapter.values[0] = float(model);
        const auto actual = render(adapter, 17, 1032);
        Reference reference;
        plaits::Patch patch{};
        patch.note = 60;
        patch.engine = model;
        patch.harmonics = patch.timbre = patch.morph = patch.decay = patch.lpg_colour = .5f;
        plaits::Modulations mods{};
        std::vector<float> expected(2064);
        for (uint32_t first = 12; first < 1032; first += 12) {
            plaits::Voice::Frame frames[12]{};
            {
                macro_oscillator::RngScope scope(reference.rng);
                reference.voice->Render(patch, mods, frames, 12);
            }
            for (uint32_t i = 0; i < 12; ++i) {
                expected[first + i] = frames[i].out / 32768.f;
                expected[1032 + first + i] = frames[i].aux / 32768.f;
            }
        }
        REQUIRE(actual == expected);
    }
}
static void signed_shift_reference() {
#if defined(__aarch64__) || defined(_M_ARM64)
    const char *family = "ARM64";
#elif defined(__x86_64__) || defined(_M_X64)
    const char *family = "x86-64";
#else
    const char *family = "other";
#endif
    uint64_t observed[3]{};
    for (int model : {2, 3, 4}) {
        Rig r;
        r.values[0] = float(model);
        r.inputs[6].flags = WS2_CONNECTED;
        std::fill_n(r.buffer->input[6][0], 256, 1.f);
        const auto samples = render(r, 128, 48000);
        uint64_t hash = 14695981039346656037ull;
        bool nonzero = false;
        for (float sample : samples) {
            nonzero |= sample != 0;
            uint32_t bits;
            std::memcpy(&bits, &sample, sizeof(bits));
            for (unsigned shift = 0; shift < 32; shift += 8) {
                hash ^= (bits >> shift) & 255;
                hash *= 1099511628211ull;
            }
        }
        observed[model - 2] = hash;
        REQUIRE(nonzero);
    }
    std::printf("{\"family\":\"%s\",\"hashes\":[\"%016llx\",\"%016llx\",\"%016llx\"]}\n", family,
                static_cast<unsigned long long>(observed[0]), static_cast<unsigned long long>(observed[1]),
                static_cast<unsigned long long>(observed[2]));
    std::fflush(stdout);
}
static void matrix_and_state() {
    for (int model = 0; model < 24; ++model) {
        Rig whole;
        whole.values[0] = float(model);
        const auto expected = render(whole, 256);
        for (uint32_t block : {1u, 12u, 17u, 128u}) {
            Rig split;
            split.values[0] = float(model);
            REQUIRE(render(split, block) == expected);
        }
        {
            allocation_watch::Scope scope;
            whole.d->reset(whole.instance);
            REQUIRE(scope.allocations() == 0);
        }
        REQUIRE(render(whole, 17) == expected);
    }
    for (uint32_t count : {1u, 4u, 16u}) {
        Rig r(count);
        render(r, 17);
    }
    Rig a;
    a.run(128);
    Bytes bytes;
    ws2_writer writer{sizeof(writer), &bytes, sink};
    REQUIRE(a.d->save_state(a.instance, &writer) == WS2_OK);
    Rig b;
    Source src{&bytes, 0};
    ws2_reader reader{sizeof(reader), &src, source, uint32_t(bytes.size())};
    REQUIRE(b.d->load_state(b.instance, &reader) == WS2_OK);
    Rig fresh;
    REQUIRE(render(b, 17) == render(fresh, 17));
    bytes[0] = 99;
    src.pos = 0;
    REQUIRE(a.d->load_state(a.instance, &reader) == WS2_UNSUPPORTED);
    bytes.clear();
    src.pos = 0;
    reader.bytes = 0;
    REQUIRE(a.d->load_state(a.instance, &reader) == WS2_INVALID);
    Rig wrong_rate(1, 42, 44100, false), wrong_rate2(1, 42, 96000, false);
    REQUIRE(macro_oscillator::MacroOscillator{}.latency_frames() == 12);
    for (int extreme = 0; extreme < 2; ++extreme) {
        Rig r(8);
        for (uint32_t p = 0; p < 10; ++p)
            r.values[p] = extreme ? macro_oscillator::MacroOscillator::params[p].maximum
                                  : macro_oscillator::MacroOscillator::params[p].minimum;
        for (auto &input : r.inputs) input.flags = WS2_CONNECTED;
        for (uint32_t p = 0; p < 8; ++p)
            for (uint32_t c = 0; c < 16; ++c) std::fill_n(r.buffer->input[p][c], 256, p & 1 ? 1e6f : -1e6f);
        for (uint32_t i = 0; i < 32; ++i) r.run(256);
    }
}
static void controls_and_capacity() {
    Rig r(4);
    for (uint32_t p : {0u, 2u, 4u, 5u, 6u, 7u}) r.inputs[p].flags = WS2_CONNECTED;
    for (uint32_t c = 0; c < 4; ++c) {
        std::fill_n(r.buffer->input[0][c], 256, float(c) / 12);
        std::fill_n(r.buffer->input[6][c], 256, 1.f);
        std::fill_n(r.buffer->input[7][c], 256, .8f);
    }
    render(r, 17);
    REQUIRE(!std::equal(r.buffer->output[0][0], r.buffer->output[0][0] + 4, r.buffer->output[0][1]));
    r.ctx.channels = 1;
    r.run(17);
    REQUIRE(r.outputs[0].channels == 1);
    r.ctx.channels = 4;
    r.run(128);
    REQUIRE(r.outputs[0].channels == 4);
    Arena arena;
    ws2_host host{sizeof(host), &arena, Arena::alloc};
    const auto *d = ws::Module<macro_oscillator::MacroOscillator>::descriptor();
    void *state = d->create(&host);
    REQUIRE(state);
    arena.limit = arena.used;
    ws2_config cfg{sizeof(cfg), 48000, 256, 16, 42, nullptr, 0};
    REQUIRE(d->prepare(state, &cfg, &host) != WS2_OK);
    d->destroy(state);
    Rig first, second(1, 43);
    first.values[0] = second.values[0] = 17;
    REQUIRE(render(first, 128) != render(second, 128));
}
static void unmodified_rng_oracle() {
    // The renamed class is the unchanged pinned header, not the adapter implementation.
    for (uint32_t seed : {0u, 1u, 0x12345678u, 0xffffffffu}) {
        stmlib::WirescapeUnusedGlobalRandom::Seed(seed);
        stmlib::Random::Seed(seed);
        for (int i = 0; i < 10000; ++i) {
            REQUIRE(stmlib::Random::GetWord() == stmlib::WirescapeUnusedGlobalRandom::GetWord());
            REQUIRE(stmlib::Random::GetFloat() == stmlib::WirescapeUnusedGlobalRandom::GetFloat());
            REQUIRE(stmlib::Random::GetSample() == stmlib::WirescapeUnusedGlobalRandom::GetSample());
        }
    }
}
static void isolated_rng() {
    Rig left, right, expected;
    left.values[0] = right.values[0] = expected.values[0] = 17;
    const auto reference = render(expected, 128);
    auto a = std::vector<float>(2048), b = a;
    std::atomic<bool> start{false};
    auto worker = [&](Rig &r, std::vector<float> &result) {
        while (!start.load(std::memory_order_acquire)) {
        }
        for (uint32_t i = 0; i < 1024; i += 128) {
            r.run(128, false);
            for (uint32_t p = 0; p < 2; ++p) std::copy_n(r.buffer->output[p][0], 128, result.data() + p * 1024 + i);
        }
    };
    std::thread first(worker, std::ref(left), std::ref(a));
    std::thread second(worker, std::ref(right), std::ref(b));
    start.store(true, std::memory_order_release);
    first.join();
    second.join();
    REQUIRE(a == reference && b == reference);
    // Same-thread interleaving must not couple streams either.
    left.d->reset(left.instance);
    right.d->reset(right.instance);
    for (uint32_t i = 0; i < 8; ++i) {
        left.run(128);
        right.run(128);
        REQUIRE(std::equal(left.buffer->output[0][0], left.buffer->output[0][0] + 128, right.buffer->output[0][0]));
    }
}
static void host_seed_high_bits() {
    Rig low(1, 42), high(1, 42 + (1ULL << 32));
    low.values[0] = high.values[0] = 17;
    const auto low_first = render(low, 128);
    const auto high_first = render(high, 128);
    REQUIRE(low_first != high_first);
    low.d->reset(low.instance);
    high.d->reset(high.instance);
    REQUIRE(render(low, 128) == low_first);
    REQUIRE(render(high, 128) == high_first);
}
static int benchmark() {
    using namespace wirescape::engine2;
    Config config;
    config.max_block = 128;
    config.max_channels = 8;
    Engine engine(config);
    REQUIRE(engine.add_node(1, ws::Module<macro_oscillator::MacroOscillator>::descriptor()));
    REQUIRE(engine.set_channels(1, 8));
    REQUIRE(engine.output(1, 0, 0));
    REQUIRE(engine.output(1, 1, 1));
    const int producer = engine.register_producer();
    REQUIRE(producer >= 0);
    REQUIRE(engine.prepare());
    float main[128]{}, aux[128]{};
    float *rows[]{main, aux};
    double worst = 0;
    std::printf("{\"graph\":\"macro_oscillator_8\",\"rate\":48000,\"block\":128,\"blocks_per_model\":1000,\"setup_"
                "bytes\":%zu,\"upstream_voice_bytes\":%zu,\"models\":[",
                engine.meter().setup_bytes, sizeof(plaits::Voice));
    for (uint32_t model = 0; model < 24; ++model) {
        ws2_event parameter{};
        parameter.struct_size = sizeof(parameter);
        parameter.type = WS2_PARAM;
        parameter.index = 0;
        parameter.value = float(model);
        parameter.channel = -1;
        REQUIRE(engine.enqueue(uint32_t(producer), 1, WS2_DIRECT_PORT, parameter, engine.meter().frame) == WS2_OK);
        for (int i = 0; i < 256; ++i) REQUIRE(engine.render(rows, 2, 128) == WS2_OK);
        std::vector<double> times(1000);
        for (auto &time : times) {
            const auto before = std::chrono::steady_clock::now();
            {
                allocation_watch::Scope scope;
                REQUIRE(engine.render(rows, 2, 128) == WS2_OK);
                REQUIRE(scope.allocations() == 0);
            }
            time = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - before).count();
        }
        std::sort(times.begin(), times.end());
        worst = std::max(worst, times.back());
        std::printf("%s{\"model\":%u,\"median_us\":%.3f,\"p95_us\":%.3f,\"worst_us\":%.3f}", model ? "," : "", model,
                    (times[499] + times[500]) * .5, times[949], times.back());
    }
    std::printf("],\"worst_us\":%.3f}\n", worst);
    return worst < 666.666667 ? 0 : 1;
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--benchmark") return benchmark();
    if (argc == 2 && std::string(argv[1]) == "--shift-hashes") {
        signed_shift_reference();
        return 0;
    }
    checked_sizes();
    direct_reference();
    signed_shift_reference();
    matrix_and_state();
    controls_and_capacity();
    unmodified_rng_oracle();
    isolated_rng();
    host_seed_high_bits();
    std::puts("Macro Oscillator upstream reference, FIFO, matrices and isolation passed");
}
