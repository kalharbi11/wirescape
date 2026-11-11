// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "app_engine.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
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
class RateUnsupported : public std::runtime_error {
  public:
    RateUnsupported() : std::runtime_error("rate-unsupported") {}
};
struct Device {
    std::string id, name, host_api;
    int index = -1;
    bool is_default = false;
    std::string backend;
    int output_channels = 2;
    std::vector<uint32_t> sample_rates{44100, 48000, 96000};
    bool supported = true;
    std::string unsupported_reason;
    double default_sample_rate = 0;
};
// Stable output identity. Windows exposes one endpoint per host API (MME, DirectSound,
// WASAPI, WDM-KS) and repeats names even inside one host API, so the id is
// "<host API>|<name>", with "#<n>" for the n-th repeat (n >= 2) of that pair in
// enumeration order. Raw PortAudio indices shift on any hot-plug and are never ids.
struct RawDevice {
    std::string name, host_api;
};
// Read-only Windows registry names; no PortAudio init or device opening. Linux returns empty.
std::vector<std::string> installed_asio_drivers();
// Pure, device-off comparison shared by the real enumerator and fake-backend tests.
inline std::vector<std::string> unavailable_asio_drivers(const std::vector<std::string> &installed,
                                                        const std::vector<RawDevice> &visible) {
    std::vector<std::string> missing;
    for (const auto &name : installed) {
        if (name.empty()) continue;
        if (std::any_of(visible.begin(), visible.end(), [&](const RawDevice &device) {
                return device.host_api == "ASIO" && device.name == name;
            }))
            continue;
        std::string bounded = name;
        if (bounded.size() > 64) {
            size_t end = 64;
            // Preserve UTF-8 when the registry name crosses the wire-size boundary.
            while (end && (static_cast<unsigned char>(bounded[end]) & 0xc0u) == 0x80u) --end;
            bounded.resize(end);
        }
        if (bounded.empty() || std::find(missing.begin(), missing.end(), bounded) != missing.end()) continue;
        missing.push_back(std::move(bounded));
        if (missing.size() == 16) break;
    }
    return missing;
}

std::vector<std::string> device_ids(const std::vector<RawDevice> &devices);
// Resolves a selection to a position in `devices`: an exact id first, then a legacy
// bare name only when exactly one device has it. Returns -1 when missing or ambiguous.
int resolve_device(const std::vector<Device> &devices, const std::string &selection);
struct StreamSelection {
    std::string directory, name; // name holds a device id (or a unique legacy name)
};
// Unknown probe results must not turn a busy endpoint into an unsupported route.
struct RateSupport {
    bool known = true;
    std::vector<uint32_t> rates;
};
inline RateSupport classify_rates(const std::array<int, 3> &results, int invalid_sample_rate) {
    constexpr std::array<uint32_t, 3> rates{44100, 48000, 96000};
    RateSupport support;
    support.rates.reserve(results.size());
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i] >= 0) support.rates.push_back(rates[i]);
        else if (results[i] != invalid_sample_rate) support.known = false;
    }
    if (!support.known) support.rates.clear();
    return support;
}
struct DeviceQuery {
    bool probe_rates = true;
    std::string target_id, active_id;
};
std::vector<Device> devices(const std::string &directory, const DeviceQuery &query = {});
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
    // Hands `next` to the running callback at a block boundary without reopening the
    // device. On success `next` holds the retired engine, which the callback no longer
    // touches. False (deadline passed or no stream) leaves both engines unchanged.
    bool exchange(EngineOwner &next, std::chrono::steady_clock::time_point deadline);
    bool running() const;
    double cpu_percent() const;
    uint64_t dropouts() const;
};
// Enumeration never starts a stream. Only CLI --confirm-audio calls play().
void list_devices(const std::string& directory);
void play(EngineOwner engine,const std::string& directory,const std::string& device,
          double rate,uint32_t block,const std::atomic<bool>&stop,std::function<void()> stop_producers = {});
}

