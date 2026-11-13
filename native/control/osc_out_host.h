// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../modules2/osc_out/config.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace wirescape::osc {
struct Message;
// Test-only deterministic send seam; production always uses the loopback socket.
using SendOverride = bool (*)(const Message &, uint16_t);
struct OutState {
    uint32_t node = 0, port = 0;
    const char *state = "disabled"; // sending | disabled | error
    uint64_t sent = 0, dropped = 0;
    const char *last_error = nullptr;
};

// One quiescently configured, control-owned sender per prepared graph.
// All buses outlive the module instances; the worker never touches AppEngine.
class OutHost {
    struct RowCursor {
        uint32_t rises = 0, falls = 0;
        float previous = 0;
        bool initialized = false, gate_high = false, retry = false;
        std::chrono::steady_clock::time_point last_send{};
    };
    struct Route {
        uint32_t node = 0;
        OutConfiguration config{};
        std::unique_ptr<OutBus> bus = std::make_unique<OutBus>();
        std::array<RowCursor, 8> cursors{};
        std::atomic<uint64_t> sent{0}, dropped{0};
        std::atomic<uint32_t> error{0};
        bool enabled = false;
    };
    std::vector<std::unique_ptr<Route>> routes_;
    std::map<uint32_t, OutBus *> buses_;
    std::atomic<bool> running_{false};
    std::thread worker_;
    std::atomic<uint32_t> socket_error_{0};
    SendOverride send_override_ = nullptr;

    void loop() noexcept;

  public:
    explicit OutHost(const std::string &document, SendOverride send_override = nullptr);
    ~OutHost() { stop(); }
    OutHost(const OutHost &) = delete;
    OutHost &operator=(const OutHost &) = delete;
    const std::map<uint32_t, OutBus *> &buses() const noexcept { return buses_; }
    void start() noexcept;
    void stop() noexcept;
    std::vector<OutState> states() const;
};
} // namespace wirescape::osc
