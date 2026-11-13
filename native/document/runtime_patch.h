// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include <map>
#include <string>
namespace wirescape::osc { struct OutBus; }
namespace wirescape::document {
constexpr size_t RuntimePatchLimit=size_t{2}*1024*1024;
// Quiescent, Qt-free reader/graph builder shared by the app and headless runner.
// It never writes documents; the Studio codec owns editable documents and migration.
AppEngine* load_runtime_patch(const std::string& bytes,AppEngine* catalog,
                             const ws_engine_config& config,std::string& error,
                             const std::vector<engine2::Tap>* telemetry_taps=nullptr,
                             const std::map<uint32_t, osc::OutBus*>* osc_out_buses=nullptr);
}
