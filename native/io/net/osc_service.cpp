// SPDX-License-Identifier: GPL-3.0-only
#include "osc_service.h"
#include "../osc/codec.h"
#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#endif
namespace wirescape::osc {
#ifdef _WIN32
using Socket = SOCKET; constexpr Socket invalid_socket = INVALID_SOCKET;
static void close_socket(Socket s) { closesocket(s); }
#else
using Socket = int; constexpr Socket invalid_socket = -1;
static void close_socket(Socket s) { close(s); }
#endif
struct Service::Impl {
    struct Listener { uint16_t requested = 0, bound = 0; bool lan = false; Socket socket = invalid_socket; };
    Sink sink; bool confirmed; std::string failure;
    std::array<Route, 64> routes{}; uint32_t route_count = 0;
    std::array<Listener, 16> listeners{}; uint32_t listener_count = 0;
    std::array<uint8_t, 65507> packet{}; std::array<uint8_t, WS2_MAX_PAYLOAD> normalized{};
    std::atomic<uint64_t> received{0}, malformed{0}, dropped{0}, truncated{0};
    std::atomic<uint32_t> sockets{0}; std::atomic<bool> running{false}; std::thread worker;
#ifdef _WIN32
    bool winsock = false;
#endif
    Impl(Sink s, bool confirm) : sink(s), confirmed(confirm) {}
    bool add(Route route) {
        if (running || route_count == routes.size() || !route.node) { failure = "Invalid OSC route or service already running"; return false; }
        if (route.lan && !confirmed) { failure = "LAN requires the module toggle and --confirm-lan/user confirmation"; return false; }
        uint32_t index = listener_count;
        for (uint32_t i = 0; i < listener_count; ++i) if (listeners[i].requested == route.port) { index = i; break; }
        if (index < listener_count && listeners[index].lan != route.lan) { failure = "Conflicting LAN settings on shared OSC port"; return false; }
        if (index == listener_count) { if (listener_count == listeners.size()) { failure = "Too many OSC ports"; return false; } listeners[listener_count++] = {route.port, 0, route.lan, invalid_socket}; }
        routes[route_count++] = route; return true;
    }
    void close_all() {
        for (uint32_t i = 0; i < listener_count; ++i) if (listeners[i].socket != invalid_socket) { close_socket(listeners[i].socket); listeners[i].socket = invalid_socket; }
        sockets = 0;
#ifdef _WIN32
        if (winsock) { WSACleanup(); winsock = false; }
#endif
    }
    bool start() {
        if (running) return false;
#ifdef _WIN32
        WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data) != 0) { failure = "Winsock startup failed"; return false; } winsock = true;
#endif
        for (uint32_t i = 0; i < listener_count; ++i) {
            auto& listener = listeners[i]; listener.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (listener.socket == invalid_socket) { failure = "OSC socket creation failed"; close_all(); return false; }
#ifdef _WIN32
            u_long nonblocking = 1; if (ioctlsocket(listener.socket, FIONBIO, &nonblocking) != 0) { failure = "OSC nonblocking setup failed"; close_all(); return false; }
#else
            const int flags = fcntl(listener.socket, F_GETFL, 0); if (flags < 0 || fcntl(listener.socket, F_SETFL, flags | O_NONBLOCK) < 0) { failure = "OSC nonblocking setup failed"; close_all(); return false; }
#endif
            sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(listener.requested); address.sin_addr.s_addr = htonl(listener.lan ? INADDR_ANY : INADDR_LOOPBACK);
            if (bind(listener.socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) { failure = "OSC port bind failed (busy or unavailable)"; close_all(); return false; }
#ifdef _WIN32
            int length = sizeof(address);
#else
            socklen_t length = sizeof(address);
#endif
            if (getsockname(listener.socket, reinterpret_cast<sockaddr*>(&address), &length) != 0) { failure = "OSC port query failed"; close_all(); return false; }
            listener.bound = ntohs(address.sin_port); ++sockets;
        }
        running = true;
        try { worker = std::thread([this] { loop(); }); } catch (...) { running = false; close_all(); failure = "OSC worker startup failed"; return false; }
        return true;
    }
    void loop() noexcept {
#ifdef _WIN32
        std::array<WSAPOLLFD, 16> descriptors{};
#else
        std::array<pollfd, 16> descriptors{};
#endif
        for (uint32_t i = 0; i < listener_count; ++i) { descriptors[i].fd = listeners[i].socket; descriptors[i].events = POLLIN; }
        while (running.load(std::memory_order_acquire)) {
#ifdef _WIN32
            const int ready = WSAPoll(descriptors.data(), listener_count, 10);
#else
            const int ready = poll(descriptors.data(), listener_count, 10);
#endif
            if (ready <= 0) { if (!listener_count) std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            for (uint32_t i = 0; i < listener_count; ++i) if (descriptors[i].revents & POLLIN) {
                for (uint32_t packets = 0; packets < 64 && running.load(std::memory_order_relaxed); ++packets) {
                    const auto length = recv(listeners[i].socket, reinterpret_cast<char*>(packet.data()), int(packet.size()), 0); if (length <= 0) break;
                    ++received;
                    if (length > 2048) { ++dropped; continue; } // J default packet cap; whole datagram consumed safely
                    if (!parse(packet.data(), size_t(length), [](const Message&) {})) { ++malformed; continue; }
                    const auto timestamp = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
                    parse(packet.data(), size_t(length), [&](const Message& message) {
                        bool shortened = false; const auto bytes = encode(message, normalized.data(), normalized.size(), shortened); if (!bytes) { ++dropped; return; } if (shortened) ++truncated;
                        ws2_event event{}; event.struct_size = sizeof(event); event.type = WS2_MESSAGE; event.message_kind = WS2_MSG_OSC; event.source_id = i + 1; event.note_id = event.channel = -1; event.data = normalized.data(); event.bytes = uint32_t(bytes);
                        for (uint32_t r = 0; r < route_count; ++r) if (routes[r].port == listeners[i].requested) if (!sink.deliver || sink.deliver(sink.context, routes[r].node, event, timestamp) != WS2_OK) ++dropped;
                    });
                }
            }
        }
    }
    void stop() { running = false; if (worker.joinable()) worker.join(); close_all(); }
};
Service::Service(Sink sink, bool confirmed) : impl_(std::make_unique<Impl>(sink, confirmed)) {}
Service::~Service() { stop(); }
bool Service::add(Route r) { return impl_->add(r); }
bool Service::start() { return impl_->start(); }
void Service::stop() { impl_->stop(); }
const std::string& Service::error() const { return impl_->failure; }
Counters Service::counters() const noexcept { return {impl_->received, impl_->malformed, impl_->dropped, impl_->truncated, impl_->sockets}; }
uint16_t Service::bound_port(uint32_t i) const { return i < impl_->listener_count ? impl_->listeners[i].bound : 0; }
}
