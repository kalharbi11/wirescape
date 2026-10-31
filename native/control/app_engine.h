// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape/engine.h"
#include "engine2.h"
// Internal app facade, not an ABI. Legacy structures are UI metadata only.
struct AppEngine;
AppEngine* app_engine_create(const ws_engine_config*);
wirescape::engine2::Config AppEngineConfig(const ws_engine_config&);
AppEngine* app_engine_create_config(const ws_engine_config*,const wirescape::engine2::Config*);
void app_engine_destroy(AppEngine*);
const char* app_engine_error(const AppEngine*);
uint32_t app_engine_abi_version();
int app_engine_load_module(AppEngine*,const char*);
int app_engine_register_module(AppEngine*,const ws_module_descriptor*);
int app_engine_copy_modules(AppEngine* destination,const AppEngine* source);
uint32_t app_engine_module_count(const AppEngine*);
const ws_module_descriptor* app_engine_module(const AppEngine*,uint32_t);
int app_engine_add_node(AppEngine*,uint32_t,const char*);
int app_engine_add_node_config(AppEngine*,uint32_t,const char*,const std::vector<uint8_t>&);
int app_engine_connect(AppEngine*,uint32_t,uint32_t,uint32_t,uint32_t);
int app_engine_prepare(AppEngine*,uint32_t,uint32_t);
int app_engine_initialize_parameter(AppEngine*,uint32_t,uint32_t,float);
int app_engine_enqueue(AppEngine*,const ws_event*);
int app_engine_render(AppEngine*,float*,uint32_t);
int app_engine_render_channels(AppEngine*,float*const*,uint32_t,uint32_t);
// Live audio callbacks: publish the callback's steady-clock time as the D13 anchor so
// Timestamped ingress (MIDI SystemMonotonic, service chokes) maps onto the timeline.
uint64_t app_engine_clock_ns();
int app_engine_render_live(AppEngine*,float*const*,uint32_t,uint32_t,uint64_t callback_time_ns);
int app_engine_reset(AppEngine*);
int app_engine_activate(AppEngine*);
int app_engine_deactivate(AppEngine*);
int app_engine_panic(AppEngine*);
uint64_t app_engine_generation(const AppEngine*);
int app_engine_get_meter(const AppEngine*,ws_meter*);
int app_engine_build_essentials(AppEngine*,const char*);
wirescape::engine2::Engine& app_engine_native(AppEngine*);

const char* app_engine_parameter_id(const AppEngine*,const char*module,uint32_t index);
