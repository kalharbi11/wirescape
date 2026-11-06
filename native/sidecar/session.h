// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../io/audio/portaudio_service.h"
#include "devices.h"
#include "midi_host.h"
#include <set>
namespace wirescape::sidecar {
struct Options {
    bool audio = false, midi = false, null_audio = false;
    std::string device, settings = "user_data/settings.json", directory;
    uint32_t rate = 48000, block = 256;
};
class Session {
    Options options_;
    ws_engine_config config_;
    audio::EngineOwner catalogue_, engine_;
    AppEngine *active_ = nullptr;
    std::unique_ptr<audio::Stream> stream_;
    ObservedMidi *observer_ = nullptr;
    std::unique_ptr<midi::Host> midi_;
    Json patch_, preferences_ = Json::object(), audio_devices_, midi_devices_;
    std::string token_, device_, fallback_, audio_state_ = "no-device";
    uint64_t generation_ = 0, tick_ = 0;
    std::set<uint32_t> used_ids_;
    std::array<bool, 128> keyboard_{};
    std::mutex sink_mutex_;
    std::condition_variable sink_wake_;
    std::thread sink_;
    std::atomic<bool> stop_sink_{false}, sink_rendering_{false};
    std::atomic<double> cpu_{0};
    std::atomic<uint64_t> dropouts_{0};
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    void start_audio();
    void stop_audio();
    void start_midi();
    void persist();
    void refresh_devices();
    Json execution(Json);
    void commit(Json, bool);
    Json apply(const Json &);
    Json audio_status() const;
    Json midi_status() const;
    Json base() const;

  public:
    explicit Session(Options);
    ~Session();
    void shutdown();
    void start(); // begin rendering only after the hello handshake is flushed
    Json hello() const;
    Json reply(const Request &);
    Json status() const;
    Json telemetry();
    Json devices();
    bool poll();
};
Options parse_options(int, char **);
} // namespace wirescape::sidecar
