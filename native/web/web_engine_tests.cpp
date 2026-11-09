// SPDX-License-Identifier: GPL-3.0-only
// Native tests for the browser demo engine's C surface (device-free).
#include "web_engine.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

using Json = nlohmann::json;
static int failures = 0;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                    \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

static Json command(const Json &c) { return Json::parse(wsw_command(c.dump().c_str())); }
static Json load(const std::string &path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return Json::parse(s.str());
}
static float loudest(int blocks) {
    float peak = 0;
    for (int i = 0; i < blocks; ++i) {
        auto *left = wsw_render(128);
        auto *right = wsw_right();
        for (int j = 0; j < 128; ++j) {
            if (!std::isfinite(left[j]) || !std::isfinite(right[j])) return -1;
            peak = std::fmax(peak, std::fmax(std::fabs(left[j]), std::fabs(right[j])));
        }
    }
    return peak;
}

int main(int argc, char **argv) {
    const std::string examples = argc > 1 ? argv[1] : "native/examples";
    CHECK(wsw_command("{}") != nullptr);
    CHECK(wsw_init(1) == 0);
    CHECK(wsw_init(48000) == 1);
    auto catalogue = Json::parse(wsw_catalogue());
    bool has_vco = false, has_midi = false;
    for (auto &m : catalogue["modules"]) {
        auto id = m["id"].get<std::string>();
        has_vco |= id == "essentials.vco";
        has_midi |= id.rfind("midi.", 0) == 0;
    }
    CHECK(has_vco);
    CHECK(!has_midi);

    // No graph yet: silence, and edits are refused.
    CHECK(loudest(4) == 0);
    CHECK(command({{"type", "setParam"}, {"moduleId", 1}, {"paramId", "x"}, {"value", 0}})["code"] == "unavailable");

    auto patch = load(examples + "/chaos_two.json");
    auto reply = command({{"type", "loadGraph"}, {"patch", patch}});
    CHECK(reply["ok"] == true);
    CHECK(reply["gen"] == "1");
    command({{"type", "noteOn"}, {"note", 60}, {"velocity", 0.8}});
    auto t = Json::parse(wsw_telemetry());
    CHECK(t["heldNotes"] == Json::array({60}));
    float peak = loudest(400);
    CHECK(peak > 0.0001f);
    CHECK(peak < 8.f);
    {
        auto live = Json::parse(wsw_telemetry());
        CHECK(live["outputs"].is_object() && !live["outputs"].empty());
        for (auto &[key, value] : live["outputs"].items()) {
            CHECK(key.find(':') != std::string::npos);
            CHECK(value.get<double>() >= -1 && value.get<double>() <= 1);
        }
        CHECK(live["scope"].size() == 1);
        for (auto &[key, points] : live["scope"].items()) CHECK(points.size() == 64);
    }
    command({{"type", "noteOff"}, {"note", 60}});
    CHECK(Json::parse(wsw_telemetry())["heldNotes"].empty());

    // A MIDI patch is refused and the live graph and generation are untouched.
    auto midi = load(examples + "/class_demo_3_macro.json");
    reply = command({{"type", "loadGraph"}, {"patch", midi}});
    CHECK(reply["ok"] == false);
    CHECK(reply["code"] == "unknown-module");
    CHECK(reply["gen"] == "1");

    // Out-of-range parameter is refused.
    uint32_t output = 0, chaos = 0;
    for (auto &m : patch["modules"]) {
        if (m["module"] == "essentials.output") output = m["id"];
        if (m["module"] == "essentials.chaos_lite" && !chaos) chaos = m["id"];
    }
    CHECK(command({{"type", "setParam"}, {"moduleId", chaos}, {"paramId", "nope"}, {"value", 0}})["code"] ==
          "not-found");

    // Topology edits: add a VCO, cable it to the output, then remove it.
    reply = command({{"type", "addModule"}, {"moduleId", 900}, {"moduleType", "essentials.vco"}});
    CHECK(reply["ok"] == true);
    CHECK(reply["gen"] == "2");
    Json cable = {{"from", {{"module", 900}, {"port", "saw"}}},
                  {"to", {{"module", output}, {"port", "left"}}},
                  {"kind", "signal"}};
    reply = command({{"type", "connect"}, {"connection", cable}});
    if (reply["ok"] != true) std::printf("connect: %s\n", reply.dump().c_str());
    reply = command({{"type", "connect"}, {"connection", cable}});
    CHECK(reply["ok"] == false);
    CHECK(loudest(20) >= 0);
    reply = command({{"type", "removeModule"}, {"moduleId", 900}});
    CHECK(reply["ok"] == true);
    CHECK(command({{"type", "removeModule"}, {"moduleId", 900}})["code"] == "not-found");
    CHECK(command({{"type", "addModule"}, {"moduleId", 901}, {"moduleType", "midi.in"}})["code"] == "unknown-module");
    CHECK(command({{"type", "selectAudioDevice"}})["code"] == "invalid");
    CHECK(Json::parse(wsw_command("not json"))["code"] == "invalid");
    CHECK(wsw_render(4096) == nullptr);
    CHECK(loudest(10) >= 0);

    // A3 telemetry is read from engine2's last rendered block, not UI guesses.
    Json telemetry_patch = Json::parse(R"json({
        "format": "wirescape.native.patch",
        "version": 2,
        "settings": {},
        "modules": [
            {"id": 7, "module": "essentials.output"},
            {"id": 9, "module": "essentials.vco", "parameters": {"frequency": 440}},
            {"id": 10, "module": "essentials.lfo"},
            {"id": 12, "module": "host.keyboard"},
            {"id": 14, "module": "essentials.adsr"}
        ],
        "connections": [
            {"kind": "signal", "from": {"module": 9, "port": "audio"},
             "to": {"module": 7, "port": "audio"}},
            {"kind": "modulation", "from": {"module": 10, "port": "control"},
             "to": {"module": 9, "port": "frequency"}, "amount": 0.25},
            {"kind": "signal", "from": {"module": 12, "port": "gate"},
             "to": {"module": 14, "port": "gate"}},
            {"kind": "signal", "from": {"module": 14, "port": "envelope"},
             "to": {"module": 9, "port": "fm"}}
        ]
    })json");
    auto telemetry_reply = command({{"type", "loadGraph"}, {"patch", telemetry_patch}});
    CHECK(telemetry_reply["ok"] == true);
    if (telemetry_reply["ok"] != true) {
        std::printf("telemetry graph refused: %s\n", telemetry_reply.dump().c_str());
        return 1;
    }
    // No render after commit means no source frames: empty maps, not stale data.
    auto empty_telemetry = Json::parse(wsw_telemetry());
    for (auto key : {"effective", "phase", "envelope", "eventActivity"})
        CHECK(empty_telemetry[key].is_object() && empty_telemetry[key].empty());
    bool modulation_observed = false;
    bool phase_advanced = false;
    double last_phase = -1;
    for (int i = 0; i < 40; ++i) {
        loudest(1);
        auto frame = Json::parse(wsw_telemetry());
        CHECK(frame["effective"].contains("9"));
        CHECK(frame["effective"]["9"].contains("frequency"));
        const auto frequency = frame["effective"]["9"].value("frequency", Json{});
        if (!frequency.is_number()) {
            std::printf("missing rendered param tap: %s\n", frame.dump().c_str());
            std::fflush(stdout);
            CHECK(false);
            continue;
        }
        // B-wire: the Web host reads actual channel-0 phase from the LFO output tap.
        const auto phase = frame["phase"].value("10", Json{});
        CHECK(phase.is_number());
        if (phase.is_number()) {
            const double measured = phase.get<double>();
            CHECK(std::isfinite(measured) && measured >= 0 && measured < 1);
            if (last_phase >= 0 && std::abs(measured - last_phase) > 0.0001) phase_advanced = true;
            last_phase = measured;
        }
        CHECK(frame["eventActivity"].empty());
        CHECK(frame["outputs"].is_object() && frame["scope"].is_object());
        if (std::fabs(frequency.get<float>() - 440.f) > 0.01f) modulation_observed = true;
    }
    CHECK(modulation_observed);
    CHECK(phase_advanced); // Must advance between genuine rendered blocks, not a locally invented clock.
    // A burst emits one pulse; a new block without emitted events decays it.
    CHECK(command({{"type", "noteOn"}, {"note", 60}, {"velocity", 0.9}})["ok"] == true);
    loudest(1);
    auto first_burst = Json::parse(wsw_telemetry());
    CHECK(first_burst["eventActivity"].value("12:notes", 0.f) == 1.f);
    CHECK(first_burst["envelope"].value("14", 0.f) > 0.f);
    loudest(1);
    auto decay = Json::parse(wsw_telemetry());
    CHECK(decay["eventActivity"].value("12:notes", 0.f) > 0.f);
    CHECK(decay["eventActivity"].value("12:notes", 0.f) < 1.f);
    // Repeated reads of the same frame publish complete but empty live maps.
    auto stale = Json::parse(wsw_telemetry());
    for (auto key : {"effective", "phase", "envelope", "eventActivity"})
        CHECK(stale[key].is_object() && stale[key].empty());
    // First event after a topology edit may equal the pre-edit cumulative count:
    // commit must reset both the last-seen cursor AND the previous pulse.
    CHECK(command({{"type", "addModule"}, {"moduleId", 13}, {"moduleType", "essentials.vco"}})["ok"] == true);
    CHECK(command({{"type", "noteOn"}, {"note", 61}, {"velocity", 0.9}})["ok"] == true);
    loudest(1);
    CHECK(Json::parse(wsw_telemetry())["eventActivity"].value("12:notes", 0.f) == 1.f);
    CHECK(command({{"type", "loadGraph"}, {"patch", telemetry_patch}})["ok"] == true);
    CHECK(command({{"type", "noteOn"}, {"note", 62}, {"velocity", 0.9}})["ok"] == true);
    loudest(1);
    CHECK(Json::parse(wsw_telemetry())["eventActivity"].value("12:notes", 0.f) == 1.f);
    const auto modulation = telemetry_patch["connections"][1];
    CHECK(command({{"type", "disconnect"}, {"connection", modulation}})["ok"] == true);
    loudest(1);
    CHECK(Json::parse(wsw_telemetry())["effective"].empty());
    // Replacing the graph with no LFO must clear its measured phase immediately.
    Json without_lfo = telemetry_patch;
    without_lfo["modules"] = Json::array();
    without_lfo["modules"].push_back({{"id", 7}, {"module", "essentials.output"}});
    without_lfo["connections"] = Json::array();
    CHECK(command({{"type", "loadGraph"}, {"patch", without_lfo}})["ok"] == true);
    CHECK(Json::parse(wsw_telemetry())["phase"].empty());
    loudest(1);
    CHECK(Json::parse(wsw_telemetry())["phase"].empty());
    // Report JSON snapshot cost in native Web job output. This is diagnostic,
    // not a new performance threshold, and never runs in the realtime engine.
    std::chrono::microseconds telemetry_cost{0};
    for (int i = 0; i < 100; ++i) {
        loudest(1);
        auto start = std::chrono::steady_clock::now();
        (void)wsw_telemetry();
        const auto elapsed = std::chrono::steady_clock::now() - start;
        telemetry_cost += std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
    }
    std::printf("wsw_telemetry 100 frames: %lld us total; %.2f us/sample (native Web CI)\n",
                static_cast<long long>(telemetry_cost.count()), telemetry_cost.count() / 100.);
    // CTest hides successful stdout: publish the same measured diagnostic to
    // the existing Actions step summary, without changing protected CI jobs.
    if (const char *summary = std::getenv("GITHUB_STEP_SUMMARY")) {
        std::ofstream report(summary, std::ios::app);
        report << "Native `wsw_telemetry` JSON: ";
        report << telemetry_cost.count() << " us / 100 frames; ";
        report << telemetry_cost.count() / 100. << " us/sample (diagnostic; not a threshold).\n";
    }

    std::printf(failures ? "web_engine_tests: %d failure(s)\n" : "web_engine_tests: ok\n", failures);
    return failures ? 1 : 0;
}
