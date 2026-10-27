#include "document_reader.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <iostream>
#include <stdexcept>

using namespace wirescape::document;
static int checks = 0;
static void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
static QByteArray read(const QString& path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "fixture readable");
    return file.readAll();
}
static QByteArray encode(const QJsonObject& object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
static void preserved(const QByteArray& bytes, Format expected) {
    const auto result = inspect(bytes);
    check(result.format == expected, "format classification");
    check(result.originalBytes == bytes, "exact original bytes retained");
    const auto parsed = QJsonDocument::fromJson(bytes);
    if (parsed.isObject()) check(result.object == parsed.object(), "opaque object retained");
    check(!result.diagnostic.isEmpty(), "diagnostic supplied");
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        check(argc == 2, "fixture directory argument");
        const QString dir = QString::fromLocal8Bit(argv[1]) + '/';
        const struct { const char* file; Format format; } fixtures[] = {
            {"patch_v1.json", Format::LegacyPatchV1},
            {"patch_v2.json", Format::LegacyPatchV2},
            {"patch_v3_unknown.json", Format::LegacyPatchV3},
            {"visual_study_v4.json", Format::VisualStudyV4},
            {"future_patch_v99.json", Format::UnsupportedVersion},
            {"malformed_patch.json", Format::Invalid}
        };
        for (const auto& fixture : fixtures) preserved(read(dir + fixture.file), fixture.format);
        const auto v2 = inspect(read(dir + "patch_v2.json"));
        check(v2.recognizedStructure(), "legacy structure recognized");
        check(v2.legacyStateFields.size() == 1, "sequence field inventoried");
        check(v2.legacyStateFields[0].sequence.isObject() && v2.legacyStateFields[0].moduleState.isUndefined(), "sequence not translated");
        auto legacy = v2.object;
        auto modules = legacy["modules"].toArray();
        auto seq = modules[0].toObject();
        seq["module_state"] = QJsonObject{{"different", true}};
        modules[0] = seq; legacy["modules"] = modules;
        auto both = inspect(encode(legacy));
        check(both.legacyStateFields.size() == 1 && both.legacyStateFields[0].sequence != both.legacyStateFields[0].moduleState, "conflicting state fields retained separately");
        seq["module_state"] = QJsonValue::Null; modules[0] = seq; legacy["modules"] = modules;
        check(inspect(encode(legacy)).legacyStateFields[0].moduleState.isNull(), "explicit state null retained");

        QJsonObject native{{"format", "wirescape.native.patch"}, {"version", 1}, {"name", "opaque fixture"}, {"outputNode", 0},
            {"modules", QJsonArray{QJsonObject{{"id", 1}, {"type", "vendor.unknown"}, {"values", QJsonObject{{"opaque", "data"}}}, {"x", 1}, {"y", 2}}}},
            {"connections", QJsonArray{}}, {"vendor", QJsonObject{{"retain", true}}}};
        const auto original = QByteArray(" \n") + encode(native) + "\n";
        preserved(original, Format::NativeV1);
        check(inspect(original).recognizedStructure(), "native structure recognized without descriptor loading");
        auto invalid = native;
        invalid["modules"] = QJsonObject{}; preserved(encode(invalid), Format::Invalid);
        invalid = native; invalid["name"] = 4; preserved(encode(invalid), Format::Invalid);
        invalid = native; invalid["outputNode"] = "0"; preserved(encode(invalid), Format::Invalid);
        invalid = native; invalid["version"] = true; preserved(encode(invalid), Format::Invalid);
        invalid = native; invalid["version"] = 1.5; preserved(encode(invalid), Format::Invalid);
        invalid = native; invalid["version"] = 0; preserved(encode(invalid), Format::Invalid);
        invalid = native; invalid["format"] = 1; preserved(encode(invalid), Format::Invalid);
        for (bool isNative : {false, true}) {
            invalid = isNative ? native : v2.object;
            modules = invalid["modules"].toArray(); modules.append(modules[0]); invalid["modules"] = modules;
            preserved(encode(invalid), Format::Invalid);
            check(!inspect(encode(invalid)).recognizedStructure(), "duplicate IDs not recognized");
            invalid = isNative ? native : v2.object;
            modules = invalid["modules"].toArray(); auto module = modules[0].toObject();
            module["id"] = isNative ? QJsonValue("1") : QJsonValue(1);
            modules[0] = module; invalid["modules"] = modules;
            preserved(encode(invalid), Format::Invalid);
            invalid = isNative ? native : v2.object;
            invalid["connections"] = QJsonArray{QJsonObject{{"from", "missing-colon"}, {"to", 3}}};
            preserved(encode(invalid), Format::Invalid);
        }
        invalid = native; invalid["modules"] = QJsonArray{4}; preserved(encode(invalid), Format::Invalid);
        invalid = native; invalid["connections"] = QJsonArray{false}; preserved(encode(invalid), Format::Invalid);
        auto future = native; future["version"] = 2; future["modules"] = "future schema";
        preserved(encode(future), Format::UnsupportedVersion);
        check(!inspect(encode(future)).recognizedStructure(), "future native version never recognized as executable");
        future["format"] = "vendor.document"; preserved(encode(future), Format::UnknownFormat);
        future["format"] = "wirescape.visual.study"; future["version"] = 5;
        preserved(encode(future), Format::UnsupportedVersion);
        preserved("{broken", Format::Invalid);
        preserved("[]", Format::Invalid);
        preserved("{} trailing", Format::Invalid);
        preserved("", Format::Invalid);
        const QByteArray oversized(MaximumDocumentBytes + 1, ' ');
        preserved(oversized, Format::Invalid);
        check(inspect(oversized).diagnostic.contains("2 MiB"), "oversize rejected before JSON parsing");
        const QByteArray atLimit = encode(native) + QByteArray(MaximumDocumentBytes - encode(native).size(), ' ');
        preserved(atLimit, Format::NativeV1);
        // Returned objects use Qt copy-on-write; staging edits cannot change the
        // earlier inspection result or its original byte representation.
        auto staged = inspect(original); auto edited = staged.object;
        edited["vendor"] = false;
        check(staged.object["vendor"].isObject() && staged.originalBytes == original, "staging copy isolated");
        std::cout << "PASS document inspection: " << checks << " checks; no GUI, module or audio activation\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
