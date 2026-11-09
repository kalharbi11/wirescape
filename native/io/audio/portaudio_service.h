// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace wirescape::audio {
using EngineOwner=std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>;
class ShutdownFailure : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
struct Device {
    std::string id, name, host_api;
    int index = -1;
    bool is_default = false;
};
// Stable output identity. Windows exposes one endpoint per host API (MME, DirectSound,
// WASAPI, WDM-KS) and repeats names even inside one host API, so the id is
// "<host API>|<name>", with "#<n>" for the n-th repeat (n >= 2) of that pair in
// enumeration order. Raw PortAudio indices shift on any hot-plug and are never ids.
struct RawDevice {
    std::string name, host_api;
};
std::vector<std::string> device_ids(const std::vector<RawDevice> &devices);
// Resolves a selection to a position in `devices`: an exact id first, then a legacy
// bare name only when exactly one device has it. Returns -1 when missing or ambiguous.
int resolve_device(const std::vector<Device> &devices, const std::string &selection);
struct StreamSelection {
    std::string directory, name; // name holds a device id (or a unique legacy name)
};
std::vector<Device> devices(const std::string &directory);
// Control-owned stream. Context exclusively owns the engine until stop proves
// callbacks quiescent. A failed backend shutdown quarantines context and engine.
class Stream {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    Stream(EngineOwner &, const StreamSelection &selection, double rate, uint32_t block,
           std::function<void()> stop_producers = {});
    ~Stream();
    EngineOwner stop();
    bool running() const;
    double cpu_percent() const;
    uint64_t dropouts() const;
};
// Enumeration never starts a stream. Only CLI --confirm-audio calls play().
void list_devices(const std::string& directory);
void play(EngineOwner engine,const std::string& directory,const std::string& device,
          double rate,uint32_t block,const std::atomic<bool>&stop,std::function<void()> stop_producers = {});
}
