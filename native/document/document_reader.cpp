#include "document_reader.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <cmath>
#include <limits>

namespace wirescape::document {
namespace {
bool integer(const QJsonValue& value, int minimum) {
    const double n = value.toDouble(-1);
    return value.isDouble() && std::isfinite(n) && n >= minimum &&
        n <= std::numeric_limits<int>::max() && std::floor(n) == n;
}
bool text(const QJsonValue& value) {
    return value.isString() && !value.toString().trimmed().isEmpty();
}
bool address(const QJsonValue& value) {
    if (!text(value)) return false;
    const QString s = value.toString();
    const auto colon = s.lastIndexOf(':');
    return colon > 0 && colon < s.size() - 1 &&
        !s.left(colon).trimmed().isEmpty() && !s.mid(colon + 1).trimmed().isEmpty();
}
} // namespace

bool ReadResult::recognizedStructure() const {
    return format == Format::NativeV1 || format == Format::LegacyPatchV1 ||
        format == Format::LegacyPatchV2 || format == Format::LegacyPatchV3 ||
        format == Format::VisualStudyV4;
}

ReadResult inspect(const QByteArray& bytes) {
    ReadResult result;
    result.originalBytes = bytes;
    const auto fail = [&](const QString& message) {
        result.format = Format::Invalid;
        result.diagnostic = message;
        result.legacyStateFields.clear();
        return result;
    };
    if (bytes.size() > MaximumDocumentBytes)
        return fail(QStringLiteral("Document exceeds the 2 MiB inspection limit; original bytes retained."));
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError)
        return fail(QStringLiteral("Invalid JSON at byte %1: %2").arg(error.offset).arg(error.errorString()));
    if (!document.isObject()) return fail(QStringLiteral("Document must be a JSON object."));
    result.object = document.object();
    const auto& object = result.object;
    if (!text(object.value("format")) || !integer(object.value("version"), 1))
        return fail(QStringLiteral("format must be a nonempty string and version a positive integer."));
    const auto name = object.value("format").toString();
    const int version = object.value("version").toInt();
    if (name == "wirescape.native.patch" && version == 1) result.format = Format::NativeV1;
    else if (name == "wirescape.patch" && version >= 1 && version <= 3)
        result.format = version == 1 ? Format::LegacyPatchV1 : version == 2 ? Format::LegacyPatchV2 : Format::LegacyPatchV3;
    else if (name == "wirescape.visual.study" && version == 4) result.format = Format::VisualStudyV4;
    else {
        const bool known = name == "wirescape.native.patch" || name == "wirescape.patch" || name == "wirescape.visual.study";
        result.format = known ? Format::UnsupportedVersion : Format::UnknownFormat;
        result.diagnostic = known ? QStringLiteral("Unsupported document version; bytes retained without interpreting its schema.")
                                  : QStringLiteral("Unknown document format; bytes retained.");
        return result;
    }
    if (!object.value("modules").isArray() || !object.value("connections").isArray())
        return fail(QStringLiteral("modules and connections must be arrays."));
    const bool native = result.format == Format::NativeV1;
    if (native && (!object.value("name").isString() || !integer(object.value("outputNode"), 0)))
        return fail(QStringLiteral("Native name/outputNode have invalid types."));
    QSet<QString> identities;
    for (const auto& item : object.value("modules").toArray()) {
        if (!item.isObject()) return fail(QStringLiteral("Every module must be an object."));
        const auto module = item.toObject();
        const auto id = module.value("id");
        if ((native ? !integer(id, 1) : !text(id)) || !text(module.value("type")))
            return fail(QStringLiteral("Module identity/type is invalid."));
        const QString key = native ? QString::number(id.toInt()) : id.toString();
        if (identities.contains(key)) return fail(QStringLiteral("Duplicate module identity: %1").arg(key));
        identities.insert(key);
        // Do not interpret any values, state, coordinates or musical units here.
        if (!native && (module.contains("sequence") || module.contains("module_state")))
            result.legacyStateFields.append({key, module.value("sequence"), module.value("module_state")});
    }
    for (const auto& item : object.value("connections").toArray()) {
        if (!item.isObject()) return fail(QStringLiteral("Every connection must be an object."));
        const auto cable = item.toObject();
        if (native) {
            if (!integer(cable.value("fromNode"), 1) || !integer(cable.value("toNode"), 1) ||
                !text(cable.value("fromPort")) || !text(cable.value("toPort")) || !text(cable.value("kind")))
                return fail(QStringLiteral("Native connection identity fields are invalid."));
        } else if (!address(cable.value("from")) || !address(cable.value("to"))) {
            return fail(QStringLiteral("Legacy connection addresses require module:port strings."));
        }
    }
    result.diagnostic = native ? QStringLiteral("Native v1 structure inspected; full graph validation is still required.")
                              : QStringLiteral("Legacy structure inspected; explicit mapping is required before import.");
    return result;
}
} // namespace wirescape::document
