#ifndef WIRESCAPE_PROVIDER_BRIDGE_H
#define WIRESCAPE_PROVIDER_BRIDGE_H

#include "wirescape/engine.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace wirescape {

// Offline proof only. Every method, including submit(), belongs to one serialized
// control owner. A provider worker must transfer input through its own bounded,
// correctly synchronized ingress before calling this class on that owner.
class ProviderBridge final {
public:
    static constexpr std::size_t capacity = 128;
    struct Config {
        uint32_t sample_rate_hz;
        uint32_t max_block_frames;
        uint64_t horizon_frames;
        uint32_t max_per_flush;
        uint64_t epoch_ns;
        uint64_t epoch_frame;
    };
    struct Input {
        uint64_t timestamp_ns;
        uint32_t node_id, type, parameter_id;
        float value;
        uint32_t source_id, note_id;
    };
    struct Counts {
        uint64_t coalesced = 0, late_control = 0, stale_note_on = 0;
        uint64_t loss_panics = 0, generation_stops = 0, backpressure = 0;
        uint64_t forwarded = 0;
    };
    enum class Status { active, stopped, invalid_config };

    ProviderBridge(ws_engine *engine, Config config) noexcept;
    bool submit(const Input &input) noexcept;
    uint32_t flush() noexcept;
    // The caller first stops producers and the render consumer, performs an
    // ACTIVE render barrier for any panic, then establishes a fresh epoch.
    bool resume_after_quiescent_barrier(Config config) noexcept;
    Status status() const noexcept { return status_; }
    std::size_t pending() const noexcept { return size_; }
    Counts counts() const noexcept { return counts_; }
    uint64_t observed_generation() const noexcept { return generation_; }

private:
    struct Pending { ws_event event; uint64_t serial; };
    bool valid_config(Config config) const noexcept;
    bool convert(uint64_t ns, uint64_t &frame) const noexcept;
    void insert(Pending pending) noexcept;
    void stop_generation() noexcept;
    void stop_with_panic() noexcept;
    bool generation_matches() noexcept;

    ws_engine *engine_;
    Config config_;
    std::array<Pending, capacity> pending_{};
    std::size_t size_ = 0;
    uint64_t serial_ = 0;
    uint64_t generation_ = 0;
    uint64_t last_forwarded_ = 0;
    bool forwarded_any_ = false;
    Status status_ = Status::invalid_config;
    Counts counts_{};
};

} // namespace wirescape
#endif
