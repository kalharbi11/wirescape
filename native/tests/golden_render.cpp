// SPDX-License-Identifier: GPL-3.0-only
#include "module_catalog.h"
#include "runtime_module_config.h"
#include "runtime_patch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <vector>
using namespace wirescape::document;
using Json = nlohmann::json;
using Bytes = std::vector<uint8_t>;
using App = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
static void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
// The byte order is the frozen Qt renderer contract: IEEE float bits and every
// event field are little-endian u32; payload bytes follow without padding.
static void u32(Bytes &bytes, uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(uint8_t(value >> shift));
}
static void f32(Bytes &bytes, float value) {
    require(std::isfinite(value), "nonfinite golden value");
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    u32(bytes, bits);
}
static std::string hex(const Bytes &bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (auto byte : bytes) {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}
static void event_bytes(Bytes &bytes, const wirescape::engine2::Outgoing &out) {
    const auto &e = out.event;
    require(e.bytes <= WS2_MAX_PAYLOAD && e.ump_words <= 4, "invalid golden event payload");
    u32(bytes, static_cast<uint32_t>(out.frame));
    u32(bytes, static_cast<uint32_t>(out.frame >> 32));
    for (uint32_t value : {out.node, out.port, e.type, e.offset, e.port, e.flags, static_cast<uint32_t>(e.note_id),
                           static_cast<uint32_t>(e.channel), static_cast<uint32_t>(e.key), e.source_id, e.index,
                           e.expression, e.message_kind, e.bytes, e.ump_words})
        u32(bytes, value);
    f32(bytes, e.value);
    f32(bytes, e.value2);
    for (uint32_t i = 0; i < e.ump_words; ++i) u32(bytes, e.ump[i]);
    bytes.insert(bytes.end(), out.payload, out.payload + e.bytes);
}
template <class Render> static Bytes signal_bytes(Render render) {
    Bytes bytes;
    for (uint32_t offset = 0; offset < 48000; offset += 128) {
        float samples[128]{};
        const auto count = std::min(128u, 48000u - offset);
        require(render(samples, count) == WS_OK, "offline render failed");
        for (uint32_t i = 0; i < count; ++i) f32(bytes, samples[i]);
    }
    return bytes;
}
static Bytes event_stream(wirescape::engine2::Engine &engine) {
    Bytes bytes;
    for (uint32_t offset = 0; offset < 48000; offset += 128) {
        float samples[128]{};
        float *rows[]{samples};
        require(engine.render(rows, 1, std::min(128u, 48000u - offset)) == WS2_OK, "event-only default render failed");
        wirescape::engine2::Outgoing out;
        while (engine.pop_egress(out)) event_bytes(bytes, out);
    }
    return bytes;
}
static void serialization_self_test() {
    Bytes samples, event;
    for (float value : {0.f, -0.f, -.5f, 1.f}) f32(samples, value);
    for (float value : {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        bool rejected = false;
        try {
            f32(samples, value);
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        require(rejected, "nonfinite golden sample accepted");
    }
    wirescape::engine2::Outgoing out{};
    out.frame = UINT64_C(0x0123456789abcdef);
    out.node = 3;
    out.port = 7;
    auto &e = out.event;
    e.type = 11;
    e.offset = 13;
    e.port = 17;
    e.flags = 19;
    e.note_id = -1;
    e.channel = -2;
    e.key = -3;
    e.source_id = 23;
    e.index = 29;
    e.expression = 31;
    e.message_kind = 37;
    e.bytes = 4;
    e.ump_words = 4;
    e.value = -.5f;
    e.value2 = .25f;
    e.ump[0] = 41;
    e.ump[1] = 43;
    e.ump[2] = 47;
    e.ump[3] = 53;
    out.payload[0] = 0;
    out.payload[1] = 127;
    out.payload[2] = 128;
    out.payload[3] = 255;
    event_bytes(event, out);
    std::cout << Json{{"samples", hex(samples)}, {"event", hex(event)}}.dump() << '\n';
}
struct GoldenEvents {
    static constexpr ws::Info info{"test.golden_events", "Golden events", 1};
    static constexpr std::array<ws2_port, 0> inputs{};
    static constexpr std::array outputs{ws::port("events", "Events", WS2_EV_MIDI, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array params{ws::param("value", "Value", 0, 1, .25f)};
    void prepare(const ws::Setup &) {}
    void reset() {}
    void process(ws::Block &b) {
        b.out(0).set_channels(0);
        ws2_event event{};
        event.struct_size = sizeof(event);
        event.type = WS2_MIDI;
        event.source_id = 37;
        event.value = b.param(0).value(0, 0);
        event.ump_words = 1;
        event.ump[0] = 0x20b00140;
        b.emit(0, event);
    }
};
static void event_self_test() {
    auto render = [](float value) {
        wirescape::engine2::Config config;
        config.sample_rate = 48000;
        config.max_block = 128;
        config.max_channels = 1;
        config.seed = 42;
        wirescape::engine2::Engine engine(config);
        require(engine.add_node(1, ws::Module<GoldenEvents>::descriptor()) && engine.prepare(),
                "event test graph prepare failed");
        require(engine.set_parameter(1, 0, value), "event test parameter failed");
        const auto digest = event_stream(engine);
        require(engine.meter().faults == 0, "event test fault");
        return digest;
    };
    const auto first = render(.25f);
    require(first == render(.25f), "fresh event hashes differ");
    require(first != render(.75f), "event hash ignored a changed event value");
    std::cout << Json{{"first_bytes", hex(first)},
                      {"second_bytes", hex(render(.25f))},
                      {"changed_bytes", hex(render(.75f))}}
                     .dump()
              << '\n';
}
int main(int argc, char **argv) {
    try {
        require(argc == 2, "examples directory required");
        if (std::strcmp(argv[1], "--serialization-self-test") == 0) {
            serialization_self_test();
            return 0;
        }
        if (std::strcmp(argv[1], "--event-self-test") == 0) {
            event_self_test();
            return 0;
        }
        Json cases = Json::array();
        auto record = [&](const std::string &name, auto render, const char *stream = "signal") {
            const auto first = render(), second = render();
            cases.push_back(
                {{"name", name}, {"first_bytes", hex(first)}, {"second_bytes", hex(second)}, {"stream", stream}});
            require(first == second, "same-build fresh renders differ");
        };
        ws_engine_config cfg{sizeof(cfg), 48000, 128, 128, 2048};
        App catalog(app_engine_create(&cfg), app_engine_destroy);
        require(bool(catalog), "catalog creation failed");
        std::vector<std::filesystem::path> examples;
        for (const auto &entry : std::filesystem::directory_iterator(argv[1]))
            if (entry.is_regular_file() && entry.path().extension() == ".json") examples.push_back(entry.path());
        std::sort(examples.begin(), examples.end());
        require(!examples.empty(), "no example patches discovered");
        for (const auto &path : examples) {
            std::ifstream file(path, std::ios::binary);
            require(bool(file), "example read failed");
            const std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            // Validate the original lexical stream before JSON projection can
            // collapse duplicate keys; the production runtime owns this refusal.
            std::string validation_error;
            App validated(load_runtime_patch(source, catalog.get(), cfg, validation_error), app_engine_destroy);
            require(bool(validated), validation_error.c_str());
            auto document = Json::parse(source);
            // Fixture-only execution projection of the old Qt v1 migration. This
            // does not edit/write documents or introduce another document owner.
            if (document.at("version") == 1) {
                for (auto &module : document.at("modules")) {
                    const auto type = module.at("type").get<std::string>();
                    Json parameters = Json::object();
                    for (uint32_t i = 0; i < app_engine_module_count(catalog.get()); ++i) {
                        const auto *d = app_engine_module(catalog.get(), i);
                        if (type != d->id) continue;
                        for (uint32_t p = 0; p < d->parameter_count; ++p) {
                            const auto key = std::to_string(d->parameters[p].id);
                            if (module.value("values", Json::object()).contains(key))
                                parameters[app_engine_parameter_id(catalog.get(), d->id, p)] = module["values"][key];
                        }
                        if (module.at("id") == document.at("outputNode"))
                            document["outputs"] = Json::array(
                                {{{"module", module.at("id")}, {"port", d->outputs[0].id}, {"bus_channel", 0}}});
                    }
                    module.erase("state");
                    module.erase("inactive");
                    module.erase("channel_seed");
                    module["module"] = type;
                    module["version"] = 1;
                    module["parameters"] = parameters;
                    module["config"] = Json::object();
                }
                for (auto &c : document.at("connections")) {
                    if (c["kind"] == "audio" || c["kind"] == "control") {
                        c["kind"] = "signal";
                        c.erase("amount");
                        c.erase("feedback");
                        c.erase("stereo_pair");
                    }
                    c["from"] = {{"module", c.at("fromNode")}, {"port", c.at("fromPort")}};
                    c["to"] = {{"module", c.at("toNode")}, {"port", c.at("toPort")}};
                }
                document["version"] = 2;
                document["settings"] = {
                    {"sample_rate", 48000}, {"max_block_frames", 256}, {"max_channels", 16}, {"feedback_block", 1}};
            }
            document["settings"]["seed"] = 42u;
            record("patch/" + path.filename().string(), [&]() {
                std::string error;
                App graph(load_runtime_patch(document.dump(), catalog.get(), cfg, error), app_engine_destroy);
                require(bool(graph), error.c_str());
                return signal_bytes(
                    [&](float *out, uint32_t count) { return app_engine_render(graph.get(), out, count); });
            });
        }
        auto descriptors = wirescape::app::builtin_modules();
        for (auto *descriptor : descriptors) {
            uint32_t signal = 0;
            while (signal < descriptor->output_count && descriptor->outputs[signal].kind != WS2_SIGNAL) ++signal;
            const bool has_signal = signal < descriptor->output_count;
            record(
                "module/" + std::string(descriptor->id),
                [&]() {
                    wirescape::engine2::Config config;
                    config.sample_rate = 48000;
                    config.max_block = 128;
                    config.max_channels = 1;
                    config.seed = 42;
                    wirescape::engine2::Engine engine(config);
                    engine.register_producer();
                    require(engine.add_node(1, descriptor,
                                            runtime_module_config(descriptor->id, nlohmann::json::object(), 1)) &&
                                (!has_signal || engine.output(1, signal, 0)) && engine.prepare(),
                            "default module graph failed");
                    const auto digest = has_signal ? signal_bytes([&](float *out, uint32_t count) {
                        float *rows[]{out};
                        return engine.render(rows, 1, count);
                    })
                                                   : event_stream(engine);
                    require(engine.meter().faults == 0, "fault in default module render");
                    return digest;
                },
                has_signal ? "signal" : "events");
        }
        std::cout
            << Json{{"sample_rate", 48000}, {"frames", 48000}, {"block", 128}, {"seed", 42}, {"cases", cases}}.dump()
            << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
