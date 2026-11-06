// SPDX-License-Identifier: GPL-3.0-only
#include "app_engine.h"
#include "runtime_module_config.h"
#include "wirescape_sdk.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
using Json = nlohmann::json;
using wirescape::engine2::Engine;
static void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
// Test-only observation of the existing DSP callback surface. Every original
// lifecycle callback and the original DSP run unchanged. No work allocates here.
static int (WS2_CALL *original_process)(void *, ws2_process *);
static uint32_t observed_index;
static bool observe_input;
static double observed;
static bool observed_once;
static int WS2_CALL observe(void *instance, ws2_process *context) {
    const auto & signal = observe_input ? context->inputs[observed_index]
                                       : context->params[observed_index];
    if (signal.channels && context->frames) {
        observed = signal.data[0][signal.flags & WS2_CONSTANT ? 0 : context->frames - 1];
        observed_once = true;
    }
    return original_process(instance, context);
}
static uint32_t slot(const ws2_descriptor *d, const std::string &name, int kind) {
    const uint32_t count = kind == 2 ? d->param_count : kind ? d->output_count : d->input_count;
    for (uint32_t i = 0; i < count; ++i) {
        const char *id = kind == 2 ? d->params[i].id : kind ? d->outputs[i].id : d->inputs[i].id;
        if (name == id) return i;
    }
    throw std::runtime_error("unknown parameter/port");
}
struct Endpoint {
    double plain, normalized;
    std::vector<float> sound;
};
static Endpoint render(const Json &patch, unsigned controller, unsigned value, AppEngine *catalog) {
    const Json *route = nullptr;
    for (const auto & cable : patch.at("connections"))
        if (cable.at("from").at("module") == 9 &&
            cable.at("from").at("port") == "cc" + std::to_string(controller - 20)) {
            check(!route, "CC row must have exactly one target");
            route = &cable;
        }
    check(route, "CC row has no target");
    const uint32_t target = route->at("to").at("module");
    wirescape::engine2::Config config;
    config.sample_rate = 48000;
    config.max_block = 256;
    config.max_channels = patch.at("settings").at("max_channels");
    config.feedback_block = patch.at("settings").at("feedback_block");
    config.seed = patch.at("settings").at("seed");
    // Descriptors outlive the graph; only the selected process entry is wrapped.
    std::map<uint32_t, ws2_descriptor> descriptors;
    Engine engine(config);
    for (const auto & module : patch.at("modules")) {
        const uint32_t node = module.at("id");
        const auto type = module.at("module").get<std::string>();
        const auto * descriptor = app_engine_native_descriptor(catalog, type.c_str());
        check(descriptor, "missing native module");
        auto &copy = descriptors[node];
        copy = *descriptor;
        if (node == target) {
            observe_input = route->at("kind") == "signal";
            observed_index = slot(descriptor, route->at("to").at("port"), observe_input ? 0 : 2);
            original_process = copy.process;
            copy.process = observe;
        }
        check(engine.add_node(node, &copy, wirescape::document::runtime_module_config(
            type, module.value("config", Json::object()))), "add node");
    }
    for (const auto & cable : patch.at("connections")) {
        uint32_t from = cable.at("from").at("module"), to = cable.at("to").at("module");
        const auto output = slot(&descriptors.at(from), cable.at("from").at("port"), 1);
        const bool modulation = cable.at("kind") == "modulation";
        const auto input = slot(&descriptors.at(to), cable.at("to").at("port"), modulation ? 2 : 0);
        check(modulation ? engine.modulate(from, output, to, input, cable.at("amount"))
                         : engine.connect(from, output, to, input), "connect");
    }
    for (const auto & output : patch.at("outputs")) {
        uint32_t node = output.at("module");
        check(engine.output(node, slot(&descriptors.at(node), output.at("port"), 1),
                            output.at("bus_channel")), "output");
    }
    const auto producer = engine.register_producer();
    check(producer >= 0, "producer");
    check(engine.prepare(), "prepare");
    for (const auto & module : patch.at("modules")) {
        uint32_t node = module.at("id");
        for (auto it = module.at("parameters").begin(); it != module.at("parameters").end(); ++it)
            check(engine.set_parameter(node, slot(&descriptors.at(node), it.key(), 2), it.value()), "base parameter");
    }
    auto midi = [&](uint8_t status, uint8_t a, uint8_t b) {
        ws2_event event{};
        event.struct_size = sizeof(event);
        event.type = WS2_MIDI;
        event.source_id = 1;
        event.ump_words = 1;
        event.ump[0] = 0x20000000u | uint32_t(status) << 16 | uint32_t(a) << 8 | b;
        check(engine.enqueue(uint32_t(producer), 1, WS2_DIRECT_PORT, event,
                             engine.meter().frame) == WS2_OK, "normal MIDI ingress");
    };
    float left[256], right[256];
    float *rows[]{left, right};
    Endpoint result{};
    auto audio = [&](unsigned frames, bool capture) {
        while (frames) {
            const auto n = std::min(frames, 256u);
            check(engine.render(rows, 2, n) == WS2_OK, "render");
            for (unsigned i = 0; i < n; ++i) {
                check(std::isfinite(left[i]) && std::isfinite(right[i]), "finite audio");
                if (capture) result.sound.push_back(left[i]);
            }
            frames -= n;
        }
    };
    midi(0xb0, uint8_t(controller), uint8_t(value));
    audio(4800, false); // Settle parameter and 10 ms CC smoothing before note-on.
    observed_once = false;
    midi(0x90, 36, 100);
    audio(24000, true); // Exactly 0.5 s held note per endpoint.
    check(observed_once, "target callback not observed");
    result.plain = observed;
    result.normalized = observe_input ? observed
        : ws::to_normal(descriptors.at(target).params[observed_index], observed);
    // Release controls affect sound after note-off, not during the held note.
    midi(0x80, 36, 0);
    audio(24000, true);
    check(engine.meter().faults == 0, "engine faults");
    return result;
}
int main(int argc, char **argv) {
    try {
        check(argc == 2, "examples directory required");
        ws_engine_config config{sizeof(config), 48000, 256, 64, 2048};
        std::unique_ptr<AppEngine, decltype(&app_engine_destroy)> catalog(app_engine_create(&config),
                                                                       app_engine_destroy);
        check(bool(catalog), "catalog");
        const std::array<const char *, 3> names{
            {"class_demo_1_chaos.json", "class_demo_2_subtractive.json", "class_demo_3_macro.json"}};
        for (unsigned patch_index = 0; patch_index < names.size(); ++patch_index) {
            std::ifstream input(std::string(argv[1]) + "/" + names[patch_index]);
            Json patch;
            input >> patch;
            check(patch.at("devices").empty(), "devices must remain disabled");
            for (unsigned cc = 21; cc <= 28; ++cc) {
                const auto low = render(patch, cc, 0, catalog.get());
                const auto high = render(patch, cc, 127, catalog.get());
                std::cout << names[patch_index] << " CC" << cc << " " << low.plain << " -> " << high.plain << '\n';
                const bool attack = patch_index == 1 && cc == 25;
                const bool release = (patch_index == 1 && cc == 26) || (patch_index == 2 && cc == 27);
                const bool pitch = patch_index == 1 && cc == 27;
                if (attack || release || pitch) {
                    // Linear 30 s/20 kHz ranges: musical envelope/pitch sweeps use
                    // explicit endpoints and >=1.5 physical ratio instead of 12 s/8 kHz travel.
                    const double start = attack ? .01 : release ? .1 : 220;
                    const double end = attack ? 1.509995 : release ? 3.09999 : 659.99978;
                    check(std::abs(low.plain - start) < start * .001, "musical low endpoint");
                    check(std::abs(high.plain - end) < end * .001, "musical high endpoint");
                    check(high.plain / low.plain >= 1.5, "musical effect ratio >=1.5");
                } else {
                    // >=0.4 actual normalized travel is the task's large-sweep criterion;
                    // Macro CC21 measures the real model_cv input (0..1), not a parameter.
                    check(std::abs(high.normalized - low.normalized) >= .4, "effective travel >=0.4");
                }
                if ((patch_index == 1 && cc == 21) || (patch_index == 2 && cc == 25)) {
                    double low_energy = 0, high_energy = 0;
                    for (size_t i = 0; i < 24000; ++i) {
                        low_energy += double(low.sound[i]) * low.sound[i];
                        high_energy += double(high.sound[i]) * high.sound[i];
                    }
                    // Cutoff must cross the C2 fundamental audibly: >=1.5 held-note RMS.
                    check(low_energy > 0 && std::sqrt(high_energy / low_energy) >= 1.5,
                          "cutoff held-note RMS ratio >=1.5");
                }
                double difference = 0;
                for (size_t i = 0; i < low.sound.size(); ++i)
                    difference += std::pow(double(high.sound[i]) - low.sound[i], 2);
                // A measured setting must also reach DSP; 1e-6 RMS rejects unused controls.
                check(std::sqrt(difference / double(low.sound.size())) > 1e-6, "CC must change rendered sound");
            }
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
