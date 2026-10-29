// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "patch_document.h"
#include "app_engine.h"
namespace wirescape::document {
ModuleCatalog app_catalog(AppEngine*);
QJsonObject editor_view(const PatchDocument&,AppEngine*);
PatchDocument editor_patch(const QJsonObject&,AppEngine*,const PatchDocument& source={});
AppEngine* build_app_patch(AppEngine* catalog,const PatchDocument&,const ws_engine_config&,QString& error,uint64_t seed=1);
}
