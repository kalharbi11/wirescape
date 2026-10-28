// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "document_reader.h"
#include <QHash>

namespace wirescape::document {
struct ModuleInfo {
    QHash<QString, QString> legacy_parameters; // numeric descriptor ID -> stable ID
    QString default_output; // descriptor output zero, supplied by the host
};
using ModuleCatalog = QHash<QString, ModuleInfo>;
// Catalog construction is quiescent; manifests, not display labels, define IDs.
bool add_manifest(ModuleCatalog& catalog, const QJsonObject& manifest,
                  const QString& default_output, QString* error = nullptr);
struct PatchDocument {
    QJsonObject object;
    QByteArray original_bytes;
    int source_version = 0;
    QString error;
    bool valid() const { return error.isEmpty() && (source_version == 1 || source_version == 2); }
};
// Legacy inspection remains separate. This API accepts native patches only.
PatchDocument read_patch(const QByteArray& bytes, const ModuleCatalog& catalog);
bool validate_patch(const QJsonObject& object, QString* error = nullptr);
// Atomic v2 save; a v1 source receives a non-overwritten sibling backup.
bool save_patch(const QString& path, const PatchDocument& document, QString* error = nullptr);
} // namespace wirescape::document
