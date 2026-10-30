// SPDX-License-Identifier: GPL-3.0-only
#include "midi_backend.h"
#include <libremidi/libremidi.hpp>
#include <map>
namespace wirescape::midi {
namespace {
#ifdef _WIN32
constexpr auto platform = libremidi::API::WINDOWS_MM;
#else
constexpr auto platform = libremidi::API::ALSA_SEQ;
#endif
class RealBackend final : public Backend {
    libremidi::observer observer{libremidi::observer_configuration{}, platform};
    std::map<std::string, std::unique_ptr<libremidi::midi_in>> opened;
    static Endpoint identity(const libremidi::input_port& port) {
        const auto name = port.display_name.empty() ? port.port_name : port.display_name;
        return {name, port.device_name + ":" + port.port_name};
    }
public:
    std::vector<Endpoint> endpoints() override {
        std::vector<Endpoint> result;
        for (const auto& port : observer.get_input_ports()) result.push_back(identity(port));
        return result;
    }
    bool open(const Endpoint& endpoint, Callback callback) override {
        if (opened.count(endpoint.system_name)) return true;
        for (const auto& port : observer.get_input_ports()) {
            const auto key = identity(port);
            if (key.system_name != endpoint.system_name) continue;
            libremidi::input_configuration cfg;
            cfg.ignore_sysex = cfg.ignore_timing = cfg.ignore_sensing = false;
            cfg.timestamps = libremidi::timestamp_mode::SystemMonotonic;
            cfg.on_message = [callback](libremidi::message&& message) {
                if (callback.message) callback.message(callback.context, message.bytes.data(), message.size(), static_cast<uint64_t>(message.timestamp));
            };
            auto input = std::make_unique<libremidi::midi_in>(cfg, platform);
            if (input->open_port(port) != stdx::error{}) return false;
            opened.emplace(endpoint.system_name, std::move(input)); return true;
        }
        return false;
    }
    void close(const Endpoint& endpoint) override { opened.erase(endpoint.system_name); }
};
}
std::unique_ptr<Backend> real_backend(bool confirm_midi) {
    if (!confirm_midi) return nullptr;
    return std::make_unique<RealBackend>();
}
}
