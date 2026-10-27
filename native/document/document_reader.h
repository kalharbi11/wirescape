#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>

namespace wirescape::document {
inline constexpr qsizetype MaximumDocumentBytes = 2 * 1024 * 1024;
enum class Format {
    Invalid, UnknownFormat, UnsupportedVersion, NativeV1,
    LegacyPatchV1, LegacyPatchV2, LegacyPatchV3, VisualStudyV4
};

struct LegacyStateFields {
    QString moduleId;
    // Undefined distinguishes absence from an explicitly stored null.
    QJsonValue sequence;
    QJsonValue moduleState;
};

struct ReadResult {
    Format format = Format::Invalid;
    QByteArray originalBytes;
    QJsonObject object;
    QString diagnostic;
    QVector<LegacyStateFields> legacyStateFields;
    bool recognizedStructure() const;
};

// Pure inspection only. Success says nothing about graph readiness, installed
// processors, parameter meanings, port compatibility, or migration equivalence.
// originalBytes is the lossless representation, including on failure. QJsonObject
// follows Qt JSON semantics (including collapsed duplicate JSON object keys).
ReadResult inspect(const QByteArray& bytes);
} // namespace wirescape::document
