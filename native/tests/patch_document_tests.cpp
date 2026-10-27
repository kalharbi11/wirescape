// SPDX-License-Identifier: GPL-3.0-only
#include "patch_document.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
using namespace wirescape::document;
static int checks = 0;
static void check(bool ok, const char* message) { ++checks; if (!ok) throw std::runtime_error(message); }
static QByteArray encode(const QJsonObject& o) { return QJsonDocument(o).toJson(QJsonDocument::Compact); }
static QByteArray read(const QString& path) { QFile f(path); check(f.open(QIODevice::ReadOnly), "open fixture"); return f.readAll(); }
static void write(const QString& path, const QByteArray& bytes) { QFile f(path); check(f.open(QIODevice::WriteOnly), "open output"); check(f.write(bytes) == bytes.size(), "write output"); }
static QJsonObject v1() {
    return {{"format", "wirescape.native.patch"}, {"version", 1}, {"name", "test"}, {"outputNode", 1},
        {"modules", QJsonArray{QJsonObject{{"id", 1}, {"type", "essentials.vco"}, {"values", QJsonObject{{"0", 220}, {"999", QJsonObject{{"opaque", true}}}}},
        {"vendor", QJsonArray{1, "nested"}}, {"preserved", "original collision"}}}}, {"connections", QJsonArray{}}, {"vendor", QJsonObject{{"retain", true}}}};
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        check(argc == 2, "fixture path"); ModuleCatalog catalog; QString error;
        QJsonObject manifest{{"id", "essentials.vco"}, {"parameters", QJsonObject{{"frequency", 0}}}};
        check(add_manifest(catalog, manifest, "audio", &error), "catalog mapping");
        auto before = catalog; check(!add_manifest(catalog, manifest, "audio", &error) && catalog.size() == before.size(), "duplicate catalog ID atomic");
        manifest["id"] = "invalid"; manifest["parameters"] = QJsonObject{{"x", 0}, {"y", 0}};
        check(!add_manifest(catalog, manifest, "audio", &error) && !catalog.contains("invalid"), "duplicate numeric mapping rejected");
        manifest["parameters"] = QJsonObject{{"x", 1.5}}; check(!add_manifest(catalog, manifest, "audio", &error), "fractional mapping rejected");
        const auto original = v1(); const auto raw = QByteArray(" \n") + encode(original) + '\n';
        const auto patch = read_patch(raw, catalog); check(patch.valid() && patch.source_version == 1, "v1 migrated");
        check(patch.object["version"] == 2 && patch.original_bytes == raw, "version and original bytes");
        const auto module = patch.object["modules"].toArray()[0].toObject();
        check(module["parameters"].toObject() == QJsonObject{{"frequency", 220}}, "stable parameter ID");
        check(module["preserved"].toObject() == original["modules"].toArray()[0].toObject(), "whole module including unmapped keys and collisions retained");
        check(patch.object["preserved"].toObject() == original, "entire v1 retained without namespace collisions");
        check(module["vendor"] == original["modules"].toArray()[0].toObject()["vendor"] && patch.object["vendor"] == original["vendor"], "unknown fields retained in place");
        check(!module.contains("state"), "no invented DSP state");
        check(read_patch(encode(patch.object), catalog).object == patch.object, "v2 roundtrip value-identical");
        auto unknown = original; auto modules = unknown["modules"].toArray(); auto item = modules[0].toObject(); item["type"] = "vendor.unknown"; modules[0] = item; unknown["modules"] = modules;
        unknown["connections"] = QJsonArray{QJsonObject{{"kind", "control"}, {"fromNode", 1}, {"fromPort", "unknown-out"}, {"toNode", 1}, {"toPort", "unknown-in"}, {"vendor", QJsonArray{true, 3}}}};
        auto u = read_patch(encode(unknown), catalog); check(u.valid(), "unknown module/cable accepted inactive");
        const auto um = u.object["modules"].toArray()[0].toObject(); check(um["inactive"] == true && um["preserved"] == item, "whole unknown module verbatim");
        check(u.object["connections"].toArray().size() == 1 && u.object["connections"].toArray()[0].toObject()["vendor"] == unknown["connections"].toArray()[0].toObject()["vendor"], "unknown cable retained");
        check(read_patch(encode(u.object), catalog).object == u.object, "unknown v1-v2-v2 roundtrip");
        const QByteArray duplicates[] = {"{\"format\":1,\"format\":2}", "{\"x\":{\"a\":1,\"\\u0061\":2}}", "{\"x\":[{\"z\":1,\"z\":2}]}"};
        for (const auto& bytes : duplicates) { auto bad = read_patch(bytes, catalog); check(!bad.valid() && bad.error.contains("Duplicate"), "duplicate raw/escaped/nested keys rejected"); }
        auto decorated = patch.object;
        auto settings = decorated["settings"].toObject(); settings["vendor"] = QJsonArray{QJsonObject{{"same", 1}}, QJsonObject{{"same", 2}}}; decorated["settings"] = settings;
        auto decoratedModules = decorated["modules"].toArray(); auto decoratedModule = decoratedModules[0].toObject();
        decoratedModule["config"] = QJsonObject{{"unknown", QJsonArray{1, "value"}}};
        decoratedModule["state"] = QJsonObject{{"version", 4}, {"encoding", "base64"}, {"data", "AQI="}, {"vendor", "{\\\"quoted\\\":value}"}};
        decoratedModules[0] = decoratedModule; decorated["modules"] = decoratedModules;
        decorated["connections"] = QJsonArray{QJsonObject{{"kind", "signal"},
            {"from", QJsonObject{{"module", 1}, {"port", "audio"}, {"vendor", true}}},
            {"to", QJsonObject{{"module", 1}, {"port", "unknown"}, {"vendor", QJsonArray{3}}}}, {"vendor", "keep"}}};
        const auto decoratedBytes = encode(decorated);
        check(read_patch(decoratedBytes, catalog).object == decorated, "v2 unknown settings/config/state/endpoints/cable fields retained");
        auto escapedBytes = decoratedBytes; escapedBytes.replace("\"vendor\"", "\"\\u0076endor\"");
        check(read_patch(escapedBytes, catalog).object == decorated, "escaped keys and string punctuation accepted; sibling keys independent");
        check(!read_patch("[]", catalog).valid() && !read_patch("{bad", catalog).valid(), "malformed JSON rejected");
        auto future = original; future["version"] = 99; check(!read_patch(encode(future), catalog).valid(), "future native rejected");
        for (const auto& name : {"patch_v1.json", "patch_v2.json", "patch_v3_unknown.json", "visual_study_v4.json", "future_patch_v99.json", "malformed_patch.json"})
            check(!read_patch(read(QString::fromLocal8Bit(argv[1]) + '/' + name), catalog).valid(), "legacy inspection remains separate from native loading");
        auto bad = patch.object;
        auto rejected = [&](const QJsonObject& o) { check(!validate_patch(o, &error) && !error.isEmpty(), "validator negative with diagnostic"); };
        modules = bad["modules"].toArray(); modules.append(modules[0]); bad["modules"] = modules; rejected(bad);
        bad = patch.object; bad["connections"] = QJsonArray{QJsonObject{{"kind", "signal"}, {"from", QJsonObject{{"module", 99}, {"port", "audio"}}}, {"to", QJsonObject{{"module", 1}, {"port", "audio"}}}}}; rejected(bad);
        bad = patch.object; bad["settings"] = QJsonObject{{"max_block_frames", 16}, {"feedback_block", 17}}; rejected(bad);
        bad = patch.object; bad["settings"] = QJsonObject{{"max_channels", 17}}; rejected(bad);
        bad = patch.object; bad["outputs"] = QJsonArray{QJsonObject{{"module", 1}, {"port", "audio"}, {"bus_channel", -1}}}; rejected(bad);
        bad = patch.object; modules = bad["modules"].toArray(); item = modules[0].toObject(); item["parameters"] = QJsonObject{{"frequency", "invalid"}}; modules[0] = item; bad["modules"] = modules; rejected(bad);
        QTemporaryDir dir; check(dir.isValid(), "temporary directory"); const auto path = dir.filePath("patch.json"); const auto backup = dir.filePath("patch.v1.json");
        write(path, raw); check(save_patch(path, patch, &error), "atomic v2 save");
        check(read(backup) == raw && read_patch(read(path), catalog).object == patch.object, "exact first backup and serialized v2");
        write(backup, "existing backup"); check(save_patch(path, patch, &error) && read(backup) == "existing backup", "backup never overwritten");
        auto invalid = patch; invalid.object["version"] = 99; const auto saved = read(path);
        check(!save_patch(path, invalid, &error) && read(path) == saved, "invalid save cannot alter target");
        for (const auto& bytes : {encode(future), QByteArray("{broken"), duplicates[0]}) {
            write(path, bytes); check(!save_patch(path, patch, &error) && read(path) == bytes, "refuse overwrite future/malformed/duplicate file");
        }
        const auto failed = read_patch("{broken", catalog); check(!save_patch(path, failed, &error), "failed load cannot save");
        const auto newPath = dir.filePath("new.json"); check(save_patch(newPath, patch, &error) && read(dir.filePath("new.v1.json")) == raw, "save-as backs up original v1");
        const auto reloaded = read_patch(read(newPath), catalog); check(reloaded.valid() && save_patch(dir.filePath("v2.json"), reloaded, &error) && !QFile::exists(dir.filePath("v2.v1.json")), "v2 save creates no v1 backup");
        check(!read_patch(QByteArray(MaximumDocumentBytes + 1, ' '), catalog).valid(), "bounded reader");
        std::cout << "PASS patch v2: " << checks << " checks\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
