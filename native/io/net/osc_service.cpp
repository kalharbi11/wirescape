// SPDX-License-Identifier: GPL-3.0-only
#include "osc_service.h"
#include "../osc/codec.h"
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace wirescape::osc {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
static void close_socket(Socket s) { closesocket(s); }
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
static void close_socket(Socket s) { close(s); }
#endif
struct Service::Impl {
    struct Listener {
        uint16_t requested = 0, bound = 0;
        bool lan = false;
        Socket socket = invalid_socket;
    };
    Sink sink;
    bool confirmed;
    std::string failure;
    std::array<Route, 32> routes{};
    std::array<bool, 32> route_enabled{};
    std::array<std::atomic<uint64_t>, 32> route_received{}, route_dropped{};
    uint32_t route_count = 0;
    std::array<Listener, 32> listeners{};
    uint32_t listener_count = 0;
    std::array<uint8_t, 65507> packet{};
    std::array<uint8_t, WS2_MAX_PAYLOAD> normalized{};
    std::atomic<uint64_t> received{0}, malformed{0}, dropped{0}, truncated{0};
    std::atomic<uint32_t> sockets{0};
    std::atomic<bool> running{false};
    std::thread worker;
#ifdef _WIN32
    bool winsock = false;
#endif
    Impl(Sink s, bool confirm) : sink(s), confirmed(confirm) {}
    bool add(Route route) {
        if (running || route_count == routes.size() || !route.node) {
            failure = "Invalid OSC route or service already running";
            return false;
        }
        // One physical UDP port cannot be safely shared between a LAN-enabled
        // and a localhost-only route. Never broaden a route's bind permission.
        const bool bound_lan = route.lan && confirmed;
        uint32_t listener = listener_count;
        for (uint32_t i = 0; i < listener_count; ++i)
            if (listeners[i].requested == route.port) { listener = i; break; }
        bool enabled = true;
        if (listener < listener_count && listeners[listener].lan != bound_lan) {
            enabled = false; // reported as port-busy; the patch itself still loads
        } else if (listener == listener_count) {
            listeners[listener_count++] = {route.port, 0, bound_lan, invalid_socket};
        }
        routes[route_count] = route;
        route_enabled[route_count++] = enabled;
        return true;
    }
    void close_all() {
        for (uint32_t i = 0; i < listener_count; ++i)
            if (listeners[i].socket != invalid_socket) {
                close_socket(listeners[i].socket);
                listeners[i].socket = invalid_socket;
            }
        sockets = 0;
#ifdef _WIN32
        if (winsock) {
            WSACleanup();
            winsock = false;
        }
#endif
    }
    bool start() {
        if (running) return false;
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            failure = "Winsock startup failed";
            return true; // unavailable ports are observations, never patch refusals
        }
        winsock = true;
#endif
        for (uint32_t i = 0; i < listener_count; ++i) {
            auto &listener = listeners[i];
            listener.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (listener.socket == invalid_socket) continue;
#ifdef _WIN32
            u_long nonblocking = 1;
            if (ioctlsocket(listener.socket, FIONBIO, &nonblocking) != 0) {
                close_socket(listener.socket); listener.socket = invalid_socket; continue;
            }
#else
            const int flags = fcntl(listener.socket, F_GETFL, 0);
            if (flags < 0 || fcntl(listener.socket, F_SETFL, flags | O_NONBLOCK) < 0) {
                close_socket(listener.socket); listener.socket = invalid_socket; continue;
            }
#endif
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(listener.requested);
            address.sin_addr.s_addr = htonl(listener.lan ? INADDR_ANY : INADDR_LOOPBACK);
            if (bind(listener.socket, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
                close_socket(listener.socket); listener.socket = invalid_socket; continue;
            }
#ifdef _WIN32
            int length = sizeof(address);
#else
            socklen_t length = sizeof(address);
#endif
            if (getsockname(listener.socket, reinterpret_cast<sockaddr *>(&address), &length) != 0) {
                close_socket(listener.socket); listener.socket = invalid_socket; continue;
            }
            listener.bound = ntohs(address.sin_port);
            sockets.fetch_add(1, std::memory_order_relaxed);
        }
        if (!sockets.load(std::memory_order_relaxed)) return true;
        running = true;
        try {
            worker = std::thread([this] { loop(); });
        } catch (...) {
            running = false;
            close_all();
            failure = "OSC worker startup failed";
            return true; // fail closed, leave the accepted patch and inactive routes
        }
        return true;
    }
    void loop() noexcept {
#ifdef _WIN32
        std::array<WSAPOLLFD, 32> descriptors{};
#else
        std::array<pollfd, 32> descriptors{};
#endif
        std::array<uint32_t, 32> indices{};
        uint32_t count = 0;
        for (uint32_t i = 0; i < listener_count; ++i) {
            if (listeners[i].socket == invalid_socket) continue;
            indices[count] = i;
            descriptors[count].fd = listeners[i].socket;
            descriptors[count].events = POLLIN;
            ++count;
        }
        while (running.load(std::memory_order_acquire)) {
#ifdef _WIN32
            const int ready = WSAPoll(descriptors.data(), count, 10);
#else
            const int ready = poll(descriptors.data(), count, 10);
#endif
            if (ready <= 0) continue;
            for (uint32_t j = 0; j < count; ++j) {
                if (!(descriptors[j].revents & POLLIN)) continue;
                const uint32_t i = indices[j];
                for (uint32_t packets = 0; packets < 64 && running.load(std::memory_order_relaxed); ++packets) {
                    const auto length = recv(listeners[i].socket, reinterpret_cast<char *>(packet.data()),
                                             int(packet.size()), 0);
                    if (length <= 0) break;
                    received.fetch_add(1, std::memory_order_relaxed);
                    if (length > 2048) {
                        dropped.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }
                    if (!parse(packet.data(), size_t(length), [](const Message &) {})) {
                        malformed.fetch_add(1, std::memory_order_relaxed);
                        continue;
                    }
                    const auto timestamp = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count());
                    parse(packet.data(), size_t(length), [&](const Message &message) {
                        bool shortened = false;
                        const auto bytes = encode(message, normalized.data(), normalized.size(), shortened);
                        if (!bytes) { dropped.fetch_add(1, std::memory_order_relaxed); return; }
                        if (shortened) truncated.fetch_add(1, std::memory_order_relaxed);
                        ws2_event event{};
                        event.struct_size = sizeof(event);
                        event.type = WS2_MESSAGE;
                        event.message_kind = WS2_MSG_OSC;
                        event.source_id = i + 1;
                        event.note_id = event.channel = -1;
                        event.data = normalized.data();
                        event.bytes = uint32_t(bytes);
                        for (uint32_t r = 0; r < route_count; ++r) {
                            if (!route_enabled[r] || routes[r].port != listeners[i].requested) continue;
                            if (!sink.deliver || sink.deliver(sink.context, routes[r].node, event, timestamp) != WS2_OK) {
                                dropped.fetch_add(1, std::memory_order_relaxed);
                                route_dropped[r].fetch_add(1, std::memory_order_relaxed);
                            } else {
                                route_received[r].fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                    });
                }
            }
        }
    }
    void stop() {
        running = false;
        if (worker.joinable()) worker.join();
        close_all();
    }
};
Service::Service(Sink sink, bool confirmed) : impl_(std::make_unique<Impl>(sink, confirmed)) {}
Service::~Service() { stop(); }
bool Service::add(Route r) { return impl_->add(r); }
bool Service::start() { return impl_->start(); }
void Service::stop() { impl_->stop(); }
const std::string &Service::error() const { return impl_->failure; }
std::vector<RouteState> Service::states() const {
    std::vector<RouteState> out;
    out.reserve(impl_->route_count);
    for (uint32_t i = 0; i < impl_->route_count; ++i) {
        const auto &route = impl_->routes[i];
        const auto effective_lan = route.lan && impl_->confirmed;
        const Impl::Listener *listener = nullptr;
        for (uint32_t j = 0; j < impl_->listener_count; ++j)
            if (impl_->listeners[j].requested == route.port &&
                impl_->listeners[j].lan == effective_lan) { listener = &impl_->listeners[j]; break; }
        const bool listening = impl_->route_enabled[i] && listener && listener->socket != invalid_socket;
        out.push_back({route.node, route.port, route.lan, effective_lan,
                       !listening ? "port-busy" : route.lan && !impl_->confirmed ? "lan-not-confirmed" : "listening",
                       impl_->route_received[i].load(std::memory_order_relaxed),
                       impl_->route_dropped[i].load(std::memory_order_relaxed)});
    }
    return out;
}
Counters Service::counters() const noexcept {
    return {impl_->received, impl_->malformed, impl_->dropped, impl_->truncated, impl_->sockets};
}
uint16_t Service::bound_port(uint32_t i) const { return i < impl_->listener_count ? impl_->listeners[i].bound : 0; }
} // namespace wirescape::osc
