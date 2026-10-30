// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
#include "../../engine2/midi_release.h"
namespace wirescape::midi {
struct MidiIn {
    static constexpr ws::Info info{"midi.in", "MIDI In", 1};
    static constexpr std::array<ws2_port, 0> inputs{};
    static constexpr std::array outputs{ws::port("midi", "MIDI", WS2_EV_MIDI, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array params{ws::param("channel_filter", "Channel (0 = Omni)", 0, 16, 0, "", WS2_STEPPED | WS2_AUTOMATABLE, WS2_LINEAR, 0, 4, 1)};
    std::array<uint32_t, 128> sources{}; uint32_t source_count = 0, last_filter = UINT32_MAX;
    std::array<ws2_event, 256> pending{}; uint32_t pending_count = 0; bool panic_pending = false;
    void prepare(const ws::Setup&) {}
    void reset() { pending_count = source_count = 0; last_filter = UINT32_MAX; panic_pending = false; }
    void process(ws::Block& b) {
        b.out(0).set_channels(0);
        if (panic_pending) { ws2_event panic{}; panic.struct_size = sizeof(panic); panic.type = WS2_NOTE_CHOKE; panic.source_id = UINT32_MAX; panic.channel = -1; panic.note_id = -1; if (b.emit(0, panic)) panic_pending = false; }
        uint32_t retained = 0;
        for (uint32_t i = 0; i < pending_count; ++i) { auto e = pending[i]; e.offset = 0; if (!b.emit(0, e)) pending[retained++] = e; }
        pending_count = retained;
        auto send = [&](const ws2_event& e) {
            if (!b.emit(0, e) && (e.type == WS2_NOTE_CHOKE || engine2::midi_release(e))) {
                if (pending_count < pending.size()) pending[pending_count++] = e; else panic_pending = true;
            }
        };
        auto update_filter = [&](uint32_t at) {
            const uint32_t filter = uint32_t(std::clamp(std::lround(b.param(0).value(0, at)), 0l, 16l));
            if (last_filter != UINT32_MAX && filter != last_filter) {
                for (uint32_t i = 0; i < source_count; ++i) { ws2_event e{}; e.struct_size = sizeof(e); e.type = WS2_NOTE_CHOKE; e.source_id = sources[i]; e.note_id = e.channel = -1; e.offset = at; send(e); }
                source_count = 0;
            }
            last_filter = filter;
        };
        update_filter(0);
        for (const auto& e : b.events()) {
            update_filter(e.offset);
            if (e.type != WS2_MIDI && e.type != WS2_NOTE_CHOKE) continue;
            if (e.type == WS2_MIDI) {
                if (!e.ump_words) continue;
                const uint32_t type = e.ump[0] >> 28;
                if (last_filter && (type == 2 || type == 4) && ((e.ump[0] >> 16) & 15) != last_filter - 1) continue;
                bool found = false; for (uint32_t i = 0; i < source_count; ++i) found = found || sources[i] == e.source_id;
                if (!found) { if (source_count < sources.size()) sources[source_count++] = e.source_id; else panic_pending = true; }
            } else {
                uint32_t keep = 0; for (uint32_t i = 0; i < source_count; ++i) if (e.source_id != UINT32_MAX && sources[i] != e.source_id) sources[keep++] = sources[i]; source_count = keep;
            }
            send(e);
        }
    }
    int save_state(ws::Writer w) { return w.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) { uint32_t version; if (!r.u32(version)) return WS2_INVALID; if (version != 1) return WS2_UNSUPPORTED; reset(); return WS2_OK; }
};
}
