// SPDX-License-Identifier: GPL-3.0-only
// Native tests for the browser demo engine's C surface (device-free).
#include "web_engine.h"
#include <cmath>
#include <cstdio>
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

    std::printf(failures ? "web_engine_tests: %d failure(s)\n" : "web_engine_tests: ok\n", failures);
    return failures ? 1 : 0;
}
