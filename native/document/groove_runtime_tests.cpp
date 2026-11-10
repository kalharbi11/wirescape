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
#include "runtime_module_config.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <thread>
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
static uint32_t param_id(AppEngine *engine, uint32_t slot) {
    for (uint32_t i = 0; i < app_engine_module_count(engine); ++i) {
        const auto *m = app_engine_module(engine, i);
        if (std::string(m->id) == "sequencing.groove") return m->parameters[slot].id;
    }
    throw std::runtime_error("Groove not registered in native catalogue");
}
static uint32_t run_id(AppEngine *engine) { return param_id(engine, 0); }
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
    check(note_events(live.get()) == 7 && note_events(reload.get()) == 7,
          "native NOTE_ON/OFF through 32-step wrap, not the old eight-step boundary");
    const auto at = [&](int step) { return a[static_cast<uint32_t>(std::ceil(period * step))]; };
    check(a[0] == 1.f && at(1) == 0.f && at(2) == 1.f && at(3) == 1.f && at(4) == 0.f && at(5) == 1.f && at(6) == 0.f &&
              at(8) == 0.f && at(31) == 0.f && at(32) == 1.f,
          "note/rest/tie/wrap transition frames");
    // S1 stop queues to the next common 32-step boundary, never acts as S0 run-low.
    const uint32_t stop = param_id(live.get(), 1);
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

static Json dual_pattern() {
    Json p = pattern();
    Json first = Json::array();
    Json second = Json::array();
    for (uint32_t i = 0; i < 32; ++i) {
        first.push_back({{"kind", "rest"}});
        second.push_back({{"kind", "rest"}});
    }
    first[0] = {{"kind", "note"}, {"key", 60}, {"velocity", 0.9}, {"mod1", 0.25}, {"mod2", 0.75}};
    first[1] = {{"kind", "tie"}};
    first[3] = {{"kind", "note"}, {"key", 64}, {"velocity", 0}};
    first[31] = {{"kind", "note"}, {"key", 72}, {"velocity", 0.7}, {"mod1", 0.5}, {"mod2", 0.6}};
    second[0] = {{"kind", "note"}, {"key", 67}, {"velocity", 0.8}, {"mod1", 0.4}, {"mod2", 0.6}};
    second[1] = {{"kind", "tie"}};
    second[31] = {{"kind", "note"}, {"key", 69}, {"velocity", 0.9}, {"mod1", 0.8}, {"mod2", 0.2}};
    p["tracks"] = Json::array({{{"steps", first}}, {{"steps", second}}});
    return p;
}
static void advance(AppEngine *e, uint32_t frames, uint32_t block) {
    std::array<float, 256> audio{};
    for (uint32_t done = 0; done < frames;) {
        const uint32_t n = std::min(frames - done, block);
        int status;
        size_t allocated;
        {
            allocation_watch::Scope watch;
            status = app_engine_render(e, audio.data(), n);
            allocated = watch.allocations();
        }
        check(status == WS_OK && allocated == 0, "S1 render is bounded and allocation-free");
        wirescape::engine2::Outgoing out;
        while (app_engine_native(e).pop_egress(out))
            check(out.node == 1 && (out.port == 4 || out.port == 9) &&
                      (out.event.type == WS2_NOTE_ON || out.event.type == WS2_NOTE_OFF) && out.event.note_id > 0 &&
                      out.event.key >= 0 && out.event.key <= 127 && out.frame < app_engine_native(e).meter().frame,
                  "egress identity");
        done += n;
    }
}
static float last(AppEngine *e, uint32_t port) {
    // advance() ends on a non-empty block, and the final sample is always the latest.
    // A one-frame render makes the precise test frame observable independently of block size.
    advance(e, 1, 1);
    return app_engine_native(e).output_sample(1, port, 0, 0);
}
static void test_s1_transport(AppEngine *cat) {
    std::string error;
    auto config = dual_pattern();
    auto saved = Json::parse(patch(config).dump());
    check(saved["modules"][0]["config"] == config, "two-track authored JSON round-trips");
    auto e = load(saved, cat, 48000, error, true);
    check(bool(e), "two-track candidate prepared");
    advance(e.get(), 1, 1); // initial parameter baselines do not issue START
    check(last(e.get(), 1) == 0 && last(e.get(), 6) == 0, "both tracks load stopped");
    run_command(e.get(), param_id(e.get(), 0), 1.f);
    advance(e.get(), 1, 1);
    auto &graph = app_engine_native(e.get());
    check(graph.output_sample(1, 1, 0, 0) == 1.f && graph.output_sample(1, 6, 0, 0) == 1.f,
          "tracks trigger simultaneously at step 1");
    check(graph.output_sample(1, 2, 0, 0) == .25f && graph.output_sample(1, 3, 0, 0) == .75f &&
              std::abs(graph.output_sample(1, 7, 0, 0) - .4f) < 1e-6f &&
              std::abs(graph.output_sample(1, 8, 0, 0) - .6f) < 1e-6f,
          "both sample-hold modulation lanes work independently on note trigger");
    run_command(e.get(), param_id(e.get(), 8), 1.f); // Queue T2 STOP, leave T1 playing.
    advance(e.get(), 1, 1);
    check(graph.output_sample(1, 6, 0, 0) == 1.f, "track STOP is not immediate");
    run_command(e.get(), param_id(e.get(), 6), 1.f); // T1 mute immediate.
    advance(e.get(), 1, 1);
    check(graph.output_sample(1, 1, 0, 0) == 0.f && graph.output_sample(1, 6, 0, 0) == 1.f,
          "independent mute releases only T1");
    check(graph.output_sample(1, 2, 0, 0) == .25f, "muted track holds modulation");
    // Long-run synchronization and a queued track STOP at the shared wrap.
    advance(e.get(), 32 * 6000 + 2, 256);
    check(graph.output_sample(1, 6, 0, 1) == 0.f, "track STOP completes at shared cycle end");
    check(graph.output_sample(1, 1, 0, 1) == 0.f, "T1 mute keeps phase but not gate");
    // T2 re-enters at the next boundary while T1 remains on its own phase.
    run_command(e.get(), param_id(e.get(), 7), 1.f);
    advance(e.get(), 1, 1);
    check(graph.output_sample(1, 6, 0, 0) == 0.f, "track restart queues boundary entry");
    advance(e.get(), 32 * 6000 + 2, 256);
    // With step 1 now passed, output is a rest; the held mod proves that it entered.
    check(std::abs(graph.output_sample(1, 7, 0, 1) - .4f) < 1e-6f,
          "track queued START enters with its own step-1 modulation");
    run_command(e.get(), param_id(e.get(), 1), 1.f); // Global STOP request.
    advance(e.get(), 1, 1);
    advance(e.get(), 32 * 6000 + 2, 256);
    check(graph.output_sample(1, 1, 0, 1) == 0.f && graph.output_sample(1, 6, 0, 1) == 0.f,
          "global queued STOP completes without an extra note");
}
static void test_s1_many_cycles(AppEngine *cat) {
    std::string error;
    Json config = dual_pattern();
    config["bpm"] = 300;
    auto e = load(patch(config), cat, 48000, error, true);
    check(bool(e), "S1 300 BPM graph loads");
    advance(e.get(), 1, 1);
    run_command(e.get(), param_id(e.get(), 0), 1.f);
    advance(e.get(), 1, 1);
    advance(e.get(), 102u * 32u * 2400u, 256);
    check(note_events(e.get()) >= 400, "102 native cycles retain bounded NOTE_ON/OFF activity");
    run_command(e.get(), param_id(e.get(), 1), 1.f);
    run_command(e.get(), param_id(e.get(), 2), 1.f); // RESET outranks STOP.
    advance(e.get(), 1, 1);
    check(app_engine_native(e.get()).output_sample(1, 1, 0, 0) == 0.f, "same-frame RESET releases without retrigger");
    advance(e.get(), 2400, 256);
    check(last(e.get(), 1) == 1.f, "RESET preserves running intent, resumes at step 1");
}
// A full event sink rejects NOTE_OFF without sacrificing its identity or allowing replacement ON.
struct GrooveSaturatedSink {
    std::array<ws2_event, 8> accepted{};
    std::array<uint64_t, 8> frames{};
    uint64_t frame = 100;
    uint32_t count = 0;
    bool block_off = false;
    static int WS2_CALL push(void *ctx, uint32_t port, const ws2_event *event) {
        auto &sink = *static_cast<GrooveSaturatedSink *>(ctx);
        if (port != 4 || (event->type == WS2_NOTE_OFF && sink.block_off) || sink.count == sink.accepted.size())
            return WS2_CAPACITY;
        sink.frames[sink.count] = sink.frame + event->offset;
        sink.accepted[sink.count++] = *event;
        return WS2_OK;
    }
};
static void test_s1_deliberate_saturation() {
    GrooveSaturatedSink sink;
    std::array<ws2_event_out, 10> outputs{};
    for (uint32_t i = 0; i < outputs.size(); ++i)
        outputs[i] = {sizeof(ws2_event_out), &sink, i, &GrooveSaturatedSink::push};
    ws2_process raw{};
    raw.frames = 1;
    raw.output_count = static_cast<uint32_t>(outputs.size());
    raw.event_outputs = outputs.data();
    ws::Block block(raw);
    groove::Sequencer old;
    old.pattern.track_count = 1;
    old.pattern.tracks[0].steps[0] = {groove::Note, 60, 1.f, .25f, .75f};
    old.reset();
    old.running = old.players[0].playing = true;
    old.trigger(block, 0, 0);
    check(sink.count == 1 && sink.accepted[0].type == WS2_NOTE_ON && sink.accepted[0].port == 4 &&
              sink.accepted[0].key == 60 && sink.accepted[0].note_id > 0 && sink.accepted[0].offset == 0 &&
              sink.frames[0] == 100,
          "saturation setup records first note identity");
    const auto first = sink.accepted[0];
    sink.block_off = true;
    old.release(block, 0, 0);
    check(old.players[0].pending.on && !old.players[0].sounding.on && sink.count == 1,
          "saturated NOTE_OFF remains pending and closes the gate");
    old.trigger(block, 0, 0);
    check(sink.count == 1 && old.players[0].pending.id == first.note_id,
          "pending release blocks all replacement NOTE_ON events");
    sink.block_off = false;
    sink.frame = 101;
    check(old.flush(block, 0, 0) && sink.count == 2 && sink.accepted[1].type == WS2_NOTE_OFF &&
              sink.accepted[1].note_id == first.note_id && sink.accepted[1].key == first.key &&
              sink.accepted[1].port == 4 && sink.accepted[1].offset == 0 && sink.frames[1] > sink.frames[0],
          "next-block retry emits exactly the matching NOTE_OFF");
    old.trigger(block, 0, 0);
    check(sink.count == 3 && sink.accepted[2].type == WS2_NOTE_ON && sink.accepted[2].note_id != first.note_id &&
              sink.accepted[2].key == first.key && sink.accepted[2].port == 4 && sink.accepted[2].offset == 0 &&
              sink.frames[2] >= sink.frames[1],
          "new note gets new identity after release");

    // Prepare a replacement while the retiring instance still owes an OFF.
    const auto second = sink.accepted[2];
    sink.block_off = true;
    old.release(block, 0, 0);
    check(old.players[0].pending.on && old.players[0].pending.id == second.note_id,
          "reload candidate begins with old release still pending");
    groove::Sequencer reloaded;
    reloaded.pattern = old.pattern;
    reloaded.reset();
    check(!reloaded.running && !reloaded.players[0].playing && !reloaded.players[0].pending.on &&
              !reloaded.players[0].sounding.on,
          "replacement initializes fully stopped with no inherited note debt");
    reloaded.trigger(block, 0, 0);
    check(sink.count == 3, "replacement cannot auto-trigger on load");
    sink.block_off = false;
    sink.frame = 102;
    check(old.flush(block, 0, 0) && sink.count == 4 && sink.accepted[3].type == WS2_NOTE_OFF &&
              sink.accepted[3].note_id == second.note_id && sink.accepted[3].key == second.key &&
              sink.accepted[3].port == 4 && sink.accepted[3].offset == 0 && sink.frames[3] > sink.frames[2] &&
              !old.players[0].pending.on,
          "retiring instance drains matching OFF without a stranded note");
}

// Real engine integration: overflow substitutes its documented global host choke
// for an undeliverable release; replan then removes the old route and chokes the
// retained receiver before a fresh stopped Groove instance can emit anything.
struct GrooveReleaseFlood {
    static constexpr ws::Info info{"test.groove_release_flood", "Release flood", 1};
    static constexpr std::array<ws2_port, 0> inputs{};
    static constexpr std::array outputs{ws::port("notes", "Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array params{ws::param("fire", "Fire", 0, 1, 0, "", WS2_STEPPED | WS2_AUTOMATABLE)};
    static inline uint32_t admitted = 0;
    bool fired = false;
    void prepare(const ws::Setup &) { reset(); }
    void reset() { fired = false; }
    void process(ws::Block &b) {
        if (admitted || fired || b.param(0).value(0, 0) < 1.f) return;
        fired = true;
        for (uint32_t i = 0; i < 256; ++i) {
            ws2_event e{};
            e.struct_size = sizeof(e);
            e.type = WS2_NOTE_OFF;
            e.note_id = 9000;
            e.key = 1;
            admitted += b.emit(0, e);
        }
    }
};
struct GrooveHostReceiver {
    static constexpr ws::Info info{"test.groove_receiver", "Note receiver", 1};
    static constexpr std::array inputs{ws::port("notes", "Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array outputs{ws::port("held", "Held")};
    static constexpr std::array<ws2_param, 0> params{};
    struct Seen {
        ws2_event event;
        uint64_t frame;
    };
    static inline std::array<Seen, 16> seen{};
    static inline uint32_t count = 0;
    static inline bool overflow = false;
    bool held = false;
    void prepare(const ws::Setup &) { reset(); }
    void reset() { held = false; }
    void process(ws::Block &b) {
        for (const auto &e : b.events()) {
            if (count < seen.size()) seen[count++] = {e, b.frame_position() + e.offset};
            else overflow = true;
            if (e.type == WS2_NOTE_ON) held = true;
            if (e.type == WS2_NOTE_OFF || e.type == WS2_NOTE_CHOKE) held = false;
        }
        b.out(0).set_channels(1);
        b.out(0).mark_constant();
        b.out(0).ch(0)[0] = held ? 1.f : 0.f;
    }
};
static void test_s1_engine_saturation_replan() {
    using wirescape::engine2::Engine;
    using wirescape::engine2::Timing;
    wirescape::engine2::Config limits;
    check(limits.ring_capacity == 256, "fixture saturates unchanged production egress capacity");
    Engine live(limits), candidate(limits);
    auto prepare = [](Engine &engine, int key) {
        groove::Config config;
        config.tracks[0].steps[0] = {groove::Note, key, 1.f, 0.f, 0.f};
        std::vector<uint8_t> bytes(sizeof(config));
        std::memcpy(bytes.data(), &config, sizeof(config));
        check(engine.add_node(1, ws::Module<GrooveReleaseFlood>::descriptor()), "real flood node");
        check(engine.add_node(2, ws::Module<groove::Sequencer>::descriptor(), bytes), "real Groove node");
        check(engine.add_node(3, ws::Module<GrooveHostReceiver>::descriptor()), "real retained note receiver");
        check(engine.connect(2, 4, 3, 0) && engine.output(3, 0, 0), "real Groove notes route");
        check(engine.register_producer() == 0 && engine.prepare(), "real prepared graph");
    };
    prepare(live, 60);
    prepare(candidate, 61); // changed config replaces Groove, retains receiver
    check(candidate.set_parameter(2, 0, 1.f), "saved high START in replacement");
    GrooveHostReceiver::count = GrooveReleaseFlood::admitted = 0;
    GrooveHostReceiver::overflow = false;
    float output[256]{};
    float *rows[]{output};
    auto render = [&](uint32_t frames) {
        allocation_watch::ThreadScope watch;
        check(live.render(rows, 1, frames) == WS2_OK && watch.allocations() == 0,
              "saturated/replanned render is allocation-free");
    };
    auto control = [&](uint32_t node, uint32_t index) {
        ws2_event e{};
        e.struct_size = sizeof(e);
        e.type = WS2_PARAM;
        e.index = index;
        e.value = 1.f;
        check(live.enqueue(0, node, WS2_DIRECT_PORT, e, 0, Timing::Immediate) == WS2_OK, "control ingress");
    };
    render(1);
    control(2, 0);
    render(1);
    wirescape::engine2::Outgoing out;
    check(live.pop_egress(out) && out.node == 2 && out.port == 4 && out.event.type == WS2_NOTE_ON &&
              out.event.note_id > 0 && out.event.key == 60 && out.event.offset == 0 && out.frame == 1,
          "real native NOTE_ON has exact source, port, key, id and absolute frame");
    const auto first = out;
    check(!live.pop_egress(out) && output[0] == 1.f, "receiver holds precisely that note");
    control(1, 0);
    control(2, 3);
    render(1); // fill critical egress, then immediate Groove mute/OFF
    if (GrooveReleaseFlood::admitted != limits.ring_capacity || live.meter().release_emergencies != 1)
        std::fprintf(stderr, "host saturation: admitted=%u emergencies=%llu receiver=%u frame=%llu\n",
                     GrooveReleaseFlood::admitted, static_cast<unsigned long long>(live.meter().release_emergencies),
                     GrooveHostReceiver::count, static_cast<unsigned long long>(live.meter().frame));
    check(GrooveReleaseFlood::admitted == limits.ring_capacity && live.meter().release_emergencies == 1,
          "real critical ring is full and the old release invokes host recovery");
    check(live.pop_egress(out) && out.event.type == WS2_NOTE_CHOKE && out.event.source_id == UINT32_MAX &&
              out.event.note_id == -1 && out.event.channel == -1 && out.event.flags == 1 && out.frame > first.frame,
          "documented global choke releases the external ledger's outstanding note");
    while (live.pop_egress(out)) check(out.event.type != WS2_NOTE_ON, "no ON may intervene after release failure");
    check(GrooveHostReceiver::count == 2 && GrooveHostReceiver::seen[1].event.type == WS2_NOTE_OFF &&
              GrooveHostReceiver::seen[1].event.note_id == first.event.note_id &&
              GrooveHostReceiver::seen[1].event.key == first.event.key && GrooveHostReceiver::seen[1].event.port == 0 &&
              GrooveHostReceiver::seen[1].event.offset == 0 && GrooveHostReceiver::seen[1].frame == 2,
          "internal receiver got the matched OFF before host recovery");
    std::atomic<bool> done{false}, success{false};
    std::thread planner([&] {
        success = live.replan(candidate, 16000000);
        done = true;
    });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!done.load() && std::chrono::steady_clock::now() < deadline) {
        render(1);
        std::this_thread::yield();
    }
    planner.join();
    check(success.load(), "real engine adopts replacement through live replan");
    render(256);
    check(!GrooveHostReceiver::overflow && GrooveHostReceiver::count == 3,
          "no replacement NOTE_ON or extra release reaches the retained receiver");
    const auto &choke = GrooveHostReceiver::seen[2];
    check(choke.event.type == WS2_NOTE_CHOKE && choke.event.port == 0 && choke.event.offset == 0 &&
              choke.event.note_id == -1 && choke.event.source_id == UINT32_MAX && choke.event.flags == 1 &&
              choke.frame >= 3 && choke.frame < live.meter().frame,
          "removed old Groove route delivers the host choke at the next render boundary");
    for (float value : output) check(value == 0.f, "stopped replacement strands no downstream note");
    for (int block = 0; block < 30; ++block) render(256);
    check(GrooveHostReceiver::count == 3 && !live.pop_egress(out) && live.meter().faults == 0,
          "saved-high replacement does not auto-start, re-emit old notes or strand a receiver");
}

static void test_s1_refusals(AppEngine *cat) {
    const Json good = dual_pattern();
    std::string error;
    for (int c = 0; c < 9; ++c) {
        Json invalid = good;
        switch (c) {
        case 0:
            invalid["tracks"][0]["steps"][0]["mod1"] = 1.1;
            break;
        case 1:
            invalid["tracks"][1]["steps"][0]["mod2"] = -0.1;
            break;
        case 2:
            invalid["tracks"][0]["armed"] = 1;
            break;
        case 3:
            invalid["tracks"][1]["mute"] = "no";
            break;
        case 4:
            invalid["tracks"][0]["steps"][0]["extra"] = true;
            break;
        case 5:
            invalid["tracks"][0]["anything"] = 2;
            break;
        case 6:
            invalid["tracks"].push_back(invalid["tracks"][0]);
            break;
        case 7:
            invalid["tracks"][0]["steps"][1]["key"] = 60;
            break;
        default:
            invalid["tracks"][0]["steps"][0]["velocity"] = 1.5;
            break;
        }
        check(!load(patch(invalid), cat, 48000, error), "S1 invalid native config refused");
        check(!error.empty(), "S1 refused candidate explains why");
    }
}


struct GrooveStateBuffer {
    std::array<uint8_t, 32> bytes{};
    uint32_t count = 0, cursor = 0;
    static int WS2_CALL write(void *ctx, const void *source, uint32_t n) {
        auto &self = *static_cast<GrooveStateBuffer *>(ctx);
        if (n > self.bytes.size() - self.count) return WS2_CAPACITY;
        std::memcpy(self.bytes.data() + self.count, source, n);
        self.count += n;
        return WS2_OK;
    }
    static int WS2_CALL read(void *ctx, void *dest, uint32_t n) {
        auto &self = *static_cast<GrooveStateBuffer *>(ctx);
        if (n > self.count - self.cursor) return WS2_INVALID;
        std::memcpy(dest, self.bytes.data() + self.cursor, n);
        self.cursor += n;
        return WS2_OK;
    }
};

static void test_v2_config_and_conditions(AppEngine *cat) {
    using namespace groove;
    Json v2 = {{"version", 2}, {"bpm", 120}, {"tracks", Json::array()}};
    Json short_track = {{"length", 8}, {"steps", Json::array()}};
    short_track["steps"].push_back({{"kind", "note"}, {"key", 60}, {"velocity", 1},
        {"gate", 0.25}, {"mod1", 0.2}, {"cond", {{"type", "chance"}, {"p", 50}}}});
    short_track["steps"].push_back({{"kind", "tie"}, {"gate", 0.75}});
    Json long_track = {{"length", 64}, {"steps", Json::array()}};
    long_track["steps"].push_back({{"kind", "note"}, {"key", 72}, {"velocity", 1},
        {"cond", {{"type", "ab"}, {"a", 1}, {"b", 4}}}});
    v2["tracks"].push_back(short_track);
    v2["tracks"].push_back(long_track);
    const auto converted = wirescape::document::runtime_module_config("sequencing.groove", v2, 17);
    Config decoded;
    check(decode(converted.data(), static_cast<uint32_t>(converted.size()), decoded), "v2 transient config validates");
    check(decoded.version == 2 && decoded.instance_seed == 17 && decoded.track_count == 2 &&
          decoded.tracks[0].length == 8 && decoded.tracks[1].length == 64 &&
          decoded.tracks[0].steps[0].gate_q == 1 &&
          decoded.tracks[0].steps[0].cond_type == Chance &&
          decoded.tracks[0].steps[0].cond_a == 50 &&
          decoded.tracks[0].steps[1].gate_q == 3 &&
          decoded.tracks[1].steps[0].cond_type == AB &&
          decoded.tracks[1].steps[0].cond_a == 1 &&
          decoded.tracks[1].steps[0].cond_b == 4,
          "v2 length, gate and condition fields map exactly");
    const auto different = wirescape::document::runtime_module_config("sequencing.groove", v2, 18);
    Config different_seed;
    check(decode(different.data(), static_cast<uint32_t>(different.size()), different_seed) &&
          different_seed.instance_seed != decoded.instance_seed,
          "distinct graph instances keep independent deterministic seeds");

    const auto legacy = wirescape::document::runtime_module_config("sequencing.groove", pattern(), 19);
    Config migrated;
    check(decode(legacy.data(), static_cast<uint32_t>(legacy.size()), migrated) &&
          migrated.version == 2 && migrated.instance_seed == 0 &&
          migrated.tracks[0].length == 32 &&
          migrated.tracks[0].steps[0].gate_q == 4 &&
          migrated.tracks[0].steps[0].cond_type == Always,
          "authored v1 preserves 32-step, full-gate, unconditional semantics");
    LegacyConfig native_v1;
    native_v1.tracks[0].steps[0] = {Note, 72, 0.8f, 0.25f, 0.75f};
    Config converted_v1;
    check(decode(reinterpret_cast<const uint8_t *>(&native_v1), sizeof(native_v1), converted_v1) &&
          converted_v1.tracks[0].steps[0].kind == Note &&
          converted_v1.tracks[0].steps[0].key == 72 &&
          converted_v1.tracks[0].steps[0].gate_q == 4,
          "legacy transient v1 layout decodes without changing note fields");

    auto refused = [&](const Json &candidate) {
        std::string error;
        check(!load(patch(candidate), cat, 48000, error) && !error.empty(),
              "invalid Groove v2 candidate refused by real patch loader");
    };
    Json bad = v2;
    bad["tracks"][0]["length"] = 12; refused(bad);
    bad = v2;
    bad["tracks"][0]["steps"][0]["gate"] = 0.3; refused(bad);
    bad = v2;
    bad["tracks"][0]["steps"][0]["cond"]["p"] = 100; refused(bad);
    bad = v2;
    bad["tracks"][0]["steps"][0]["cond"] = {{"type", "ab"}, {"a", 2}, {"b", 1}}; refused(bad);
    bad = v2;
    bad["tracks"][0]["steps"][1]["cond"] = {{"type", "first"}}; refused(bad);
    bad = v2;
    bad["tracks"][0]["steps"][0]["keys"] = Json::array({60, 64}); refused(bad);
    bad = v2;
    bad["tracks"][0]["steps"] = Json::array();
    for (uint32_t i = 0; i < 9; ++i) bad["tracks"][0]["steps"].push_back({{"kind", "rest"}});
    refused(bad);
    bad = v2; bad["version"] = 3; refused(bad);
    std::string error;
    check(bool(load(patch(v2), cat, 48000, error)), "v2 accepted through the real native patch loader");

    Sequencer seq;
    seq.pattern = decoded;
    seq.rng_seed = 12345;
    seq.reset();
    auto &p = seq.players[0];
    Step cond = decoded.tracks[0].steps[0];
    for (uint32_t loop = 1; loop <= 16; ++loop) {
        p.loop = loop;
        cond.cond_type = AB; cond.cond_a = 2; cond.cond_b = 4;
        check(seq.condition_passes(cond, p) == ((loop - 1) % 4 == 1), "A:B repeats on track loops");
        cond.cond_type = First;
        check(seq.condition_passes(cond, p) == (loop == 1), "first condition");
        cond.cond_type = NotFirst;
        check(seq.condition_passes(cond, p) == (loop != 1), "not-first condition");
    }
    cond.cond_type = Chance; cond.cond_a = 50; cond.cond_b = 0;
    p.rng = seq.seed_for(0);
    uint32_t hits = 0;
    for (uint32_t i = 0; i < 1000; ++i) hits += seq.condition_passes(cond, p);
    check(hits >= 450 && hits <= 550, "fixed-seed 50 percent chance remains in bounded distribution band");
    p.rng = seq.seed_for(0);
    uint32_t replay = 0;
    for (uint32_t i = 0; i < 1000; ++i) replay += seq.condition_passes(cond, p);
    check(hits == replay, "chance RNG repeats after Reset/Start seed");
    check(seq.seed_for(0) != seq.seed_for(1), "per-track chance streams are independent");
    GrooveStateBuffer state;
    seq.players[0].loop = 123;
    seq.players[0].rng = 456;
    seq.players[1].loop = 78;
    seq.players[1].rng = 90;
    ws2_writer writer{sizeof(ws2_writer), &state, GrooveStateBuffer::write};
    check(seq.save_state(ws::Writer(&writer)) == WS2_OK && state.count == 20,
          "Groove state v2 saves version and two loop/RNG pairs");
    seq.reset();
    ws2_reader reader{sizeof(ws2_reader), &state, GrooveStateBuffer::read, state.count};
    check(seq.load_state(ws::Reader(&reader)) == WS2_OK && state.cursor == state.count &&
          seq.players[0].loop == 123 && seq.players[0].rng == 456 &&
          seq.players[1].loop == 78 && seq.players[1].rng == 90,
          "Groove state v2 reload keeps both exact loop counters and RNG states");
    state.cursor = 0;
    state.count = 4;
    const uint32_t old_version = 1;
    std::memcpy(state.bytes.data(), &old_version, 4);
    check(seq.load_state(ws::Reader(&reader)) == WS2_OK &&
          seq.players[0].loop == 1 && seq.players[1].loop == 1 &&
          seq.players[0].rng == seq.seed_for(0) && seq.players[1].rng == seq.seed_for(1),
          "legacy Groove state v1 defaults loop and RNG without restoring stale notes");
    seq.players[0].playing = seq.players[1].playing = true;
    check(seq.master_steps() == 64, "master cycle follows longest active track");
    seq.players[1].playing = seq.players[1].armed = false;
    check(seq.master_steps() == 8, "master cycle follows remaining shorter track");
    // Native sixteenth-note scheduling without audio devices. No note is sounding,
    // so the exact boundary path can be driven with an empty event sink.
    seq.reset();
    seq.running = true;
    seq.players[0].playing = seq.players[1].playing = true;
    ws2_process raw{};
    raw.frames = 1;
    ws::Block dummy(raw);
    for (uint32_t i = 0; i < 8; ++i) seq.boundary(dummy, 0);
    check(seq.step == 8 && seq.step_t[0] == 0 && seq.step_t[1] == 8 &&
          seq.players[0].loop == 2 && seq.players[1].loop == 1,
          "8-step track wraps independently while 64-step track continues");
    seq.players[0].stop = true;
    for (uint32_t i = 0; i < 8; ++i) seq.boundary(dummy, 0);
    check(seq.step == 16 && !seq.players[0].playing && !seq.players[0].armed &&
          seq.players[1].playing && seq.players[1].loop == 1,
          "track STOP completes on its own loop, not the master end");
    seq.players[0].enter = true;
    seq.players[0].armed = true;
    for (uint32_t i = 0; i < 16; ++i) seq.boundary(dummy, 0);
    check(seq.step == 32 && seq.players[0].playing && seq.players[1].playing,
          "track START enters on a matching 16-step bar boundary");
    seq.stop_queued = true;
    for (uint32_t i = 0; i < 31; ++i) {
        seq.boundary(dummy, 0);
        check(seq.running && seq.stop_queued, "global STOP stays queued until master end");
    }
    seq.boundary(dummy, 0);
    check(!seq.running && !seq.stop_queued && seq.step == 0 && seq.step_t[0] == 0 &&
          seq.step_t[1] == 0 && !seq.players[0].playing && !seq.players[1].playing,
          "global STOP completes only at the 64-step master-cycle boundary");
    seq.samples_per_step = 100;
    bool shorter = false;
    check(seq.gate_duration(0, 0, shorter) == 175. && shorter,
          "the last tied step's three-quarter gate sets the release span");
}

int main() {
    try {
        ws_engine_config cfg{sizeof(cfg), 48000, 256, 64, 2048};
        Owner cat(app_engine_create(&cfg), app_engine_destroy);
        check(bool(cat), "native catalogue creation");
        test_refusals(cat.get());
        test_s1_refusals(cat.get());
        test_v2_config_and_conditions(cat.get());
        test_s1_transport(cat.get());
        test_s1_deliberate_saturation();
        test_s1_many_cycles(cat.get());
        test_s1_engine_saturation_replan();
        for (double rate : {44100., 48000., 96000.})
            for (uint32_t block : {1u, 17u, 64u, 128u, 256u}) test_rate_block(cat.get(), rate, block);
        std::puts("PASS Groove v1 compatibility, v2 config/conditions, stopped/noalloc/clock");
    } catch (const std::exception &e) {
        std::fprintf(stderr, "FAIL Groove v1/S1: %s\n", e.what());
        return 1;
    }
}
