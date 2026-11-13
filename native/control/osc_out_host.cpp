// SPDX-License-Identifier: GPL-3.0-only
#include "osc_out_host.h"
#include "../document/runtime_module_config.h"
#include "../io/osc/codec.h"
#include <chrono>
#include <cmath>
#include <cstring>
#include <nlohmann/json.hpp>
#include <stdexcept>
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
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace wirescape::osc {
namespace {
#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket invalid_socket = INVALID_SOCKET;
void close_socket(Socket s) noexcept { closesocket(s); }
#else
using Socket = int;
constexpr Socket invalid_socket = -1;
void close_socket(Socket s) noexcept { close(s); }
#endif
// Only the sender thread performs network operations; all endpoints are loopback.
bool send_message(Socket sock, uint16_t port, const Message &message) noexcept {
    std::array<uint8_t, 1472> buffer{};
    bool truncated = false;
    const auto size = encode(message, buffer.data(), buffer.size(), truncated);
    if (!size || truncated) return false;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int result = sendto(sock, reinterpret_cast<const char *>(buffer.data()), int(size), 0,
                              reinterpret_cast<const sockaddr *>(&address), sizeof(address));
    return result == int(size);
}
} // namespace

OutHost::OutHost(const std::string &document) {
    const auto patch = nlohmann::json::parse(document);
    for (const auto &module : patch.at("modules")) {
        if (module.value("module", std::string{}) != "osc.out") continue;
        if (routes_.size() >= 32) throw std::runtime_error("At most 32 OSC output routes");
        auto route = std::make_unique<Route>();
        route->node = module.at("id").get<uint32_t>();
        const auto config = module.value("config", nlohmann::json::object());
        const auto bytes = document::runtime_module_config("osc.out", config, route->node);
        if (bytes.size() != sizeof(route->config))
            throw std::runtime_error("OSC Out configuration was not prepared");
        std::memcpy(&route->config, bytes.data(), sizeof(route->config));
        route->config.bus = route->bus.get();
        for (const auto &row : route->config.rows) route->enabled |= row.enabled != 0;
        route->enabled |= route->config.notes.enabled != 0;
        if (!buses_.emplace(route->node, route->bus.get()).second)
            throw std::runtime_error("Duplicate OSC output module ID");
        routes_.push_back(std::move(route));
    }
}
void OutHost::start() noexcept {
    if (worker_.joinable() || routes_.empty()) return;
    bool enabled = false;
    for (const auto &route : routes_) enabled |= route->enabled;
    if (!enabled) return; // disabled modules must not open a socket
    socket_error_.store(0);
    running_.store(true, std::memory_order_release);
    try {
        worker_ = std::thread([this] { loop(); });
    } catch (...) {
        running_.store(false, std::memory_order_release);
        socket_error_.store(1, std::memory_order_release);
    }
}
void OutHost::stop() noexcept {
    running_.store(false, std::memory_order_release);
    if (worker_.joinable()) worker_.join();
}
void OutHost::loop() noexcept {
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        socket_error_.store(1, std::memory_order_release);
        return;
    }
#endif
    Socket sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    bool ready = sock != invalid_socket;
    if (ready) {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = 0;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ready = bind(sock, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) == 0;
    }
    if (ready) {
#ifdef _WIN32
        u_long nonblocking = 1;
        ready = ioctlsocket(sock, FIONBIO, &nonblocking) == 0;
#else
        const int flags = fcntl(sock, F_GETFL, 0);
        ready = flags >= 0 && fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
    }
    if (!ready) {
        socket_error_.store(1, std::memory_order_release);
        if (sock != invalid_socket) close_socket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }
    auto send = [&](Route &route, const char *path, uint32_t count, const Argument *args) {
        Message message{};
        message.address = path;
        message.count = count;
        for (uint32_t a = 0; a < count; ++a) message.arguments[a] = args[a];
        if (send_message(sock, uint16_t(route.config.port), message))
            route.sent.fetch_add(1, std::memory_order_relaxed);
        else {
            route.dropped.fetch_add(1, std::memory_order_relaxed);
            route.error.store(1, std::memory_order_relaxed);
        }
    };
    while (running_.load(std::memory_order_acquire)) {
        const auto now = std::chrono::steady_clock::now();
        for (const auto &item : routes_) {
            auto &route = *item;
            if (!route.enabled) continue;
            for (size_t i = 0; i < route.config.rows.size(); ++i) {
                const auto &cfg = route.config.rows[i];
                if (!cfg.enabled) continue;
                const auto &row = route.bus->rows[i];
                auto &cursor = route.cursors[i];
                const auto blocks = row.blocks.load(std::memory_order_acquire);
                if (!blocks) continue; // no rendered block yet
                if (cfg.kind == 1) {
                    // Counters retain even a rise and fall inside one block. A gate
                    // alternates, so the preceding level determines the edge order.
                    const auto rises = row.rises.load(std::memory_order_relaxed);
                    const auto falls = row.falls.load(std::memory_order_relaxed);
                    const bool pending = cursor.rises != rises || cursor.falls != falls;
                    if (!cursor.initialized && !pending) {
                        Argument arg{};
                        arg.type = 'f';
                        arg.real = row.value.load(std::memory_order_relaxed) >= 0.5f ? 1.0f : 0.0f;
                        send(route, cfg.address, 1, &arg);
                        cursor.gate_high = arg.real >= 0.5f;
                    }
                    for (uint32_t edge = 0; edge < 64; ++edge) {
                        if (cursor.rises == rises && cursor.falls == falls) break;
                        const bool next_high = !cursor.gate_high;
                        if (next_high && cursor.rises == rises) break;
                        if (!next_high && cursor.falls == falls) break;
                        Argument arg{};
                        arg.type = 'f';
                        arg.real = next_high ? 1.0f : 0.0f;
                        send(route, cfg.address, 1, &arg);
                        cursor.gate_high = next_high;
                        if (next_high) ++cursor.rises;
                        else ++cursor.falls;
                    }
                    cursor.initialized = true;
                } else {
                    const float value = row.value.load(std::memory_order_relaxed);
                    const auto interval = std::chrono::duration<double>(1.0 / route.config.rate_hz);
                    if ((!cursor.initialized || std::abs(value - cursor.previous) > cfg.deadband) &&
                        (!cursor.initialized || now - cursor.last_send >= interval)) {
                        Argument arg{};
                        arg.type = 'f';
                        arg.real = value;
                        send(route, cfg.address, 1, &arg);
                        cursor.previous = value;
                        cursor.last_send = now;
                        cursor.initialized = true;
                    }
                }
                cursor.blocks = blocks;
            }
            if (route.config.notes.enabled) {
                // SPSC: render owns head and writes the event before release; this
                // worker owns tail and publishes consumption only after encoding.
                const auto head = route.bus->note_head.load(std::memory_order_acquire);
                auto tail = route.bus->note_tail.load(std::memory_order_relaxed);
                for (uint32_t n = 0; n < 64 && tail != head; ++n) {
                    const auto event = route.bus->notes[tail % route.bus->notes.size()];
                    Argument args[3]{};
                    args[0].type = 'i';
                    args[0].integer = event.key;
                    args[1].type = 'f';
                    args[1].real = event.velocity;
                    args[2].type = 'i';
                    args[2].integer = event.channel;
                    send(route, route.config.notes.address, 3, args);
                    ++tail;
                    route.bus->note_tail.store(tail, std::memory_order_release);
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    close_socket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
}
std::vector<OutState> OutHost::states() const {
    std::vector<OutState> result;
    result.reserve(routes_.size());
    for (const auto &item : routes_) {
        const auto &route = *item;
        OutState s;
        s.node = route.node;
        s.port = route.config.port;
        s.state = !route.enabled ? "disabled" : socket_error_.load(std::memory_order_acquire) ? "error" : "sending";
        s.sent = route.sent.load(std::memory_order_relaxed);
        s.dropped = route.dropped.load(std::memory_order_relaxed) +
                    route.bus->dropped.load(std::memory_order_relaxed);
        if (socket_error_.load(std::memory_order_relaxed)) s.last_error = "socket-error";
        else if (route.error.load(std::memory_order_relaxed)) s.last_error = "send-error";
        result.push_back(s);
    }
    return result;
}
} // namespace wirescape::osc
