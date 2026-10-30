// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include <string>
namespace wirescape::document {
constexpr size_t RuntimePatchLimit=2*1024*1024;
// Quiescent, Qt-free reader/graph builder shared by the app and headless runner.
// It never writes documents; Qt's lossless migration/atomic writer remains authoritative.
AppEngine* load_runtime_patch(const std::string& bytes,AppEngine* catalog,
                             const ws_engine_config& config,std::string& error);
}
