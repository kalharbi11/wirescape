// SPDX-License-Identifier: GPL-3.0-only
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

namespace {
constexpr uint32_t kFrames = 256, kVoices = 16;
struct RigSettings {
    double rate = 48000;
    uint32_t voices = 1;
};
struct Rig {
    Pool pool{};
    ws2_host host{sizeof(host), &pool, allocate};
    const ws2_descriptor *descriptor = ws::Module<quad_vca::QuadVca>::descriptor();
    void *state = nullptr;
    struct Data {
        float input[8][kVoices][kFrames]{};
        float output[5][kVoices][kFrames]{};
    };
    std::unique_ptr<Data> data = std::make_unique<Data>();
    const float *input_rows[8][kVoices]{};
    float *output_rows[5][kVoices]{};
    ws2_signal inputs[8]{};
    ws2_signal_out outputs[5]{};
    float values[9]{};
    const float *param_rows[9]{};
    ws2_signal params[9]{};
    ws2_process ctx{};

    explicit Rig(RigSettings settings = {}) {
        const ws2_config cfg{sizeof(cfg), settings.rate, kFrames, kVoices, 123, nullptr, 0};
        state = descriptor->create(&host);
        REQUIRE(state && descriptor->prepare(state, &cfg, &host) == WS2_OK);
        for (uint32_t port = 0; port < 8; ++port) {
            for (uint32_t c = 0; c < kVoices; ++c) input_rows[port][c] = data->input[port][c];
            inputs[port] = {sizeof(ws2_signal), input_rows[port], settings.voices, WS2_CONSTANT};
        }
        for (uint32_t port = 0; port < 5; ++port) {
            for (uint32_t c = 0; c < kVoices; ++c) output_rows[port][c] = data->output[port][c];
            outputs[port] = {sizeof(ws2_signal_out), output_rows[port], settings.voices, 0, kVoices};
        }
        for (uint32_t p = 0; p < 9; ++p) {
            values[p] = quad_vca::QuadVca::params[p].initial;
            param_rows[p] = &values[p];
            params[p] = {sizeof(ws2_signal), &param_rows[p], 1, WS2_CONSTANT};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = settings.rate;
        ctx.channels = settings.voices;
        ctx.input_count = 8;
        ctx.output_count = 5;
        ctx.param_count = 9;
        ctx.inputs = inputs;
        ctx.outputs = outputs;
        ctx.params = params;
    }
    ~Rig() { descriptor->destroy(state); }
    void connect(uint32_t port, bool constant = true) {
        inputs[port].flags = WS2_CONNECTED | (constant ? WS2_CONSTANT : 0u);
    }
    void run(uint32_t frames) {
        ctx.frames = frames;
        int code = WS2_INVALID;
        {
            allocation_watch::Scope no_alloc;
            code = descriptor->process(state, &ctx);
            REQUIRE(no_alloc.allocations() == 0);
        }
        REQUIRE(code == WS2_OK);
        for (auto &output : outputs) REQUIRE(output.channels == ctx.channels);
        ctx.frame_position += frames;
    }
    float sample(uint32_t port, uint32_t voice = 0, uint32_t frame = 0) const {
        return data->output[port][voice][(outputs[port].flags & WS2_CONSTANT) ? 0 : frame];
    }
};

void test_legacy_slots() {
    const auto &params = quad_vca::QuadVca::params;
    REQUIRE(params.size() == 9);
    for (uint32_t p = 0; p < params.size(); ++p) REQUIRE(params[p].legacy_index == p);
}

void test_vca_and_normalling() {
    Rig r;
    r.connect(0);
    r.data->input[0][0][0] = 1.f;
    r.values[0] = .5f;
    r.run(256);
    for (auto &o : r.outputs) REQUIRE(o.flags & WS2_CONSTANT);
    REQUIRE(r.sample(0) == .5f);
    for (uint32_t k = 1; k < 4; ++k) REQUIRE(r.sample(k) == 1.f);
    REQUIRE(r.sample(4) == 3.5f);
    REQUIRE(std::abs(20.0 * std::log10(double(r.sample(0))) + 6.020599913) < .05);

    // The second channel normals from in1, not the attenuated out1.
    r.values[0] = 0.f;
    r.run(17);
    REQUIRE(r.sample(0) == 0.f && r.sample(1) == 1.f && r.sample(4) == 3.f);
    r.connect(1);
    r.data->input[1][0][0] = .25f;
    r.run(17);
    REQUIRE(r.sample(1) == .25f && r.sample(2) == .25f && r.sample(3) == .25f);

    r.data->input[0][0][0] = 2.f;
    r.inputs[1].flags = WS2_CONSTANT; // Restore all-input normalling.
    r.values[0] = 1.f;
    r.run(17);
    for (uint32_t k = 0; k < 4; ++k) REQUIRE(r.sample(k) == 2.f);
    REQUIRE(r.sample(4) == 4.f); // Only the mix is clamped.
}
void test_varying_control() {
    Rig r;
    r.connect(0, false);
    r.connect(4, false);
    r.values[0] = .8f;
    for (uint32_t i = 0; i < 256; ++i) {
        r.data->input[0][0][i] = float(i) / 256.f;
        r.data->input[4][0][i] = float(i) / 512.f;
    }
    r.run(256);
    REQUIRE(!(r.outputs[0].flags & WS2_CONSTANT));
    for (uint32_t i = 0; i < 256; ++i)
        REQUIRE(std::abs(r.sample(0, 0, i) - float(i) / 256.f * .8f * float(i) / 512.f) < 1e-6f);
}
void test_frame_rate_voice_matrix() {
    for (double rate : {32000., 48000., 96000.})
        for (uint32_t voices : {1u, 16u})
            for (uint32_t frames : {1u, 17u, 256u}) {
                Rig r({rate, voices});
                r.connect(0);
                for (uint32_t c = 0; c < voices; ++c) r.data->input[0][c][0] = .2f;
                r.run(frames);
                for (uint32_t c = 0; c < voices; ++c) {
                    REQUIRE(std::abs(r.sample(0, c) - .2f) < 1e-6f);
                    REQUIRE(std::abs(r.sample(4, c) - .8f) < 1e-6f);
                }
                r.values[4] = 1.f;
                r.run(frames);
                for (uint32_t k = 0; k < 5; ++k) {
                    for (uint32_t c = 0; c < voices; ++c) {
                        for (uint32_t i = 0; i < frames; ++i) {
                            REQUIRE(std::isfinite(r.sample(k, c, i)));
                        }
                    }
                }
            }
}
std::vector<float> release_trace(float response) {
    Rig r;
    r.connect(0);
    r.connect(4);
    r.data->input[0][0][0] = .8f;
    r.values[4] = 1.f;
    r.values[8] = response;
    r.data->input[4][0][0] = 1.f;
    for (int i = 0; i < 80; ++i) r.run(256); // settle 0.427 s
    r.data->input[4][0][0] = 0.f;
    std::vector<float> trace;
    for (int i = 0; i < 120; ++i) {
        r.run(256);
        trace.push_back(std::abs(r.sample(0, 0, 255)));
    }
    r.descriptor->reset(r.state);
    r.run(256);
    REQUIRE(std::abs(r.sample(0, 0, 0)) < 1e-7f);
    return trace;
}
void test_lpg_decay() {
    const auto fast = release_trace(0.f), slow = release_trace(1.f);
    REQUIRE(fast[0] > fast[40] && fast[40] > fast[100]);
    REQUIRE(slow[0] > slow[40] && slow[40] > slow[100]);
    REQUIRE(slow[40] > fast[40] * 2.f);
    // The release envelope is monotonic after the initial filter transition.
    for (size_t i = 4; i < fast.size(); ++i) REQUIRE(fast[i] <= fast[i - 1] + 1e-5f);
}
void test_lpg_closing_darkens() {
    constexpr double rate = 48000., pi = 3.141592653589793;
    Rig low({rate}), high({rate});
    for (Rig *r : {&low, &high}) {
        r->connect(0, false);
        r->connect(4);
        r->values[4] = 1.f;
        r->values[8] = .5f;
        r->data->input[4][0][0] = 1.f;
    }
    double held_lo = 0, held_hi = 0, tail_lo = 0, tail_hi = 0;
    auto block = [&](uint32_t block_index, bool held) {
        for (uint32_t i = 0; i < 256; ++i) {
            const double t = (block_index * 256.0 + i) / rate;
            low.data->input[0][0][i] = float(.4 * std::sin(2 * pi * 100 * t));
            high.data->input[0][0][i] = float(.4 * std::sin(2 * pi * 4000 * t));
        }
        low.run(256);
        high.run(256);
        if (held && block_index > 35) {
            for (uint32_t i = 0; i < 256; ++i) {
                held_lo += std::pow(low.sample(0, 0, i), 2);
                held_hi += std::pow(high.sample(0, 0, i), 2);
            }
        }
        if (!held && block_index > 40) {
            for (uint32_t i = 0; i < 256; ++i) {
                tail_lo += std::pow(low.sample(0, 0, i), 2);
                tail_hi += std::pow(high.sample(0, 0, i), 2);
            }
        }
    };
    for (uint32_t n = 0; n < 80; ++n) block(n, true);
    low.data->input[4][0][0] = high.data->input[4][0][0] = 0;
    for (uint32_t n = 0; n < 80; ++n) block(n, false);
    REQUIRE(held_lo > 0 && held_hi > 0 && tail_lo > 0 && tail_hi > 0);
    REQUIRE(tail_hi / held_hi < tail_lo / held_lo);
}
} // namespace

int main() {
    test_legacy_slots();
    test_vca_and_normalling();
    test_varying_control();
    test_frame_rate_voice_matrix();
    test_lpg_decay();
    test_lpg_closing_darkens();
    std::puts("quad VCA: matrix, VCA, normalling, mix, LPG decay/spectrum, reset and no allocations PASS");
}
