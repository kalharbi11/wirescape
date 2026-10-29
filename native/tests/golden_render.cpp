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
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        require(argc == 2, "examples directory required");
        QJsonArray cases;
        auto record = [&](const QString &name, auto render) {
            const auto first = render(), second = render();
            cases.append(QJsonObject{{"name", name}, {"first", first}, {"second", second}});
            require(first == second, "same-build fresh renders differ");
        };
        ws_engine_config cfg{sizeof(cfg), 48000, 128, 128, 2048};
        App catalog(app_engine_create(&cfg, 42), app_engine_destroy);
        require(bool(catalog), "catalog creation failed");
        QDir examples(QString::fromLocal8Bit(argv[1]));
        const auto names = examples.entryList({"*.json"}, QDir::Files, QDir::Name);
        require(!names.empty(), "no example patches discovered");
        for (const auto &name : names) {
            QFile file(examples.filePath(name));
            require(file.open(QIODevice::ReadOnly), "example read failed");
            const auto document = read_patch(file.readAll(), app_catalog(catalog.get()));
            require(document.valid(), "example validation failed");
            record("patch/" + name, [&]() {
                QString error;
                App graph(build_app_patch(catalog.get(), document, cfg, error, 42), app_engine_destroy);
                require(bool(graph), "example graph build failed");
                return hash([&](float *out, uint32_t count) { return app_engine_render(graph.get(), out, count); });
            });
        }
        for (auto *descriptor : wirescape::app::builtin_modules()) {
            record("module/" + QString::fromUtf8(descriptor->id), [&]() {
                wirescape::engine2::Config config;
                config.sample_rate = 48000;
                config.max_block = 128;
                config.max_channels = 1;
                config.seed = 42;
                wirescape::engine2::Engine engine(config);
                engine.register_producer();
                require(engine.add_node(1, descriptor) && engine.output(1, 0, 0) && engine.prepare(),
                        "default module graph failed");
                return hash([&](float *out, uint32_t count) {
                    float *rows[]{out};
                    return engine.render(rows, 1, count);
                });
            });
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
