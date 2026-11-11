// SPDX-License-Identifier: GPL-3.0-only
// Device-free / LAN-free OSC network-worker regressions.
#include "../../sdk/examples/test_support.h"
#include "../osc/codec.h"
#include "osc_service.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
using namespace wirescape::osc;
struct Capture {
    std::atomic<uint32_t> count{0};
    std::atomic<uint64_t> timestamp{0};
    static int deliver(void *p, uint32_t node, const ws2_event &e, uint64_t when) noexcept {
        auto &c = *static_cast<Capture *>(p);
        if (node == 32) return WS2_INVALID; // exercise a bounded sink rejection
        if (node == 1 || node == 2) {
            if (e.type == WS2_MESSAGE && e.message_kind == WS2_MSG_OSC) {
                c.timestamp.store(when, std::memory_order_release);
                c.count.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return WS2_OK;
    }
};
int main() {
    Capture c;
    // Unconfirmed LAN never binds any interface other than loopback.
    Service local({&c, Capture::deliver});
    REQUIRE(local.add({1, 0, true}) && local.add({2, 0, false}) && local.start());
    REQUIRE(local.counters().sockets == 1);
    auto state = local.states();
    REQUIRE(state.size() == 2 && std::strcmp(state[0].state, "lan-not-confirmed") == 0);
    REQUIRE(!state[0].lan_bound && !state[1].lan_bound);
    Message m;
    m.address = "/pico1/knob/3";
    m.count = 1;
    m.arguments[0].type = 'f';
    m.arguments[0].real = .5f;
    uint8_t bytes[1472]{};
    bool truncated = false;
    auto n = encode(m, bytes, sizeof(bytes), truncated);
    REQUIRE(n > 0 && !truncated);
    auto sender = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(local.bound_port());
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto before = std::chrono::steady_clock::now();
    const int sent = sendto(sender, reinterpret_cast<const char *>(bytes), int(n), 0,
                            reinterpret_cast<sockaddr *>(&address), sizeof(address));
    REQUIRE(sent == int(n));
    uint32_t attempts = 0;
    while (c.count.load() < 2 && attempts++ < 2000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto after = std::chrono::steady_clock::now();
    state = local.states();
    REQUIRE(c.count == 2 && c.timestamp != 0);
    const auto before_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(before.time_since_epoch()).count();
    const auto after_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(after.time_since_epoch()).count();
    REQUIRE(c.timestamp >= uint64_t(before_ns));
    REQUIRE(c.timestamp <= uint64_t(after_ns));
    REQUIRE(state[0].received == 1 && state[1].received == 1);
    REQUIRE(state[0].dropped == 0 && state[1].dropped == 0);
    // Occupied localhost ports do not fail service startup or accept unwanted data.
    Service busy({&c, Capture::deliver});
    REQUIRE(busy.add({3, local.bound_port(), false}) && busy.start());
    REQUIRE(busy.counters().sockets == 0 && std::strcmp(busy.states()[0].state, "port-busy") == 0);
    busy.stop();
#ifdef _WIN32
    closesocket(sender);
#else
    close(sender);
#endif
    local.stop();
    REQUIRE(local.counters().sockets == 0);
    // All 32 routes share one loopback socket. The 33rd is refused without
    // widening the port set; malformed/oversized packets never reach the DSP.
    Service bounded({&c, Capture::deliver});
    for (uint32_t node = 1; node <= 32; ++node) REQUIRE(bounded.add({node, 0, false}));
    REQUIRE(!bounded.add({33, 0, false}));
    REQUIRE(bounded.start() && bounded.counters().sockets == 1);
    REQUIRE(bounded.states().size() == 32);
    auto flood_sender = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in flood_address{};
    flood_address.sin_family = AF_INET;
    flood_address.sin_port = htons(bounded.bound_port());
    flood_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const std::array<uint8_t, 8> malformed{{1, 2, 3, 4, 5, 6, 7, 8}};
    const std::array<uint8_t, 2052> oversized{};
    REQUIRE(sendto(flood_sender, reinterpret_cast<const char *>(bytes), int(n), 0,
                   reinterpret_cast<sockaddr *>(&flood_address), sizeof(flood_address)) == int(n));
    REQUIRE(sendto(flood_sender, reinterpret_cast<const char *>(malformed.data()), int(malformed.size()), 0,
                   reinterpret_cast<sockaddr *>(&flood_address), sizeof(flood_address)) == int(malformed.size()));
    REQUIRE(sendto(flood_sender, reinterpret_cast<const char *>(oversized.data()), int(oversized.size()), 0,
                   reinterpret_cast<sockaddr *>(&flood_address), sizeof(flood_address)) == int(oversized.size()));
    for (uint32_t wait = 0; wait < 2000 && bounded.counters().received < 3; ++wait)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    REQUIRE(bounded.counters().received == 3);
    REQUIRE(bounded.counters().malformed == 1);
    REQUIRE(bounded.counters().dropped >= 2); // one oversized + one rejected enqueue
    const auto bounded_states = bounded.states();
    REQUIRE(bounded_states[0].received == 1 && bounded_states[30].received == 1);
    REQUIRE(bounded_states[31].received == 0 && bounded_states[31].dropped == 1);
#ifdef _WIN32
    closesocket(flood_sender);
#else
    close(flood_sender);
#endif
    bounded.stop();
    REQUIRE(bounded.counters().sockets == 0);
    // LAN/localhost conflicts stay isolated even with explicit permission:
    // no test ever opens a LAN socket.
    Service mismatch({&c, Capture::deliver}, true);
    REQUIRE(mismatch.add({1, 9100, false}) && mismatch.add({2, 9100, true}));
    REQUIRE(std::strcmp(mismatch.states()[1].state, "port-busy") == 0);
    REQUIRE(mismatch.counters().sockets == 0);
}
