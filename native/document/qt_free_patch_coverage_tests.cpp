// SPDX-License-Identifier: GPL-3.0-only
// Qt-free replacements for the loaded-graph assertions in the retired
// abi2_app_patch_tests. No audio or MIDI device is opened.
#include "runtime_patch.h"
#include "../engine2/engine2.h"
#include "../tests2/allocation_watch.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <iterator>

namespace {
using Owner = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

Owner load(const std::string& bytes, AppEngine* catalogue, const ws_engine_config& config) {
    std::string error;
    Owner result(wirescape::document::load_runtime_patch(bytes, catalogue, config, error),
                 app_engine_destroy);
    if (!result) throw std::runtime_error("Qt-free runtime patch load: " + error);
    return result;
}

std::string fixture(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "open original CHAOS fixture");
    const std::string text((std::istreambuf_iterator<char>(file)), {});
    require(!text.empty(), "CHAOS fixture is nonempty");
    return text;
}

float render_and_check(AppEngine* graph, int blocks, const char* context) {
    float peak = 0.0f;
    float samples[128]{};
    for (int i = 0; i < blocks; ++i) {
        int status = -1;
        size_t allocations = 0;
        {
            allocation_watch::Scope watch;
            status = app_engine_render(graph, samples, 128);
            allocations = watch.allocations();
        }
        if (status != WS_OK || allocations != 0)
            throw std::runtime_error(std::string(context) + ": render or allocation failure");
        for (float sample : samples) {
            require(std::isfinite(sample), "Qt-free loaded graph produces finite samples");
            peak = std::max(peak, std::abs(sample));
        }
    }
    require(peak > 0.0f, "Qt-free loaded graph is non-silent");
    require(app_engine_native(graph).meter().faults == 0, "Qt-free loaded graph is fault-free");
    return peak;
}

void keyboard_macro(AppEngine* catalogue, const ws_engine_config& config) {
    // Same on-disk document shape as the Qt loader test, through runtime_patch.
    const std::string source = R"({
      "format":"wirescape.native.patch","version":2,"name":"Offline keyboard to Macro",
      "settings":{"sample_rate":48000,"max_block_frames":128,"max_channels":16,"seed":42},
      "modules":[
        {"id":1,"module":"host.keyboard","version":1,"parameters":{"voices":4}},
        {"id":2,"module":"ports.macro_oscillator","version":1,"parameters":{}},
        {"id":3,"module":"essentials.output","version":2,"parameters":{}}],
      "connections":[
        {"kind":"signal","from":{"module":1,"port":"pitch"},"to":{"module":2,"port":"pitch"}},
        {"kind":"signal","from":{"module":1,"port":"gate"},"to":{"module":2,"port":"trigger"}},
        {"kind":"signal","from":{"module":1,"port":"velocity"},"to":{"module":2,"port":"level"}},
        {"kind":"signal","from":{"module":2,"port":"main"},"to":{"module":3,"port":"audio"}}],
      "outputs":[{"module":3,"port":"audio","bus_channel":0}]})";
    auto graph = load(source, catalogue, config);
    for (uint32_t key : {60u, 64u, 67u, 72u}) {
        const ws_event on{sizeof(ws_event), 0, 1, WS_NOTE_ON, 0,
                          static_cast<float>(key), 1, key, app_engine_generation(graph.get())};
        require(app_engine_enqueue(graph.get(), &on) == WS_OK,
                "enqueue synthetic keyboard chord through loaded Macro graph");
    }
    render_and_check(graph.get(), 64, "keyboard to Macro");
    require(app_engine_native(graph.get()).output_channels(2, 0) == 4,
            "Macro loader preserves four voices");
    for (double rate : {44100.0, 96000.0}) {
        auto rejected_rate = config;
        rejected_rate.sample_rate = rate;
        std::string error;
        Owner rejected(wirescape::document::load_runtime_patch(
                           source, catalogue, rejected_rate, error), app_engine_destroy);
        require(!rejected && !error.empty(), "loaded Macro rejects unsupported rate");
    }
}

void dual_chaos(AppEngine* catalogue, const ws_engine_config& config,
                const std::filesystem::path& examples) {
    auto graph = load(fixture(examples / "chaos_two.json"), catalogue, config);
    render_and_check(graph.get(), 32, "loaded dual CHAOS");
    const auto& native = app_engine_native(graph.get());
    require(native.output_channels(2, 0) == 4 && native.output_channels(3, 0) == 4,
            "loaded dual CHAOS preserves four voices per instrument");
}

void fake_midi_dual_chaos(AppEngine* catalogue, const ws_engine_config& config,
                          const std::filesystem::path& examples) {
    auto graph = load(fixture(examples / "chaos_usb_midi.json"), catalogue, config);
    auto& native = app_engine_native(graph.get());
    for (uint32_t key : {60u, 64u, 67u, 72u}) {
        ws2_event event{};
        event.struct_size = sizeof(event);
        event.type = WS2_MIDI;
        event.source_id = 1;
        event.channel = -1;
        event.note_id = -1;
        event.ump_words = 1;
        event.ump[0] = 0x20900000u | (key << 8) | 127u;
        require(native.enqueue(0, 1, WS2_DIRECT_PORT, event, 0) == WS2_OK,
                "inject fake USB MIDI into loaded CHAOS graph");
    }
    render_and_check(graph.get(), 16, "fake MIDI to dual CHAOS");
    require(native.output_channels(2, 0) == 4 && native.output_channels(3, 0) == 4,
            "fake MIDI loaded CHAOS preserves four voices per instrument");
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 2, "usage: qt_free_patch_coverage_tests <examples-directory>");
        const ws_engine_config config{sizeof(ws_engine_config), 48000, 128, 64, 2048};
        Owner catalogue(app_engine_create(&config), app_engine_destroy);
        require(bool(catalogue), "create Qt-free catalogue");
        const std::filesystem::path examples(argv[1]);
        keyboard_macro(catalogue.get(), config);
        dual_chaos(catalogue.get(), config, examples);
        fake_midi_dual_chaos(catalogue.get(), config, examples);
        std::cout << "Qt-free loaded Macro / dual CHAOS / fake MIDI assertions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
