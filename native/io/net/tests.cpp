// SPDX-License-Identifier: GPL-3.0-only
#include "osc_service.h"
#include "../osc/codec.h"
#include "../../sdk/examples/test_support.h"
#include <atomic>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
using namespace wirescape::osc;
struct Output { std::atomic<uint32_t> count{0}; static int deliver(void* p, uint32_t node, const ws2_event& e, uint64_t) noexcept { if (e.type == WS2_MESSAGE && e.message_kind == WS2_MSG_OSC && (node == 1 || node == 2)) ++static_cast<Output*>(p)->count; return WS2_OK; } };
int main() {
    Output output; Service denied({&output, Output::deliver}); REQUIRE(!denied.add({1, 9000, true}) && denied.counters().sockets == 0);
    Service conflict({&output, Output::deliver}, true); REQUIRE(conflict.add({1, 9000, false}) && !conflict.add({2, 9000, true})); // no socket started
    Service service({&output, Output::deliver}); REQUIRE(service.add({1, 0, false}) && service.add({2, 0, false}) && service.start()); REQUIRE(service.counters().sockets == 1);
    Message m; m.address = "/pico1/knob/3"; m.count = 1; m.arguments[0].type = 'f'; m.arguments[0].real = .5f; uint8_t bytes[1472]{}; bool truncated = false; size_t n = encode(m, bytes, sizeof(bytes), truncated);
    auto socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(service.bound_port()); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    REQUIRE(sendto(socket, reinterpret_cast<const char*>(bytes), int(n), 0, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == int(n));
    for (int i = 0; i < 2000 && output.count.load() < 2; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
    service.stop(); REQUIRE(output.count == 2 && service.counters().received == 1 && service.counters().sockets == 0);
}
