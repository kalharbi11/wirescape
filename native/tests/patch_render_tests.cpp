// SPDX-License-Identifier: GPL-3.0-only
#include "patch_document.h"
#include "module_loader.h"
#include "wirescape/engine.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
using namespace wirescape::document;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void check_save(bool ok, const QString& error, const char* message) {
    if (!ok) throw std::runtime_error(std::string(message) + ": " + error.toStdString());
}
static QByteArray read(const QString& path) { QFile f(path); check(f.open(QIODevice::ReadOnly), "read fixture/manifest"); return f.readAll(); }
static uint32_t port(const ws_port_descriptor* ports, uint32_t count, const QString& id) {
    for (uint32_t i = 0; i < count; ++i) if (id == QString::fromUtf8(ports[i].id)) return i;
    throw std::runtime_error("Missing descriptor port ID");
}
static QByteArray render(const QJsonObject& patch, bool v2, const ModuleCatalog& catalog,
                         const QHash<QString, const ws_module_descriptor*>& descriptors) {
    ws_engine_config config{sizeof(config), 48000, 256, 32, 128};
    std::unique_ptr<ws_engine, decltype(&ws_engine_destroy)> engine(ws_engine_create(&config), ws_engine_destroy);
    check(bool(engine), "create render engine");
    for (auto d : descriptors) check(ws_engine_register_module(engine.get(), d) == WS_OK, "register ABI1 descriptor");
    QHash<uint32_t, const ws_module_descriptor*> nodes;
    for (const auto& item : patch["modules"].toArray()) {
        const auto m = item.toObject(); const auto id = uint32_t(m["id"].toInt());
        const auto type = m[v2 ? "module" : "type"].toString();
        check(descriptors.contains(type), "demo module known"); nodes.insert(id, descriptors[type]);
        check(ws_engine_add_node(engine.get(), id, type.toUtf8().constData()) == WS_OK, "add demo node");
    }
    for (const auto& item : patch["connections"].toArray()) {
        const auto c = item.toObject();
        const auto from = v2 ? c["from"].toObject() : QJsonObject{{"module", c["fromNode"]}, {"port", c["fromPort"]}};
        const auto to = v2 ? c["to"].toObject() : QJsonObject{{"module", c["toNode"]}, {"port", c["toPort"]}};
        const auto source = uint32_t(from["module"].toInt()), destination = uint32_t(to["module"].toInt());
        check(nodes.contains(source) && nodes.contains(destination), "demo cable endpoint");
        check(ws_engine_connect(engine.get(), source, port(nodes[source]->outputs, nodes[source]->output_count, from["port"].toString()),
              destination, port(nodes[destination]->inputs, nodes[destination]->input_count, to["port"].toString())) == WS_OK, "connect demo");
    }
    const auto output = v2 ? patch["outputs"].toArray()[0].toObject() : QJsonObject{{"module", patch["outputNode"]}};
    const auto outputNode = uint32_t(output["module"].toInt());
    const auto outputPort = v2 ? port(nodes[outputNode]->outputs, nodes[outputNode]->output_count, output["port"].toString()) : 0;
    check(ws_engine_prepare(engine.get(), outputNode, outputPort) == WS_OK, "prepare demo");
    for (const auto& item : patch["modules"].toArray()) {
        const auto m = item.toObject(); const auto type = m[v2 ? "module" : "type"].toString();
        const auto values = m[v2 ? "parameters" : "values"].toObject();
        for (auto p = values.begin(); p != values.end(); ++p) {
            bool ok = false; const auto legacy = v2 ? catalog[type].legacy_parameters.key(p.key()) : p.key();
            const auto id = legacy.toUInt(&ok); check(ok, "stable ID maps back to descriptor ID");
            check(ws_engine_initialize_parameter(engine.get(), uint32_t(m["id"].toInt()), id, float(p.value().toDouble())) == WS_OK, "initialize plain parameter");
        }
    }
    check(ws_engine_activate(engine.get()) == WS_OK, "activate offline engine");
    std::vector<float> samples(96000);
    for (uint32_t offset = 0; offset < samples.size(); offset += 256) {
        const auto frames = std::min<uint32_t>(256, uint32_t(samples.size()) - offset);
        check(ws_engine_render(engine.get(), samples.data() + offset, frames) == WS_OK, "offline render");
    }
    float peak = 0; for (float sample : samples) { check(std::isfinite(sample), "finite WAV"); peak = std::max(peak, std::abs(sample)); }
    check(peak > .001f, "non-silent demo render");
    // A complete mono IEEE-float WAV, with explicit little-endian encoding.
    QByteArray wav; auto u32 = [&](uint32_t n) { n = qToLittleEndian(n); wav.append(reinterpret_cast<const char*>(&n), 4); };
    auto u16 = [&](uint16_t n) { n = qToLittleEndian(n); wav.append(reinterpret_cast<const char*>(&n), 2); };
    const auto bytes = uint32_t(samples.size() * sizeof(float));
    wav.append("RIFF", 4); u32(36 + bytes); wav.append("WAVEfmt ", 8); u32(16); u16(3); u16(1);
    u32(48000); u32(48000 * 4); u16(4); u16(32); wav.append("data", 4); u32(bytes);
    for (float sample : samples) { uint32_t bits; std::memcpy(&bits, &sample, 4); u32(bits); }
    return wav;
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        check(argc == 4, "module directory, source modules, demo arguments");
        ModuleLoader loader(argv[1]); ModuleCatalog catalog;
        QHash<QString, const ws_module_descriptor*> descriptors;
        const auto originalBytes = read(QString::fromLocal8Bit(argv[3]));
        const auto original = QJsonDocument::fromJson(originalBytes).object();
        for (const auto& item : original["modules"].toArray()) {
            const auto type = item.toObject()["type"].toString(); const auto name = type.mid(QString("essentials.").size());
            const auto manifest = QJsonDocument::fromJson(read(QDir(QString::fromLocal8Bit(argv[2])).filePath(name + "/module.json"))).object();
            auto* d = loader.abi1(name.toStdString()); check(d->output_count > 0, "demo descriptor output");
            QString error; check(add_manifest(catalog, manifest, QString::fromUtf8(d->outputs[0].id), &error), "demo catalog");
            descriptors.insert(type, d);
        }
        const auto migrated = read_patch(originalBytes, catalog); check(migrated.valid(), "demo migration");
        QTemporaryDir dir; check(dir.isValid(), "temporary directory"); const auto path = dir.filePath("demo.json"); QString error;
        check_save(save_patch(path, migrated, &error), error, "save migrated demo");
        const auto reloaded = read_patch(read(path), catalog); check(reloaded.valid() && reloaded.source_version == 2, "reload v2 demo");
        const auto before = render(original, false, catalog, descriptors), after = render(reloaded.object, true, catalog, descriptors);
        check(before == after, "v1 vs saved/reloaded v2 WAV bytes bit-identical");
        const auto a = QCryptographicHash::hash(before, QCryptographicHash::Sha256).toHex();
        const auto b = QCryptographicHash::hash(after, QCryptographicHash::Sha256).toHex();
        check(a == b, "WAV hashes identical");
        std::cout << "PASS Six_Essentials_Demo v1 -> v2 -> reload: " << a.constData() << " (both WAVs)\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
