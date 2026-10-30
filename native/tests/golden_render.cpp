// SPDX-License-Identifier: GPL-3.0-only
#include "app_patch.h"
#include "module_catalog.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace wirescape::document;
using App = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
static void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
template <class Render> static QString hash(Render render) {
    QByteArray bytes;
    for (uint32_t offset = 0; offset < 48000; offset += 128) {
        float samples[128]{};
        const auto count = std::min(128u, 48000u - offset);
        require(render(samples, count) == WS_OK, "offline render failed");
        for (uint32_t i = 0; i < count; ++i) {
            require(std::isfinite(samples[i]), "nonfinite golden sample");
            uint32_t bits;
            std::memcpy(&bits, &samples[i], sizeof(bits));
            bits = qToLittleEndian(bits);
            bytes.append(reinterpret_cast<const char *>(&bits), sizeof(bits));
        }
    }
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}
static QString event_hash(wirescape::engine2::Engine &engine) {
    QByteArray bytes;
    auto u32 = [&](uint32_t value) {
        value = qToLittleEndian(value);
        bytes.append(reinterpret_cast<const char *>(&value), sizeof(value));
    };
    auto f32 = [&](float value) {
        require(std::isfinite(value), "nonfinite golden event value");
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    };
    for (uint32_t offset = 0; offset < 48000; offset += 128) {
        float samples[128]{};
        float *rows[]{samples};
        require(engine.render(rows, 1, std::min(128u, 48000u - offset)) == WS2_OK, "event-only default render failed");
        wirescape::engine2::Outgoing out;
        while (engine.pop_egress(out)) {
            const auto &e = out.event;
            require(e.bytes <= WS2_MAX_PAYLOAD && e.ump_words <= 4, "invalid golden event payload");
            u32(static_cast<uint32_t>(out.frame));
            u32(static_cast<uint32_t>(out.frame >> 32));
            for (uint32_t value :
                 {out.node, out.port, e.type, e.offset, e.port, e.flags, static_cast<uint32_t>(e.note_id),
                  static_cast<uint32_t>(e.channel), static_cast<uint32_t>(e.key), e.source_id, e.index, e.expression,
                  e.message_kind, e.bytes, e.ump_words})
                u32(value);
            f32(e.value);
            f32(e.value2);
            for (uint32_t i = 0; i < e.ump_words; ++i) u32(e.ump[i]);
            bytes.append(reinterpret_cast<const char *>(out.payload), e.bytes);
        }
    }
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
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
        const auto digest = event_hash(engine);
        require(engine.meter().faults == 0, "event test fault");
        return digest;
    };
    const auto first = render(.25f);
    require(first == render(.25f), "fresh event hashes differ");
    require(first != render(.75f), "event hash ignored a changed event value");
    std::cout << "PASS canonical event hashes: fresh instances match; changed event value differs\n";
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        require(argc == 2, "examples directory required");
        if (std::strcmp(argv[1], "--event-self-test") == 0) {
            event_self_test();
            return 0;
        }
        QJsonArray cases;
        auto record = [&](const QString &name, auto render, const char *stream = "signal") {
            const auto first = render(), second = render();
            cases.append(QJsonObject{{"name", name}, {"first", first}, {"second", second}, {"stream", stream}});
            require(first == second, "same-build fresh renders differ");
        };
        ws_engine_config cfg{sizeof(cfg), 48000, 128, 128, 2048};
        App catalog(app_engine_create(&cfg), app_engine_destroy);
        require(bool(catalog), "catalog creation failed");
        QDir examples(QString::fromLocal8Bit(argv[1]));
        const auto names = examples.entryList({"*.json"}, QDir::Files, QDir::Name);
        require(!names.empty(), "no example patches discovered");
        for (const auto &name : names) {
            QFile file(examples.filePath(name));
            require(file.open(QIODevice::ReadOnly), "example read failed");
            auto document = read_patch(file.readAll(), app_catalog(catalog.get()));
            require(document.valid(), "example validation failed");
            auto settings = document.object["settings"].toObject();
            settings["seed"] = 42;
            document.object["settings"] = settings;
            record("patch/" + name, [&]() {
                QString error;
                App graph(build_app_patch(catalog.get(), document, cfg, error), app_engine_destroy);
                require(bool(graph), "example graph build failed");
                return hash([&](float *out, uint32_t count) { return app_engine_render(graph.get(), out, count); });
            });
        }
        auto descriptors = wirescape::app::builtin_modules();
        for (auto *descriptor : descriptors) {
            uint32_t signal = 0;
            while (signal < descriptor->output_count && descriptor->outputs[signal].kind != WS2_SIGNAL) ++signal;
            const bool has_signal = signal < descriptor->output_count;
            record(
                "module/" + QString::fromUtf8(descriptor->id),
                [&]() {
                    wirescape::engine2::Config config;
                    config.sample_rate = 48000;
                    config.max_block = 128;
                    config.max_channels = 1;
                    config.seed = 42;
                    wirescape::engine2::Engine engine(config);
                    engine.register_producer();
                    require(engine.add_node(1, descriptor) && (!has_signal || engine.output(1, signal, 0)) &&
                                engine.prepare(),
                            "default module graph failed");
                    const auto digest = has_signal ? hash([&](float *out, uint32_t count) {
                        float *rows[]{out};
                        return engine.render(rows, 1, count);
                    })
                                                   : event_hash(engine);
                    require(engine.meter().faults == 0, "fault in default module render");
                    return digest;
                },
                has_signal ? "signal" : "events");
        }
        std::cout << QJsonDocument(
                         QJsonObject{
                             {"sample_rate", 48000}, {"frames", 48000}, {"block", 128}, {"seed", 42}, {"cases", cases}})
                         .toJson(QJsonDocument::Compact)
                         .constData()
                  << '\n';
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
