// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../document/telemetry_taps.h"
#include "../io/audio/engine_slot.h"
#include "../io/audio/portaudio_service.h"
#include "devices.h"
#include "midi_host.h"
#include "osc_host.h"
#include "../control/osc_out_host.h"
#include <array>
#include <chrono>
#include <optional>
#include <set>
namespace wirescape::sidecar {
struct Options {
    bool audio = false, midi = false, lan = false, null_audio = false, bench_summary = false;
    std::string device, settings = "user_data/settings.json", directory;
    uint32_t rate = 48000, block = 256;
};
// The sink thread is the sole writer. Read only after it has joined.
struct RenderHistogram {
    std::array<uint64_t, 201> bins{}; // 0..199%, plus >=200%
    uint64_t blocks = 0, overruns = 0, faults = 0, previous_faults = 0;
    double max_percent = 0;
    uint64_t overrun_render_count = 0, overrun_wake_count = 0;
    double overrun_render_max_us = 0, overrun_wake_max_us = 0;

    // Sample the existing Engine2 render-fault meter on the sink thread.
    // A replan may reset the engine counter; retain all observed deltas.
    void observe_faults(uint64_t current) noexcept {
        faults += current >= previous_faults ? current - previous_faults : current;
        previous_faults = current;
    }
    void record(double percent, bool late) noexcept {
        ++bins[static_cast<size_t>(percent >= 200 ? 200 : percent)];
        ++blocks;
        if (late) ++overruns;
        if (percent > max_percent) max_percent = percent;
    }
    // Reporting only: lateness still means completion past the absolute deadline.
    // Both causes can contribute to the same overrun; counts are not exclusive.
    void record_timing(double percent, std::chrono::steady_clock::time_point before,
                       std::chrono::steady_clock::time_point after, std::chrono::steady_clock::time_point start,
                       std::chrono::steady_clock::time_point deadline) noexcept {
        const bool late = after > deadline;
        record(percent, late);
        if (!late) return;
        const double render_us = std::chrono::duration<double, std::micro>(after - before).count();
        const double wake_us = std::chrono::duration<double, std::micro>(before - start).count();
        if (render_us > overrun_render_max_us) overrun_render_max_us = render_us;
        if (wake_us > overrun_wake_max_us) overrun_wake_max_us = wake_us;
        if (after - before > deadline - start) ++overrun_render_count;
        if (before > start) ++overrun_wake_count;
    }
    uint32_t percentile(uint32_t pct) const noexcept {
        if (!blocks) return 0;
        uint64_t rank = (blocks - 1) * pct / 100 + 1;
        for (size_t i = 0; i < bins.size(); ++i) {
            if (rank <= bins[i]) return static_cast<uint32_t>(i);
            rank -= bins[i];
        }
        return 200;
    }
};
class Session {
    Options options_;
    ws_engine_config config_;
    audio::EngineOwner catalogue_, engine_;
    AppEngine *active_ = nullptr;
    std::unique_ptr<audio::Stream> stream_;
    ObservedMidi *observer_ = nullptr;
    std::unique_ptr<monitor::Capture> monitor_;
    std::unique_ptr<midi::Host> midi_;
    std::unique_ptr<osc::Host> osc_;
    std::unique_ptr<osc::OutHost> osc_out_;
    Json patch_, preferences_ = Json::object(), audio_devices_, midi_devices_;
    Json unavailable_audio_drivers_ = Json::array(); // optional, control-thread ASIO diagnostics
    std::string token_, device_, audio_state_ = "no-device", audio_reason_, audio_error_;
    uint32_t requested_block_ = 0; // user's block while an ASIO driver forces another; 0 = none
    uint64_t generation_ = 0, tick_ = 0;
    std::set<uint32_t> used_ids_;
    document::telemetry::Sampler telemetry_;
    std::array<bool, 128> keyboard_{};
    std::mutex sink_mutex_;
    std::condition_variable sink_wake_;
    std::thread sink_;
#ifdef _WIN32
    // Control-owned manual-reset event: interrupts the sink's kernel timer
    // before join; never closed while the render thread may still wait on it.
    void *sink_stop_event_ = nullptr;
#endif
    std::atomic<bool> stop_sink_{false}, sink_rendering_{false};
    audio::EngineSlot sink_slot_; // null-sink twin of the PortAudio callback handoff
    std::atomic<double> cpu_{0};
    std::atomic<uint64_t> dropouts_{0};
    RenderHistogram bench_; // only sink thread records; joined before shutdown reads
    bool bench_reported_ = false;
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point audio_retry_{};
    // Last cold audio enumeration (no open stream). A cold PortAudio init enumerates every
    // host API on the command thread, so it runs at most every kColdEnumeration.
    std::optional<std::chrono::steady_clock::time_point> cold_enumeration_;
    std::chrono::milliseconds audio_backoff_{100};
    void start_audio(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    bool poll_audio(std::chrono::steady_clock::time_point, bool enumerated);
    void open_at_driver_block(int64_t frames);
    void stop_audio();
    void start_midi();
    void start_osc();
    void persist();
    bool refresh_devices(bool audio = true, bool probe_rates = true, const std::string &target = {});
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
    // Begin rendering only after the hello handshake is flushed.
    void start(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    Json hello() const;
    Json reply(const Request &);
    Json status() const;
    Json telemetry();
    Json devices();
    bool poll(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
};
Options parse_options(int, char **);
} // namespace wirescape::sidecar

