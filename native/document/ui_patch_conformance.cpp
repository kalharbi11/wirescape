// SPDX-License-Identifier: GPL-3.0-only
#include "runtime_patch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <nlohmann/json.hpp>
#include <stdexcept>

using Json = nlohmann::json;
using Owner = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;

static void require(bool ok, const std::string &why) {
    if (!ok) throw std::runtime_error(why);
}

int main(int argc, char **argv) {
    try {
        require(argc == 3, "fixture and UI catalogue paths required");
        std::ifstream fixtures_file(argv[1]), catalogue_file(argv[2]);
        const auto fixtures = Json::parse(fixtures_file), ui = Json::parse(catalogue_file);
        require(fixtures.size() == 15, "all fifteen UI starters must be exported");
        ws_engine_config config{sizeof(config), 48000, 256, 64, 2048};
        Owner catalogue(app_engine_create(&config), app_engine_destroy);
        require(bool(catalogue), "create catalogue without devices");
        for (uint32_t i = 0; i < app_engine_module_count(catalogue.get()); ++i) {
            auto *descriptor = app_engine_module(catalogue.get(), i);
            auto *native = app_engine_native_descriptor(catalogue.get(), descriptor->id);
            bool found = false;
            for (const auto &module : ui.at("modules")) {
                if (module.at("id") != descriptor->id) continue;
                require(module.at("version") == native->version, "catalogue module version drift");
                const auto &legacy = module.at("legacy_parameters");
                const std::string id = descriptor->id;
                require(legacy.size() == native->param_count, id + ": parameter slot count drift");
                std::set<uint32_t> used_legacy_indices;
                for (uint32_t p = 0; p < native->param_count; ++p) {
                    const auto &param = native->params[p];
                    require(legacy.find(param.id) != legacy.end(), id + ": parameter missing from UI legacy map");
                    const bool unique_slot = used_legacy_indices.insert(param.legacy_index).second;
                    require(unique_slot, id + ": duplicate legacy_index");
                    require(param.legacy_index == legacy.at(param.id).get<uint32_t>(),
                            id + ": legacy_index differs from UI catalogue");
                    require(descriptor->parameters[p].id == param.legacy_index,
                            id + ": facade numeric slot differs from native descriptor");
                }
                found = true;
            }
            require(found, "native module missing in UI catalogue");
        }
        size_t accepted = 0;
        for (const auto &fixture : fixtures) {
            const auto name = fixture.at("id").get<std::string>();
            const auto &patch = fixture.at("patch");
            const auto before = patch.dump();
            std::string error;
            Owner engine(wirescape::document::load_runtime_patch(before, catalogue.get(), config, error),
                         app_engine_destroy);
            require(patch.dump() == before, name + ": execution mutated editable data");
            require(bool(engine), (name + ": ").append(error));
            bool has_bus[2]{false, false};
            for (const auto &route : patch.at("outputs")) {
                if (route.at("port") != "audio") continue;
                const int bus = route.at("bus_channel").get<int>();
                if (bus < 0 || bus >= 2) continue;
                bool from_output = false;
                for (const auto &module : patch.at("modules")) {
                    from_output |= module.at("module") == "essentials.output" && module.at("id") == route.at("module");
                }
                require(from_output, name + ": output bus references a non-Output module");
                require(!has_bus[bus], name + ": duplicate output bus route");
                has_bus[bus] = true;
            }
            require(has_bus[0] && has_bus[1], name + ": missing stereo output bus");
            // Automatic patches must actually emit sound without any MIDI/key event.
            // Run long enough to cover the first clock edge at the authored slow tempi.
            const bool horror_pad = name == "hollow-chapel" || name == "breathing-walls";
            const bool stock_drone = name == "dust-reactor";
            const bool sequence = name == "witching-hour" || name == "neon-elegy";
            const bool autoplay = horror_pad || stock_drone || sequence;
            float left[256]{}, right[256]{}, peak = 0.f, stereo_difference = 0.f;
            float *channels[]{left, right};
            for (int block = 0; block < (autoplay ? 256 : 1); ++block) {
                require(app_engine_render_channels(engine.get(), channels, 2, 256) == WS_OK, name + ": render failed");
                for (int i = 0; i < 256; ++i) {
                    require(std::isfinite(left[i]) && std::isfinite(right[i]), name + ": non-finite sample");
                    peak = std::max(peak, std::max(std::fabs(left[i]), std::fabs(right[i])));
                    stereo_difference = std::max(stereo_difference, std::fabs(left[i] - right[i]));
                }
            }
            if (autoplay) require(peak > 1.e-5f, name + ": autoplay produced silence");
            if (name == "hollow-chapel")
                require(stereo_difference > 1.e-5f, name + ": stereo output collapsed to mono");
            ++accepted;
        }

        require(accepted == 15, "all starters must load and render");
        // Unknown identities still fail closed without changing editable data.
        auto unknown = fixtures.at(0).at("patch");
        unknown["modules"][0]["module"] = "test.unknown_module";
        const auto before = unknown.dump();
        std::string error;
        Owner refused(wirescape::document::load_runtime_patch(before, catalogue.get(), config, error),
                      app_engine_destroy);
        require(!refused && error == "unknown-module", "genuinely unknown module must be refused");
        require(unknown.dump() == before, "refusal mutated editable data");
        std::cout << "UI starters: " << accepted << " loaded, 1 unknown-module refusal\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
