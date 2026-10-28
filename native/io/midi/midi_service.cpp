// SPDX-License-Identifier: GPL-3.0-only
#include "midi_service.h"
#include <algorithm>
#include <chrono>
namespace wirescape::midi {
namespace {
uint64_t now_ns() { return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
// Same semantics as engine D8 release classification; replaced by shared helper on integration.
bool release(const ws2_event& e) {
    if (e.type == WS2_NOTE_CHOKE || e.type == WS2_NOTE_OFF) return true;
    if (e.type != WS2_MIDI || !e.ump_words) return false;
    const uint32_t type = e.ump[0] >> 28, status = (e.ump[0] >> 20) & 15, key = (e.ump[0] >> 8) & 127;
    if (type != 2 && type != 4) return false;
    if (status == 8) return true;
    if (type == 2 && status == 9 && !(e.ump[0] & 127)) return true;
    if (status == 11) {
        if (key == 120 || key == 123) return true;
        return key == 64 && (type == 2 ? (e.ump[0] & 127) < 64 : e.ump_words == 2 && e.ump[1] < 0x80000000u);
    }
    return false;
}
}
bool Service::Ring::push(const Packet& packet, bool critical) noexcept {
    const auto w = write.load(std::memory_order_relaxed), r = read.load(std::memory_order_acquire);
    if (w - r >= capacity - (critical ? 0 : reserve)) return false;
    packets[w % capacity] = packet; write.store(w + 1, std::memory_order_release); return true;
}
bool Service::Ring::pop(Packet& packet) noexcept {
    const auto r = read.load(std::memory_order_relaxed), w = write.load(std::memory_order_acquire);
    if (r == w) return false;
    packet = packets[r % capacity]; read.store(r + 1, std::memory_order_release); return true;
}
Service::Service(Backend& backend, Sink sink) : backend_(backend), sink_(sink) {}
Service::~Service() { stop(); }
bool Service::enable(Selection saved) {
    if (running_ || saved.name.empty()) return false;
    for (size_t i = 0; i < count_; ++i) if (slots_[i]->saved.name == saved.name && slots_[i]->saved.system_name == saved.system_name) return true;
    if (count_ == slots_.size()) return false;
    auto slot = std::make_unique<Slot>(); slot->owner = this; slot->source = uint32_t(count_ + 1); slot->saved = std::move(saved); slots_[count_++] = std::move(slot); return true;
}
bool Service::subscribe(Route route) {
    if (running_ || !route.node || route_count_ == routes_.size()) return false;
    routes_[route_count_++] = std::move(route); return true;
}
void Service::midi1(void* context, const uint8_t* bytes, size_t size, uint64_t timestamp) {
    auto& slot = *static_cast<Slot*>(context); auto& service = *slot.owner;
    if (!slot.accepting.load(std::memory_order_acquire)) return;
    if (!bytes || size == 0) { ++service.malformed_; return; }
    const uint8_t status = bytes[0];
    const size_t wanted = status >= 0xf8 ? 1 : ((status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0 ? 2 : 3);
    if (size > 3) { ++service.oversize_; ++service.dropped_; return; }
    if (status < 0x80 || (status >= 0xf0 && status < 0xf8) || size != wanted || (size > 1 && bytes[1] > 127) || (size > 2 && bytes[2] > 127)) { ++service.malformed_; return; }
    const uint32_t word = (status < 0xf0 ? 0x20000000u : 0x10000000u) | uint32_t(status) << 16 | (size > 1 ? uint32_t(bytes[1]) << 8 : 0u) | (size > 2 ? bytes[2] : 0u);
    ump(context, &word, 1, timestamp);
}
void Service::ump(void* context, const uint32_t* words, size_t count, uint64_t timestamp) {
    auto& slot = *static_cast<Slot*>(context); auto& service = *slot.owner;
    if (!slot.accepting.load(std::memory_order_acquire)) return;
    if (!words || count < 1 || count > 4) { ++service.malformed_; return; }
    const uint32_t type = words[0] >> 28;
    const size_t wanted = type <= 2 ? 1 : type <= 4 ? 2 : 4;
    if (count != wanted) { ++service.malformed_; return; }
    Packet packet; packet.timestamp = timestamp; auto& event = packet.event;
    event.struct_size = sizeof(event); event.type = WS2_MIDI; event.source_id = slot.source; event.channel = -1; event.note_id = -1; event.ump_words = uint32_t(count);
    std::copy_n(words, count, event.ump); ++service.received_;
    const bool critical = release(event);
    if (!slot.ring.push(packet, critical)) { ++service.ring_full_; ++service.dropped_; if (critical) slot.emergency.store(true, std::memory_order_release); }
}
void Service::refresh() {
    const auto available = backend_.endpoints();
    for (size_t i = 0; i < count_; ++i) {
        auto& slot = *slots_[i]; const Endpoint* found = nullptr; size_t matches = 0;
        for (const auto& endpoint : available) if (endpoint.name == slot.saved.name) { found = &endpoint; ++matches; }
        if (matches != 1) found = nullptr;
        if (!found && !slot.saved.system_name.empty()) for (const auto& endpoint : available) if (endpoint.system_name == slot.saved.system_name) { found = &endpoint; break; }
        if (slot.online && (!found || found->system_name != slot.actual.system_name)) {
            slot.accepting = false; backend_.close(slot.actual); slot.online = false; slot.emergency = true; ++disconnects_;
        }
        if (!slot.online && found) {
            slot.actual = *found; slot.accepting = true;
            const bool opened = backend_.open(*found, Callback{&slot, midi1, ump}); slot.online = opened;
            if (!opened) { slot.accepting = false; ++send_fail_; }
        }
    }
}
void Service::send(const Slot& slot, const Packet& packet) noexcept {
    for (size_t i = 0; i < route_count_; ++i) {
        const auto& route = routes_[i];
        const bool matching = route.all_enabled || route.endpoint.name == slot.saved.name || (!route.endpoint.system_name.empty() && route.endpoint.system_name == slot.saved.system_name);
        if (!matching) continue;
        if (!sink_.emit || sink_.emit(sink_.context, route.node, packet.event, packet.timestamp) != WS2_OK) ++send_fail_;
    }
}
void Service::pump() {
    for (size_t i = 0; i < count_; ++i) {
        auto& slot = *slots_[i]; Packet packet;
        if (slot.emergency.exchange(false, std::memory_order_acq_rel)) {
            for (uint32_t n = 0; n < Ring::capacity && slot.ring.pop(packet); ++n) ++dropped_;
            packet = {}; packet.timestamp = now_ns(); packet.event.struct_size = sizeof(ws2_event); packet.event.type = WS2_NOTE_CHOKE; packet.event.source_id = slot.source; packet.event.channel = -1; packet.event.note_id = -1; send(slot, packet);
        }
        for (uint32_t n = 0; n < Ring::capacity && slot.ring.pop(packet); ++n) send(slot, packet);
    }
}
void Service::poll() { if (running_) return; refresh(); pump(); }
bool Service::start() {
    if (running_.exchange(true)) return false;
    try {
        worker_ = std::thread([this] {
            auto next_refresh = std::chrono::steady_clock::now();
            try {
                while (running_.load(std::memory_order_acquire)) {
                    const auto now = std::chrono::steady_clock::now();
                    if (now >= next_refresh) { refresh(); next_refresh = now + std::chrono::milliseconds(500); }
                    pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            } catch (...) { ++send_fail_; running_ = false; }
        });
    } catch (...) { running_ = false; return false; }
    return true;
}
void Service::stop() {
    running_.store(false, std::memory_order_release); if (worker_.joinable()) worker_.join();
    for (size_t i = 0; i < count_; ++i) { auto& slot = *slots_[i]; slot.accepting = false; if (slot.online.exchange(false)) { backend_.close(slot.actual); slot.emergency = true; } }
    pump();
}
Counters Service::counters() const noexcept { return {received_, dropped_, malformed_, oversize_, ring_full_, disconnects_, send_fail_}; }
std::vector<EndpointState> Service::endpoint_states() const {
    std::vector<EndpointState> states;
    for (size_t i = 0; i < count_; ++i) states.push_back({slots_[i]->saved, slots_[i]->source, slots_[i]->online.load()});
    return states;
}
}
