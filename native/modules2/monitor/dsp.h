// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../io/osc/codec.h"
#include "tap.h"
#include "wirescape_sdk.h"
#include <new>
namespace wirescape::monitor {
struct Monitor {
    static constexpr ws::Info info{"monitor", "Monitor", 1};
    static constexpr std::array inputs{ws::port("events", "Events", WS2_EV_MESSAGE, WS2_POLY, 0, nullptr, WS2_EVENT),
                                       ws::port("signal1", "Signal 1", WS2_ANY),
                                       ws::port("signal2", "Signal 2", WS2_ANY)};
    static constexpr std::array<ws2_port, 0> outputs{};
    static constexpr std::array<ws2_param, 0> params{};
    struct Key {
        bool used = false;
        uint32_t source = 0, type = 0, code = 0, count = 0;
        uint64_t window = 0;
        char address[256]{};
    };
    std::array<Key, 128> keys{};
    Tap *tap = nullptr;
    double rate = 48000;
    uint64_t next_signal = 0;
    static ws2_memory memory_requirements(const ws2_config &) { return {sizeof(ws2_memory), 0, sizeof(Tap) + 32, 32}; }
    bool prepare(const ws::Setup &s) {
        Configuration cfg;
        if (s.config.bytes) {
            if (!s.config.data || s.config.bytes != sizeof(cfg)) return false;
            std::memmove(&cfg, s.config.data, sizeof(cfg));
            if (cfg.version != 1 || cfg.reserved) return false;
        }
        tap = cfg.tap;
        if (!tap) {
            void *memory = s.alloc(sizeof(Tap), 32, WS2_LARGE);
            if (!memory) return false;
            tap = new (memory) Tap();
        }
        rate = s.sample_rate();
        return true;
    }
    void reset() {
        keys = {};
        next_signal = 0;
    } // never reset shared SPSC indices while a UI consumer may read
    bool permit(uint32_t source, uint32_t type, uint32_t code, std::string_view address, uint64_t frame) {
        const uint64_t window = uint64_t(double(frame) / rate);
        Key *free = nullptr;
        for (auto &k : keys) {
            if (k.used && k.source == source && k.type == type && k.code == code && address == k.address) {
                if (k.window != window) {
                    k.window = window;
                    k.count = 0;
                }
                if (k.count >= 30) {
                    ++tap->rate_limited;
                    return false;
                }
                ++k.count;
                return true;
            }
            if (!k.used || k.window != window) free = &k;
        }
        if (!free) {
            ++tap->dropped;
            return false;
        }
        *free = {};
        free->used = true;
        free->source = source;
        free->type = type;
        free->code = code;
        free->window = window;
        free->count = 1;
        if (!address.empty()) std::memcpy(free->address, address.data(), address.size());
        return true;
    }
    void process(ws::Block &b) {
        for (const auto &e : b.events()) {
            ++tap->received;
            std::string_view address;
            uint32_t code = uint32_t(e.key);
            if (e.type == WS2_MIDI && e.ump_words) {
                const uint32_t status = (e.ump[0] >> 20) & 15;
                code = e.ump[0] & ((status >= 8 && status <= 11) ? 0xffff7f00u : 0xffff0000u);
            }
            if (e.type == WS2_MESSAGE && e.message_kind == WS2_MSG_OSC)
                osc::parse(e.data, e.bytes, [&](const osc::Message &m) { address = m.address; });
            if (!permit(e.source_id, e.type, code, address, b.frame_position() + e.offset)) continue;
            Entry entry;
            entry.frame = b.frame_position() + e.offset;
            entry.source = e.source_id;
            entry.type = e.type;
            entry.key = code;
            if (e.type == WS2_MIDI) {
                entry.bytes = std::min(4u, e.ump_words) * 4;
                std::memcpy(entry.payload, e.ump, entry.bytes);
            } else if (e.type == WS2_MESSAGE) {
                const uint32_t n = std::min<uint32_t>(e.bytes, sizeof(entry.payload));
                if (n) std::memcpy(entry.payload, e.data, n);
                entry.bytes = n | (e.bytes > n ? 0x80000000u : 0);
            } else {
                NoteSummary n{e.key, e.note_id, e.channel, e.expression, e.value, e.value2};
                entry.bytes = sizeof(n);
                std::memcpy(entry.payload, &n, sizeof(n));
            }
            tap->push(entry);
        }
        if (b.frame_position() >= next_signal) {
            next_signal = b.frame_position() + std::max(uint64_t(1), uint64_t(rate / 30.));
            for (uint32_t p = 1; p < 3; ++p)
                if (b.in(p).connected())
                    for (uint32_t c = 0; c < b.in(p).channels(); ++c) {
                        ++tap->received;
                        if (!permit(0, signal_type | p, c, {}, b.frame_position())) continue;
                        Entry e;
                        e.frame = b.frame_position();
                        e.type = signal_type | p;
                        e.key = c;
                        e.bytes = sizeof(float);
                        const float value = b.in(p).value(c, 0);
                        std::memcpy(e.payload, &value, sizeof(value));
                        tap->push(e);
                    }
        }
    }
    int save_state(ws::Writer w) { return w.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) {
        uint32_t v;
        if (!r.u32(v)) return WS2_INVALID;
        if (v != 1) return WS2_UNSUPPORTED;
        reset();
        return WS2_OK;
    }
};
} // namespace wirescape::monitor
