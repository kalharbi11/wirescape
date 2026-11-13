// SPDX-License-Identifier: GPL-3.0-only
// Qt-free replacements for the loaded-graph assertions in the retired
// abi2_app_patch_tests. No audio or MIDI device is opened.
#include "../engine2/engine2.h"
#include "../tests2/allocation_watch.h"
#include "runtime_patch.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
namespace {
using Owner = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
Owner load(const std::string &bytes, AppEngine *catalogue, const ws_engine_config &config) {
    std::string error;
    Owner result(wirescape::document::load_runtime_patch(bytes, catalogue, config, error), app_engine_destroy);
    if (!result) throw std::runtime_error("Qt-free runtime patch load: " + error);
    return result;
}
std::string fixture(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "open original CHAOS fixture");
    const std::string text((std::istreambuf_iterator<char>(file)), {});
    require(!text.empty(), "CHAOS fixture is nonempty");
    return text;
}
float render_and_check(AppEngine *graph, int blocks, const char *context) {
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
// Loaded v1 -> serialized v2 -> disk reload -> 96,000 rendered samples.
// This preserves the app-loader-vs-ABI1 bound from Qt abi2_app_patch_tests,
// instead of assuming independent codec/engine unit tests cover that seam.
void serialized_demo_equivalence(AppEngine *catalogue, const ws_engine_config &config,
                                 const std::filesystem::path &modules, const char *demo) {
    using Json = nlohmann::json;
    const std::string original = fixture(demo);
    const auto source = Json::parse(original);
    require(source.at("version") == 1, "Six Essentials fixture remains v1");
    auto saved = source;
    saved["version"] = 2;
    saved["settings"] = {{"max_channels", 16}, {"feedback_block", 1}, {"seed", 1u}};
    saved["outputs"] = Json::array({{{"module", source.at("outputNode")}, {"port", "audio"}, {"bus_channel", 0}},
                                    {{"module", source.at("outputNode")}, {"port", "audio"}, {"bus_channel", 1}}});
    saved["preserved"] = source;
    saved["devices"] = Json::array();
    saved["host_mappings"] = Json::array();
    for (auto &module : saved["modules"]) {
        const auto original_module = module;
        const auto type = module.at("type").get<std::string>();
        module["preserved"] = original_module;
        module["module"] = type;
        module["version"] = 1;
        module["config"] = Json::object();
        module["parameters"] = Json::object();
        for (auto it = module["values"].begin(); it != module["values"].end(); ++it) {
            const char *id = app_engine_parameter_id(catalogue, type.c_str(), uint32_t(std::stoul(it.key())));
            require(id != nullptr, "saved v1 parameter ID resolved in catalogue");
            module["parameters"][id] = it.value();
        }
        module.erase("type");
        module.erase("values");
    }
    for (auto &connection : saved["connections"]) {
        connection["from"] = {{"module", connection["fromNode"]}, {"port", connection["fromPort"]}};
        connection["to"] = {{"module", connection["toNode"]}, {"port", connection["toPort"]}};
        connection["kind"] = "signal";
        for (const char *key : {"fromNode", "fromPort", "toNode", "toPort"}) connection.erase(key);
    }
    const auto temp =
        std::filesystem::temp_directory_path() /
        ("wirescape-qt-free-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct RemoveTemp {
        std::filesystem::path path;
        ~RemoveTemp() {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } cleanup{temp};
    std::filesystem::create_directories(temp);
    const auto saved_path = temp / "saved-v2.json";
    {
        std::ofstream output(saved_path, std::ios::binary | std::ios::trunc);
        require(bool(output), "open v2 save file");
        output << saved.dump(2);
        require(bool(output), "serialize saved v2 file");
    }
    const std::string reread = fixture(saved_path);
    require(Json::parse(reread) == saved, "serialized v2 disk reload preserves complete object");
    auto from_v1 = load(original, catalogue, config);
    auto from_saved_v2 = load(reread, catalogue, config);
    using Legacy = std::unique_ptr<ws_engine, decltype(&ws_engine_destroy)>;
    Legacy old(ws_engine_create(&config), ws_engine_destroy);
    require(bool(old), "create ABI1 reference engine");
    for (const char *name : {"noise", "clock", "function_envelope", "slew", "vcf", "ladder", "vca", "output"}) {
#ifdef _WIN32
        const auto filename = modules / (std::string("ws_") + name + ".dll");
#else
        const auto filename = modules / (std::string("libws_") + name + ".so");
#endif
        require(ws_engine_load_module(old.get(), filename.string().c_str()) == WS_OK, "load ABI1 reference module");
    }
    const auto descriptor = [&](const std::string &type) -> const ws_module_descriptor * {
        for (uint32_t i = 0; i < ws_engine_module_count(old.get()); ++i) {
            const auto *item = ws_engine_module(old.get(), i);
            if (type == item->id) return item;
        }
        throw std::runtime_error("ABI1 descriptor missing");
    };
    const auto node = [&](int id) -> const ws_module_descriptor * {
        for (const auto &module : source["modules"])
            if (module.at("id") == id) return descriptor(module.at("type").get<std::string>());
        throw std::runtime_error("ABI1 node descriptor missing");
    };
    const auto port = [](const ws_port_descriptor *ports, uint32_t count, const std::string &id) -> uint32_t {
        for (uint32_t i = 0; i < count; ++i)
            if (id == ports[i].id) return i;
        throw std::runtime_error("ABI1 port missing");
    };
    for (const auto &module : source["modules"])
        require(ws_engine_add_node(old.get(), module.at("id").get<uint32_t>(),
                                   module.at("type").get<std::string>().c_str()) == WS_OK,
                "ABI1 reference node added");
    for (const auto &cable : source["connections"]) {
        const auto from = cable.at("fromNode").get<uint32_t>();
        const auto to = cable.at("toNode").get<uint32_t>();
        const auto *a = node(int(from));
        const auto *b = node(int(to));
        require(ws_engine_connect(old.get(), from,
                                  port(a->outputs, a->output_count, cable.at("fromPort").get<std::string>()), to,
                                  port(b->inputs, b->input_count, cable.at("toPort").get<std::string>())) == WS_OK,
                "ABI1 reference cable connected");
    }
    require(ws_engine_prepare(old.get(), source.at("outputNode").get<uint32_t>(), 0) == WS_OK,
            "ABI1 reference prepared");
    for (const auto &module : source["modules"])
        for (auto it = module["values"].begin(); it != module["values"].end(); ++it)
            require(ws_engine_initialize_parameter(old.get(), module.at("id").get<uint32_t>(),
                                                   uint32_t(std::stoul(it.key())), it.value().get<float>()) == WS_OK,
                    "ABI1 reference parameter initialized");
    float baseline[128]{}, loaded[128]{}, reloaded[128]{};
    double max_abs_error = 0, baseline_peak = 0;
    size_t allocations = 0, rendered = 0;
    for (int block = 0; block < 750; ++block) {
        require(ws_engine_render(old.get(), baseline, 128) == WS_OK, "ABI1 render");
        int a = -1, b = -1;
        {
            allocation_watch::Scope watch;
            a = app_engine_render(from_v1.get(), loaded, 128);
            b = app_engine_render(from_saved_v2.get(), reloaded, 128);
            allocations += watch.allocations();
        }
        require(a == WS_OK && b == WS_OK, "v1 and saved v2 loaded graphs render");
        for (int i = 0; i < 128; ++i) {
            require(std::isfinite(loaded[i]), "saved v2 render finite");
            require(loaded[i] == reloaded[i], "v1 to disk v2 renders exactly equal PCM samples");
            baseline_peak = std::max(baseline_peak, std::abs(double(baseline[i])));
            max_abs_error = std::max(max_abs_error, std::abs(double(baseline[i]) - loaded[i]));
        }
        rendered += 128;
    }
    require(baseline_peak > 0.0, "ABI1 reference render must not be silent");
    require(allocations == 0, "saved graph callbacks allocate no memory");
    require(max_abs_error <= 1e-7, "ABI1 vs saved/reloaded app-loader bound 1e-7");
    require(rendered == 96000u, "96,000 source and saved-reloaded PCM samples compared");
    std::cout << "v1 -> disk v2 -> reload exact PCM; ABI1 max_abs_error=" << max_abs_error << " allocs=" << allocations
              << '\n';
}
void keyboard_macro(AppEngine *catalogue, const ws_engine_config &config) {
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
        const ws_event on{
            sizeof(ws_event), 0, 1, WS_NOTE_ON, 0, static_cast<float>(key), 1, key, app_engine_generation(graph.get())};
        require(app_engine_enqueue(graph.get(), &on) == WS_OK,
                "enqueue synthetic keyboard chord through loaded Macro graph");
    }
    render_and_check(graph.get(), 64, "keyboard to Macro");
    require(app_engine_native(graph.get()).output_channels(2, 0) == 4, "Macro loader preserves four voices");
    // This asserts off-rate loader refusal only. Sidecar protocol_tests.py
    // separately checks failed loadGraph commits and continued live output.
    for (double rate : {44100.0, 96000.0}) {
        auto rejected_rate = config;
        rejected_rate.sample_rate = rate;
        std::string error;
        Owner rejected(wirescape::document::load_runtime_patch(source, catalogue, rejected_rate, error),
                       app_engine_destroy);
        require(!rejected && !error.empty(), "loaded Macro rejects unsupported rate");
        float previous[128]{};
        size_t allocations = 0;
        int status;
        {
            allocation_watch::Scope watch;
            status = app_engine_render(graph.get(), previous, 128);
            allocations = watch.allocations();
        }
        require(status == WS_OK && allocations == 0 && app_engine_native(graph.get()).meter().faults == 0,
                "off-rate loader refusal leaves the separately loaded graph renderable");
    }
}
void dual_chaos(AppEngine *catalogue, const ws_engine_config &config, const std::filesystem::path &examples) {
    auto graph = load(fixture(examples / "chaos_two.json"), catalogue, config);
    render_and_check(graph.get(), 32, "loaded dual CHAOS");
    const auto &native = app_engine_native(graph.get());
    require(native.output_channels(2, 0) == 4 && native.output_channels(3, 0) == 4,
            "loaded dual CHAOS preserves four voices per instrument");
}
void fake_midi_dual_chaos(AppEngine *catalogue, const ws_engine_config &config, const std::filesystem::path &examples) {
    auto graph = load(fixture(examples / "chaos_usb_midi.json"), catalogue, config);
    auto &native = app_engine_native(graph.get());
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
int main(int argc, char **argv) {
    try {
        require(argc == 4, "usage: qt_free_patch_coverage_tests <examples-dir> <ABI1-modules-dir> <demo>");
        const ws_engine_config config{sizeof(ws_engine_config), 48000, 128, 64, 2048};
        Owner catalogue(app_engine_create(&config), app_engine_destroy);
        require(bool(catalogue), "create Qt-free catalogue");
        const std::filesystem::path examples(argv[1]);
        serialized_demo_equivalence(catalogue.get(), config, argv[2], argv[3]);
        keyboard_macro(catalogue.get(), config);
        dual_chaos(catalogue.get(), config, examples);
        fake_midi_dual_chaos(catalogue.get(), config, examples);
        std::cout << "Qt-free loaded Macro / dual CHAOS / fake MIDI assertions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
