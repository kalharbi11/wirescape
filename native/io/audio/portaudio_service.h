// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
namespace wirescape::audio {
using EngineOwner=std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>;
// Enumeration never starts a stream. Only CLI --confirm-audio calls play().
void list_devices(const std::string& directory);
void play(EngineOwner engine, const std::string &directory, double rate, const std::string &device, uint32_t block,
          const std::atomic<bool> &stop, std::function<void()> stop_producers = {},
          const std::function<void()> &control_tick = {});
}
