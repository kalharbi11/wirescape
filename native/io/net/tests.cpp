// SPDX-License-Identifier: GPL-3.0-only
// Device-free / LAN-free OSC network-worker regressions.
#include "../../sdk/examples/test_support.h"
#include "../osc/codec.h"
#include "osc_service.h"
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
    for (uint32_t i = 0; i < 2000 && c.count.load() < 2; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
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
    // LAN/localhost conflicts stay isolated even with explicit permission:
    // no test ever opens a LAN socket.
    Service mismatch({&c, Capture::deliver}, true);
    REQUIRE(mismatch.add({1, 9100, false}) && mismatch.add({2, 9100, true}));
    REQUIRE(std::strcmp(mismatch.states()[1].state, "port-busy") == 0);
    REQUIRE(mismatch.counters().sockets == 0);
}
