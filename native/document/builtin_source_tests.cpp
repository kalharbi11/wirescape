// SPDX-License-Identifier: GPL-3.0-only
#include "runtime_patch.h"
#include "../tests2/allocation_watch.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>

using Json = nlohmann::json;
using Owner = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
static void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
static Json cable(unsigned from, const char *out, unsigned to, const char *in, const char *kind = "signal") {
    return {{"kind", kind}, {"from", {{"module", from}, {"port", out}}},
            {"to", {{"module", to}, {"port", in}}}};
}
static Json patch(bool sequencer, uint64_t seed) {
    Json modules = Json::array({
        {{"id", 1}, {"module", "essentials.clock"}, {"version", 2},
         {"parameters", {{"bpm", 300}, {"ticks_per_beat", 24}}}},
        {{"id", 2}, {"module", sequencer ? "example.step_sequencer" : "example.random_source"}, {"version", 1},
         {"parameters", sequencer ? Json{{"length", 2}, {"pitch1", 0}, {"pitch2", 1}}
                                  : Json{{"range", 0.75}, {"smooth", 0.01}}}}});
    Json cables = Json::array({cable(1, "clock", 2, "clock")});
    if (sequencer) {
        modules.push_back({{"id", 3}, {"module", "essentials.vco"}, {"version", 2}});
        cables.push_back(cable(2, "notes", 3, "notes", "event"));
    }
    return {{"format", "wirescape.native.patch"}, {"version", 2},
            {"settings", {{"seed", seed}, {"max_channels", 1}, {"feedback_block", 1}}},
            {"modules", modules}, {"connections", cables},
            {"outputs", Json::array({{{"module", sequencer ? 3 : 2},
                                      {"port", sequencer ? "audio" : "step"}, {"bus_channel", 0}}})}};
}
static Owner load(const Json &patch, AppEngine *catalogue) {
    ws_engine_config config{sizeof(config), 48000, 256, 64, 2048};
    std::string error;
    Owner engine(wirescape::document::load_runtime_patch(patch.dump(), catalogue, config, error), app_engine_destroy);
    if (!engine) throw std::runtime_error(error);
    return engine;
}
static void render(AppEngine *engine, float *out, uint32_t frames) {
    int result;
    size_t allocations;
    {
        allocation_watch::Scope watch;
        result = app_engine_render(engine, out, frames);
        allocations = watch.allocations();
    }
    require(result == WS_OK && allocations == 0, "device-free render must succeed without allocation");
    require(app_engine_native(engine).meter().faults == 0, "no engine faults");
}
static void sequencer(AppEngine *catalogue) {
    auto engine = load(patch(true, 42), catalogue);
    // Only the notes cable can set the VCO to C4 then C5; its default is A3.
    // Observe the rendered clock: its fractional phase can cross a tick one sample
    // later than nominal, so integer frame/400 is not the actual trigger time.
    std::array<float, 1200> audio{}, clock{};
    for (unsigned frame = 0; frame < audio.size(); frame += 200) {
        render(engine.get(), audio.data() + frame, 200);
        for (unsigned i = 0; i < 200; ++i)
            clock[frame + i] = app_engine_native(engine.get()).output_sample(1, 0, 0, i);
    }
    double phase = 0;
    double peak = 0;
    unsigned edges = 0;
    bool clock_high = false;
    constexpr double tau = 6.283185307179586476925286766559;
    for (unsigned frame = 0; frame < audio.size(); ++frame) {
        const bool high = clock[frame] > 0.5f;
        if (high && !clock_high) ++edges;
        clock_high = high;
        require(std::isfinite(audio[frame]), "finite sequence audio");
        require(std::abs(audio[frame] - std::sin(phase)) < 1e-5, "clocked notes set the VCO pitch at each step");
        peak = std::max(peak, double(std::abs(audio[frame])));
        const double hz = (edges % 2 == 0) ? 523.2511306011972 : 261.6255653005986;
        phase += tau * hz / 48000;
        if (phase >= tau) phase -= tau;
    }
    require(edges == 3, "three clock rises drive step 1, step 2 and wrap");
    require(peak > 0.5, "sequencer notes render non-silent through VCO");
}
static std::vector<float> random_render(AppEngine *engine) {
    std::vector<float> result;
    result.reserve(8192);
    float audio[256];
    for (unsigned block = 0; block < 16; ++block) {
        render(engine, audio, 256);
        auto &native = app_engine_native(engine);
        for (unsigned i = 0; i < 256; ++i) {
            require(audio[i] == native.output_sample(2, 0, 0, i), "random step is routed to bus");
            for (unsigned port = 0; port < 2; ++port) {
                const float sample = native.output_sample(2, port, 0, i);
                require(std::isfinite(sample) && sample >= -0.75f && sample <= 0.75f, "random output in range");
                result.push_back(sample);
            }
        }
    }
    return result;
}
static void random_source(AppEngine *catalogue) {
    auto first = load(patch(false, 42), catalogue);
    auto replay = load(patch(false, 42), catalogue);
    auto other = load(patch(false, 43), catalogue);
    const auto expected = random_render(first.get());
    require(expected == random_render(replay.get()), "same seed gives bit-identical step and smooth output");
    require(expected != random_render(other.get()), "different seed changes the random sequence");
    bool changed = false, smoothing = false;
    for (size_t i = 2; i < expected.size(); i += 2) {
        changed |= expected[i] != expected[i - 2];
        smoothing |= expected[i] != expected[i + 1];
    }
    require(changed && smoothing, "clock advances random values and smooth output follows");
    require(app_engine_reset(first.get()) == WS_OK, "reset random graph");
    require(expected == random_render(first.get()), "reset replays the seeded graph");
}
int main() {
    try {
        ws_engine_config config{sizeof(config), 48000, 256, 64, 2048};
        Owner catalogue(app_engine_create(&config), app_engine_destroy);
        require(bool(catalogue), "create builtin catalogue");
        sequencer(catalogue.get());
        random_source(catalogue.get());
        std::cout << "builtin sequencer notes and seeded random device-free renders pass\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
