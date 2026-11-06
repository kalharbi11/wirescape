// SPDX-License-Identifier: GPL-3.0-only
#include "runtime_patch.h"
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
        require(fixtures.size() == 12, "all twelve UI starters must be exported");
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
                std::set<uint32_t> slots;
                for (uint32_t p = 0; p < native->param_count; ++p) {
                    const auto &param = native->params[p];
                    require(legacy.contains(param.id), id + ": parameter missing from UI legacy map");
                    require(slots.insert(param.legacy_index).second, id + ": duplicate legacy_index");
                    require(param.legacy_index == legacy.at(param.id).get<uint32_t>(),
                            id + ": legacy_index differs from UI catalogue");
                    require(descriptor->params[p].id == param.legacy_index,
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
            for (const auto &module : patch.at("modules")) {
                if (module.at("module") != "essentials.output") continue;
                for (int bus = 0; bus < 2; ++bus) {
                    bool routed = false;
                    for (const auto &route : patch.at("outputs"))
                        routed |= route.at("module") == module.at("id") && route.at("port") == "audio" &&
                                  route.at("bus_channel") == bus;
                    require(routed, name + ": Output missing bus " + std::to_string(bus));
                }
            }
            float samples[256]{};
            require(app_engine_render(engine.get(), samples, 256) == WS_OK, name + ": render failed");
            for (float sample : samples) require(std::isfinite(sample), name + ": non-finite sample");
            ++accepted;
        }

        require(accepted == 12, "all starters must load and render");
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
