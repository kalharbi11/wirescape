// SPDX-License-Identifier: GPL-3.0-only
// Real native graph proof: module config -> render -> serialized reload.
// Reproduce cppcheck Qt model\'s empty keyword without assuming analyzer-specific defines.
#define emit
#include "../modules2/groove/dsp.h"
#ifndef emit
#error Groove must restore the caller's Qt keyword macro after including the SDK
#endif
#undef emit
#include "../tests2/allocation_watch.h"
#include "runtime_patch.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>
using Json = nlohmann::json;
using Owner = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
static void check(bool b, const char *why) {
    if (!b) throw std::runtime_error(why);
}
static Json pattern() {
    return {{"version", 1},
            {"bpm", 120},
            {"tracks", Json::array({{{"steps", Json::array({{{"kind", "note"}, {"key", 72}, {"velocity", 0.8}},
                                                            {{"kind", "rest"}},
                                                            {{"kind", "note"}, {"key", 64}, {"velocity", 0.5}},
                                                            {{"kind", "tie"}},
                                                            {{"kind", "rest"}},
                                                            {{"kind", "note"}, {"key", 60}, {"velocity", 1}},
                                                            {{"kind", "rest"}},
                                                            {{"kind", "rest"}}})}}})}};
}
static Json cable(uint32_t from, const char *source, uint32_t to, const char *input, const char *kind = "signal") {
    return {{"kind", kind}, {"from", {{"module", from}, {"port", source}}}, {"to", {{"module", to}, {"port", input}}}};
}
static Json patch(Json config, bool cv = false) {
    Json p = {{"format", "wirescape.native.patch"},
              {"version", 2},
              {"settings", {{"max_channels", 1}, {"feedback_block", 1}}},
              {"modules", Json::array({{{"id", 1},
                                        {"module", "sequencing.groove"},
                                        {"version", 1},
                                        {"config", config},
                                        {"parameters", {{"run", 0}}}},
                                       {{"id", 2}, {"module", "essentials.vco"}, {"version", 2}}})},
              {"connections", Json::array()},
              {"outputs", Json::array({{{"module", 2}, {"port", "audio"}, {"bus_channel", 0}}})}};
    if (cv) {
        p["modules"].push_back({{"id", 3}, {"module", "essentials.vca"}, {"version", 2}});
        p["connections"].push_back(cable(1, "t1_pitch", 2, "pitch"));
        p["connections"].push_back(cable(1, "t1_gate", 3, "gain_cv"));
        p["connections"].push_back(cable(2, "audio", 3, "audio"));
        p["outputs"] = Json::array({{{"module", 3}, {"port", "audio"}, {"bus_channel", 0}}});
    } else p["connections"].push_back(cable(1, "t1_notes", 2, "notes", "event"));
    return p;
}
static Owner load(const Json &p, AppEngine *catalog, double rate, std::string &error, bool tap_notes = false) {
    ws_engine_config cfg{sizeof(cfg), rate, 256, 64, 2048};
    std::vector<wirescape::engine2::Tap> taps;
    if (tap_notes) taps.push_back({wirescape::engine2::Tap::EventOut, 1, 4, wirescape::engine2::Tap::First});
    return Owner(wirescape::document::load_runtime_patch(p.dump(), catalog, cfg, error, tap_notes ? &taps : nullptr),
                 app_engine_destroy);
}
static uint32_t note_events(AppEngine *engine) {
    float value = 0;
    uint32_t count = 0;
    uint64_t frame = 0;
    check(app_engine_native(engine).read_telemetry(&value, &count, 1, frame) == 1,
          "native notes telemetry tap configured");
    return count;
}
static uint32_t run_id(AppEngine *engine) {
    for (uint32_t i = 0; i < app_engine_module_count(engine); ++i) {
        const auto *m = app_engine_module(engine, i);
        if (std::string(m->id) == "sequencing.groove") return m->parameters[0].id;
    }
    throw std::runtime_error("Groove not registered in native catalogue");
}
// A rendered graph must receive live parameter events. initialize_parameter
// is quiescent/position-zero only, even if this module is currently stopped.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): parameter id, then value.
static void run_command(AppEngine *e, uint32_t param, float value) {
    ws_event event{};
    event.struct_size = sizeof(event);
    event.type = WS_PARAMETER;
    event.node_id = 1;
    event.parameter_id = param;
    event.value = value;
    event.frame = app_engine_native(e).meter().frame;
    check(app_engine_enqueue(e, &event) == WS_OK, "existing ingress accepts run command");
}
static std::vector<float> trace(AppEngine *e, uint32_t count, uint32_t block, bool verify_cv = false) {
    std::vector<float> out;
    out.reserve(count);
    std::array<float, 256> audio{};
    bool audible = false;
    for (uint32_t done = 0; done < count;) {
        uint32_t n = std::min(count - done, block);
        int result;
        size_t allocations;
        {
            allocation_watch::Scope watch;
            result = app_engine_render(e, audio.data(), n);
            allocations = watch.allocations();
        }
        check(result == WS_OK && allocations == 0, "real render succeeds with no allocation");
        auto &graph = app_engine_native(e);
        for (uint32_t i = 0; i < n; ++i) {
            float g = graph.output_sample(1, 1, 0, i);
            check((g == 0.f || g == 1.f) && std::isfinite(audio[i]), "finite native gate and audio");
            if (verify_cv) {
                if (g == 0.f) check(audio[i] == 0.f, "CV gate closes downstream VCA");
                else if (std::abs(audio[i]) > 0.25f) audible = true;
            }
            check(graph.output_sample(1, 2, 0, i) == 0.f && graph.output_sample(1, 3, 0, i) == 0.f,
                  "inactive modulation lanes are deterministic");
            out.push_back(g);
        }
        done += n;
    }
    if (verify_cv) check(audible, "pitch and gate drive audible native CV patch");
    return out;
}
static void test_refusals(AppEngine *cat) {
    std::string error;
    Json good = pattern();
    for (int c = 0; c < 9; ++c) {
        Json bad = good;
        switch (c) {
        case 0:
            bad["version"] = 2;
            break;
        case 1:
            bad["bpm"] = 0;
            break;
        case 2:
            bad["tracks"] = Json::array();
            break;
        case 3:
            bad["tracks"][0]["steps"][0]["key"] = 128;
            break;
        case 4:
            bad["tracks"][0]["steps"][0]["velocity"] = -1;
            break;
        case 5:
            bad["tracks"][0]["steps"][1]["extra"] = 1;
            break;
        case 6:
            while (bad["tracks"][0]["steps"].size() <= 32) bad["tracks"][0]["steps"].push_back({{"kind", "rest"}});
            break;
        case 7:
            bad["unknown"] = true;
            break;
        default:
            bad["tracks"][0]["steps"][1]["kind"] = "invalid";
            break;
        }
        check(!load(patch(bad), cat, 48000, error), "invalid candidate refused");
        check(!error.empty(), "native explains refusal");
    }
    // A saved run=1 parameter must never bypass the stopped-on-load rule.
    Json armed = patch(good);
    armed["modules"][0]["parameters"]["run"] = 1;
    auto restored = load(armed, cat, 48000, error, true);
    check(bool(restored), "load saved high run parameter");
    const auto silent = trace(restored.get(), 512, 128);
    check(std::all_of(silent.begin(), silent.end(), [](float g) { return g == 0.f; }),
          "saved high run never causes an accidental note-on");
    check(note_events(restored.get()) == 0, "saved run-high emits no notes");
    const auto id = run_id(restored.get());
    run_command(restored.get(), id, 0.f);
    check(trace(restored.get(), 1, 1)[0] == 0.f, "re-arm from saved high");
    run_command(restored.get(), id, 1.f);
    check(trace(restored.get(), 1, 1)[0] == 1.f, "new start edge after reload");
}
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): sample rate, then block size.
static void test_rate_block(AppEngine *cat, double rate, uint32_t block) {
    std::string error;
    auto authored = patch(pattern());
    auto saved = authored.dump(2);
    auto restored = Json::parse(saved);
    check(restored["modules"][0]["config"] == pattern(), "authored JSON is lossless across save/reload");
    auto live = load(authored, cat, rate, error, true);
    auto reload = load(restored, cat, rate, error, true);
    check(bool(live) && bool(reload), "structured pattern loads into real engine");
    check(trace(live.get(), block, block)[0] == 0.f, "fresh patch starts stopped");
    check(trace(reload.get(), block, block)[0] == 0.f, "reloaded patch starts stopped");
    uint32_t id = run_id(live.get());
    run_command(live.get(), id, 1.f);
    run_command(reload.get(), id, 1.f);
    const double period = rate / 8.; // 120 BPM sixteenth note
    const uint32_t count = static_cast<uint32_t>(std::ceil(period * 32)) + 2;
    const auto a = trace(live.get(), count, block);
    const auto b = trace(reload.get(), count, block);
    check(a == b, "native loop identical after serialized reload");
    check(note_events(live.get()) == 8 && note_events(reload.get()) == 8,
          "native NOTE_ON/OFF through 32-step wrap, not the old eight-step boundary");
    const auto at = [&](int step) { return a[static_cast<uint32_t>(std::ceil(period * step))]; };
    check(a[0] == 1.f && at(1) == 0.f && at(2) == 1.f && at(3) == 1.f && at(4) == 0.f && at(5) == 1.f && at(6) == 0.f &&
              at(8) == 0.f && at(31) == 0.f && at(32) == 1.f,
          "note/rest/tie/wrap transition frames");
    // S1 stop queues to the next common 32-step boundary, never acts as S0 run-low.
    const uint32_t stop = app_engine_module(live.get(), 0)->parameters[1].id;
    run_command(live.get(), stop, 1.f);
    check(trace(live.get(), 1, 1)[0] == 1.f, "queued STOP does not truncate step 1");
    const auto until_stop = trace(live.get(), static_cast<uint32_t>(std::ceil(period * 32)) + 2, block);
    check(until_stop.back() == 0.f, "stop completes at the next 32-step boundary");
    // Separate signal route: pitch -> VCO pitch; gate -> VCA gain.
    // Never attach quantized pitch CV and raw note events to the same VCO.
    auto cv = load(patch(pattern(), true), cat, rate, error);
    check(bool(cv), "pitch/gate CV patch loads through native graph");
    check(trace(cv.get(), block, block)[0] == 0.f, "signal patch loads stopped");
    run_command(cv.get(), id, 1.f);
    trace(cv.get(), static_cast<uint32_t>(std::ceil(period * 2)) + 2, block, true);
}
int main() {
    try {
        ws_engine_config cfg{sizeof(cfg), 48000, 256, 64, 2048};
        Owner cat(app_engine_create(&cfg), app_engine_destroy);
        check(bool(cat), "native catalogue creation");
        test_refusals(cat.get());
        for (double rate : {44100., 48000., 96000.})
            for (uint32_t block : {1u, 17u, 64u, 128u, 256u}) test_rate_block(cat.get(), rate, block);
        std::puts("PASS Groove v1 config/refusal/save-reload/stopped/noalloc/32-step timing");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL Groove v1/S1: %s\n", e.what());
        return 1;
    }
}
