// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "midi_backend.h"
#include "wirescape/module2.h"
#include <array>
#include <atomic>
#include <thread>
namespace wirescape::midi {
struct Selection { std::string name, system_name; }; // both saved, never enumeration indices
struct Route { uint32_t node = 0; Selection endpoint; bool all_enabled = true; };
struct Sink {
    void* context = nullptr;
    // Timestamp is SystemMonotonic nanoseconds. The host maps it through engine D13.
    int (*emit)(void*, uint32_t node, const ws2_event&, uint64_t timestamp_ns) noexcept = nullptr;
};
struct Counters { uint64_t received = 0, dropped = 0, malformed = 0, oversize = 0, ring_full = 0, disconnects = 0, send_fail = 0; };
struct EndpointState { Selection saved; uint32_t source_id = 0; bool online = false; };
// All configuration is quiescent. Worker owns enumeration/pump; callbacks only push
// their own SPSC ring. Stop joins producers and drains releases before host teardown.
class Service {
public:
    Service(Backend&, Sink);
    ~Service();
    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;
    bool enable(Selection);
    bool subscribe(Route);
    bool start();
    void stop();
    void poll(); // deterministic fake-backend tests; never call while start() is active
    Counters counters() const noexcept;
    std::vector<EndpointState> endpoint_states() const; // immutable names + atomic status
private:
    struct Packet { ws2_event event{}; uint64_t timestamp = 0; };
    struct Ring {
        static constexpr uint32_t capacity = 4096, reserve = 256;
        std::array<Packet, capacity> packets{};
        std::atomic<uint32_t> write{0}, read{0};
        bool push(const Packet&, bool release) noexcept;
        bool pop(Packet&) noexcept;
    };
    struct Slot {
        Service* owner = nullptr; uint32_t source = 0;
        Selection saved; Endpoint actual;
        Ring ring;
        std::atomic<bool> accepting{false}, online{false}, emergency{false};
    };
    Backend& backend_;
    Sink sink_;
    std::array<std::unique_ptr<Slot>, 16> slots_{};
    std::array<Route, 64> routes_{};
    size_t count_ = 0, route_count_ = 0;
    std::atomic<bool> running_{false}; std::thread worker_;
    std::atomic<uint64_t> received_{0}, dropped_{0}, malformed_{0}, oversize_{0}, ring_full_{0}, disconnects_{0}, send_fail_{0};
    static void midi1(void*, const uint8_t*, size_t, uint64_t);
    static void ump(void*, const uint32_t*, size_t, uint64_t);
    void refresh();
    void pump();
    void send(const Slot&, const Packet&) noexcept;
};
}
