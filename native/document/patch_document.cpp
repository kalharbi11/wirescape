// SPDX-License-Identifier: GPL-3.0-only
#include "patch_document.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <cmath>
#include <cstdint>
#include <limits>

namespace wirescape::document {
namespace {
bool fail(QString* error, const QString& message) {
    if (error) *error = message;
    return false;
}
bool integer(const QJsonValue& value, double minimum, double maximum) {
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && n >= minimum && n <= maximum && std::floor(n) == n;
}
bool text(const QJsonValue& value) { return value.isString() && !value.toString().trimmed().isEmpty(); }
bool number(const QJsonValue& value) { return value.isDouble() && std::isfinite(value.toDouble()); }
constexpr double MaximumId = std::numeric_limits<uint32_t>::max();
// Parse first, then check raw tokens: QJsonObject alone collapses duplicates.
// Decode keys with Qt too, so escaped aliases have the same identity as Qt keys.
bool parse(const QByteArray& bytes, QJsonObject& object, QString* error) {
    if (bytes.size() > MaximumDocumentBytes) return fail(error, "Patch exceeds the 2 MiB limit.");
    QJsonParseError problem;
    const auto json = QJsonDocument::fromJson(bytes, &problem);
    if (problem.error != QJsonParseError::NoError)
        return fail(error, QString("Invalid JSON at byte %1: %2").arg(problem.offset).arg(problem.errorString()));
    if (!json.isObject()) return fail(error, "Patch must be an object.");
    QVector<QSet<QString>> keys;
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        const char c = bytes[i];
        if (c == '{' || c == '[') keys.append(QSet<QString>{});
        else if (c == '}' || c == ']') keys.removeLast();
        else if (c == '"') {
            const qsizetype begin = i++;
            while (i < bytes.size() && bytes[i] != '"') { if (bytes[i] == '\\') ++i; ++i; }
            qsizetype after = i + 1;
            while (after < bytes.size() && (bytes[after] == ' ' || bytes[after] == '\t' || bytes[after] == '\r' || bytes[after] == '\n')) ++after;
            if (after < bytes.size() && bytes[after] == ':') {
                const auto token = bytes.mid(begin, i - begin + 1);
                const QString key = QJsonDocument::fromJson(QByteArray("[") + token + ']').array()[0].toString();
                if (keys.last().contains(key)) return fail(error, QString("Duplicate JSON key: %1").arg(key));
                keys.last().insert(key);
            }
        }
    }
    object = json.object();
    return true;
}
bool endpoint(const QJsonValue& value, const QSet<quint32>& ids, QString* error) {
    if (!value.isObject()) return fail(error, "Cable/output endpoint must be an object.");
    const auto o = value.toObject();
    if (!integer(o["module"], 1, MaximumId) || !ids.contains(quint32(o["module"].toDouble())) || !text(o["port"]))
        return fail(error, "Cable/output endpoint references a missing module or invalid port.");
    return true;
}
} // namespace

bool add_manifest(ModuleCatalog& catalog, const QJsonObject& manifest,
                  const QString& default_output, QString* error) {
    if (error) error->clear();
    if (!text(manifest["id"]) || catalog.contains(manifest["id"].toString()))
        return fail(error, "Manifest ID is missing or duplicated.");
    ModuleInfo info; info.default_output = default_output;
    if (manifest.contains("parameters") && !manifest["parameters"].isObject()) return fail(error, "Invalid parameter manifest.");
    const auto parameters = manifest["parameters"].toObject();
    for (auto i = parameters.begin(); i != parameters.end(); ++i) {
        if (i.key().isEmpty() || !integer(i.value(), 0, MaximumId)) return fail(error, "Invalid legacy parameter mapping.");
        const auto key = QString::number(quint32(i.value().toDouble()));
        if (info.legacy_parameters.contains(key)) return fail(error, "Duplicate legacy parameter mapping.");
        info.legacy_parameters.insert(key, i.key());
    }
    catalog.insert(manifest["id"].toString(), info);
    return true;
}

bool validate_patch(const QJsonObject& o, QString* error) {
    if (error) error->clear();
    if (o["format"] != "wirescape.native.patch" || o["version"] != 2)
        return fail(error, "Only native patch version 2 can be saved.");
    if (!o["settings"].isObject() || !o["modules"].isArray() || !o["connections"].isArray() || !o["outputs"].isArray())
        return fail(error, "settings/modules/connections/outputs have invalid types.");
    const auto settings = o["settings"].toObject();
    for (const auto& key : {"max_block_frames", "feedback_block", "max_channels"})
        if (settings.contains(key) && !integer(settings[key], 1, QString(key) == "max_channels" ? 16 : 4096))
            return fail(error, QString("Invalid setting: %1").arg(key));
    if (settings.contains("sample_rate") && (!number(settings["sample_rate"]) || settings["sample_rate"].toDouble() <= 0))
        return fail(error, "Invalid sample_rate.");
    if (settings["feedback_block"].toInt(1) > settings["max_block_frames"].toInt(256))
        return fail(error, "feedback_block exceeds max_block_frames.");
    if (settings.contains("seed") && !integer(settings["seed"], 0, std::numeric_limits<double>::max())) return fail(error, "Invalid seed.");
    if (settings.contains("timing_mode") && settings["timing_mode"] != "immediate" && settings["timing_mode"] != "timestamped")
        return fail(error, "Invalid timing_mode.");
    QSet<quint32> ids;
    for (const auto& item : o["modules"].toArray()) {
        if (!item.isObject()) return fail(error, "Module must be an object.");
        const auto m = item.toObject();
        if (!integer(m["id"], 1, MaximumId) || !text(m["module"]) || !integer(m["version"], 1, MaximumId) || !m["parameters"].isObject())
            return fail(error, "Invalid module ID/type/version/parameters.");
        const auto id = quint32(m["id"].toDouble());
        if (ids.contains(id)) return fail(error, "Duplicate module ID.");
        ids.insert(id);
        const auto parameters = m["parameters"].toObject();
        for (auto p = parameters.begin(); p != parameters.end(); ++p)
            if (p.key().isEmpty() || !number(p.value())) return fail(error, "Invalid plain parameter value.");
        if (m.contains("config") && !m["config"].isObject()) return fail(error, "Invalid module config.");
        if (m.contains("inactive") && !m["inactive"].isBool()) return fail(error, "Invalid inactive flag.");
        if (m.contains("channel_seed") && !integer(m["channel_seed"], 0, std::numeric_limits<double>::max())) return fail(error, "Invalid channel seed.");
        if (m.contains("state")) {
            const auto state = m["state"].toObject();
            if (!m["state"].isObject() || !integer(state["version"], 0, MaximumId) || state["encoding"] != "base64" || !state["data"].isString())
                return fail(error, "Invalid state envelope.");
        }
    }
    for (const auto& item : o["connections"].toArray()) {
        if (!item.isObject()) return fail(error, "Connection must be an object.");
        const auto cable = item.toObject(); const auto kind = cable["kind"].toString();
        if (kind != "signal" && kind != "modulation" && kind != "event") return fail(error, "Invalid connection kind.");
        if (!endpoint(cable["from"], ids, error) || !endpoint(cable["to"], ids, error)) return false;
        if (cable.contains("feedback") && !cable["feedback"].isBool()) return fail(error, "Invalid feedback flag.");
        if ((kind == "modulation" || cable.contains("amount")) && (!number(cable["amount"]) || std::abs(cable["amount"].toDouble()) > 1))
            return fail(error, "Invalid modulation amount.");
        if (cable.contains("stereo_pair") && !cable["stereo_pair"].isString()) return fail(error, "Invalid stereo_pair.");
    }
    for (const auto& item : o["outputs"].toArray()) {
        if (!endpoint(item, ids, error)) return false;
        if (!integer(item.toObject()["bus_channel"], 0, 15)) return fail(error, "Invalid output bus channel.");
    }
    for (const auto& key : {"host_mappings", "devices"}) if (o.contains(key)) {
        if (!o[key].isArray()) return fail(error, QString("Invalid %1.").arg(key));
        for (const auto& item : o[key].toArray()) if (!item.isObject()) return fail(error, QString("Invalid %1 entry.").arg(key));
    }
    return true;
}

PatchDocument read_patch(const QByteArray& bytes, const ModuleCatalog& catalog) {
    PatchDocument result; result.original_bytes = bytes;
    QJsonObject original;
    if (!parse(bytes, original, &result.error)) return result;
    if (original["format"] != "wirescape.native.patch" || !integer(original["version"], 1, 2)) {
        result.error = "Unsupported patch format/version; file must not be overwritten."; return result;
    }
    result.source_version = original["version"].toInt(); result.object = original;
    if (result.source_version == 1) {
        const auto inspected = inspect(bytes);
        if (inspected.format != Format::NativeV1) { result.error = inspected.diagnostic; return result; }
        // Whole v1 envelopes make preservation collision-free, including fields
        // whose names acquire new meanings in v2. They are never DSP state.
        result.object["preserved"] = original;
        QJsonArray modules, cables, outputs;
        for (const auto& item : original["modules"].toArray()) {
            const auto old = item.toObject(); auto m = old;
            const auto type = old["type"].toString(); const auto found = catalog.constFind(type);
            QJsonObject parameters;
            if (old.contains("values") && !old["values"].isObject()) { result.error = "Invalid v1 values object."; return result; }
            if (found != catalog.cend()) {
                const auto values = old["values"].toObject();
                for (auto p = values.begin(); p != values.end(); ++p) if (found->legacy_parameters.contains(p.key())) {
                    if (!number(p.value())) { result.error = "Invalid mapped v1 parameter."; return result; }
                    parameters[found->legacy_parameters[p.key()]] = p.value();
                }
            }
            m.remove("type"); m.remove("values"); m.remove("state");
            m.remove("inactive"); m.remove("channel_seed"); m["config"] = QJsonObject{};
            m["module"] = type; m["version"] = 1; m["parameters"] = parameters; m["preserved"] = old;
            if (found == catalog.cend()) m["inactive"] = true;
            modules.append(m);
            if (old["id"] == original["outputNode"] && found != catalog.cend()) {
                if (found->default_output.isEmpty()) { result.error = "Catalog lacks the v1 output-zero port ID."; return result; }
                outputs.append(QJsonObject{{"module", old["id"]}, {"port", found->default_output}, {"bus_channel", 0}});
            }
        }
        for (const auto& item : original["connections"].toArray()) {
            auto c = item.toObject(); const auto kind = c["kind"].toString();
            c["kind"] = (kind == "audio" || kind == "control") ? "signal" : kind;
            if (kind == "audio" || kind == "control") { c.remove("amount"); c.remove("feedback"); c.remove("stereo_pair"); }
            c["from"] = QJsonObject{{"module", c["fromNode"]}, {"port", c["fromPort"]}};
            c["to"] = QJsonObject{{"module", c["toNode"]}, {"port", c["toPort"]}};
            c.remove("fromNode"); c.remove("fromPort"); c.remove("toNode"); c.remove("toPort"); cables.append(c);
        }
        if (original["outputNode"].toInt() != 0) {
            bool found = false;
            for (const auto& item : modules) found |= item.toObject()["id"] == original["outputNode"];
            if (!found) { result.error = "v1 outputNode references a missing module."; return result; }
        }
        auto settings = original["settings"].toObject();
        settings["sample_rate"] = 48000;
        settings["max_block_frames"] = 256;
        settings["max_channels"] = 16;
        settings["feedback_block"] = 1;
        settings.remove("seed"); settings.remove("timing_mode");
        result.object["host_mappings"] = QJsonArray{}; result.object["devices"] = QJsonArray{};
        result.object["version"] = 2; result.object["settings"] = settings;
        result.object["modules"] = modules; result.object["connections"] = cables; result.object["outputs"] = outputs;
    }
    validate_patch(result.object, &result.error);
    return result;
}
bool save_patch(const QString& path, const PatchDocument& document, QString* error) {
    if (error) error->clear();
    if (!document.valid()) return fail(error, "Cannot save a failed/unloaded patch.");
    if (!validate_patch(document.object, error)) return false;
    const auto bytes = QJsonDocument(document.object).toJson(QJsonDocument::Indented);
    if (bytes.size() > MaximumDocumentBytes) return fail(error, "Serialized patch exceeds the 2 MiB limit.");
    QByteArray backup_bytes = document.source_version == 1 ? document.original_bytes : QByteArray{};
    if (QFile::exists(path)) {
        QFile existing(path);
        if (!existing.open(QIODevice::ReadOnly)) return fail(error, existing.errorString());
        const auto prior = existing.read(MaximumDocumentBytes + 1);
        if (existing.error() != QFile::NoError) return fail(error, existing.errorString());
        QJsonObject object;
        if (!parse(prior, object, error)) return false;
        if (object["format"] != "wirescape.native.patch") return fail(error, "Refusing to overwrite another format.");
        if (object["version"] == 1) {
            if (inspect(prior).format != Format::NativeV1) return fail(error, "Refusing to overwrite malformed v1.");
            backup_bytes = prior;
        } else if (object["version"] != 2 || !validate_patch(object, error)) {
            return fail(error, "Refusing to overwrite an invalid/future patch.");
        }
    }
    if (!backup_bytes.isEmpty()) {
        QJsonObject original;
        if (!parse(backup_bytes, original, error) || inspect(backup_bytes).format != Format::NativeV1)
            return fail(error, "Invalid v1 backup source.");
        const QFileInfo target(path);
        const QString backup_path = target.dir().filePath(target.completeBaseName() + ".v1.json");
        if (QFileInfo(backup_path).exists() && !QFileInfo(backup_path).isFile()) return fail(error, "Backup path is not a file.");
        if (!QFile::exists(backup_path)) {
            QTemporaryFile temporary(target.dir().filePath(".wirescape-backup-XXXXXX"));
            if (!temporary.open() || temporary.write(backup_bytes) != backup_bytes.size() || !temporary.flush())
                return fail(error, "Could not stage v1 backup; target unchanged.");
            // QTemporaryFile::rename closes its native handle before publishing.
            // QFile::rename refuses an existing destination, including a race.
            if (!temporary.rename(backup_path)) {
                if (!QFileInfo(backup_path).isFile()) return fail(error, "Could not publish v1 backup; target unchanged.");
            } else {
                temporary.setAutoRemove(false);
            }
        }
    }
    QSaveFile target(path); // same-directory temporary + atomic commit
    target.setDirectWriteFallback(false);
    if (!target.open(QIODevice::WriteOnly) || target.write(bytes) != bytes.size()) return fail(error, target.errorString());
    if (!target.commit()) return fail(error, target.errorString());
    return true;
}
} // namespace wirescape::document
