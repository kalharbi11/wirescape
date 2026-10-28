// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
namespace wirescape::monitor {
struct Entry {
    uint64_t frame = 0;
    uint32_t source = 0, type = 0, key = 0, bytes = 0; // high bytes bit = truncated
    uint8_t payload[104]{};
};
static_assert(sizeof(Entry) == 128, "Monitor ring entries are fixed 128 bytes");
constexpr uint32_t signal_type = 0x10000000u;
struct Tap {
    std::array<Entry, 1024> entries{};
    std::atomic<uint32_t> write{0}, read{0};
    std::atomic<uint32_t> received{0}, dropped{0}, rate_limited{0};
    bool push(const Entry& e) noexcept {
        const uint32_t w = write.load(std::memory_order_relaxed), r = read.load(std::memory_order_acquire);
        if (w - r == entries.size()) { ++dropped; return false; }
        entries[w % entries.size()] = e; write.store(w + 1, std::memory_order_release); return true;
    }
    bool pop(Entry& e) noexcept {
        const uint32_t r = read.load(std::memory_order_relaxed), w = write.load(std::memory_order_acquire);
        if (r == w) return false;
        e = entries[r % entries.size()]; read.store(r + 1, std::memory_order_release); return true;
    }
    void clear() noexcept { read.store(write.load(std::memory_order_acquire), std::memory_order_release); }
};
struct Configuration { uint32_t version = 1, reserved = 0; Tap* tap = nullptr; }; // host-private pointer; JSON never contains it
struct NoteSummary { int32_t key = 0, note_id = -1, channel = -1; uint32_t expression = 0; float value = 0, value2 = 0; };
}
