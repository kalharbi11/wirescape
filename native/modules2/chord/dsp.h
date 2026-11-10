// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Qt and cppcheck may define emit as a macro; restore the caller's definition.
#ifdef emit
#pragma push_macro("emit")
#undef emit
#define CHORD_RESTORE_EMIT
#endif
#include "wirescape_sdk.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace chord {

struct Chord {
    static constexpr ws::Info info{"essentials.chord", "Chord", 1};
    static constexpr std::array inputs{
        ws::port("notes", "Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array outputs{
        ws::port("notes", "Notes", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr uint32_t kStepped = WS2_STEPPED | WS2_AUTOMATABLE;
    static constexpr std::array params{
        ws::param("enable1", "Enable 1", 0, 1, 1, "", kStepped, WS2_LINEAR, 0, 4, 1),
        ws::param("shift1", "Shift 1", -24, 24, 4, "st", kStepped, WS2_LINEAR, 1, 4, 1),
        ws::param("enable2", "Enable 2", 0, 1, 1, "", kStepped, WS2_LINEAR, 2, 4, 1),
        ws::param("shift2", "Shift 2", -24, 24, 7, "st", kStepped, WS2_LINEAR, 3, 4, 1),
        ws::param("enable3", "Enable 3", 0, 1, 0, "", kStepped, WS2_LINEAR, 4, 4, 1),
        ws::param("shift3", "Shift 3", -24, 24, 12, "st", kStepped, WS2_LINEAR, 5, 4, 1),
        ws::param("enable4", "Enable 4", 0, 1, 0, "", kStepped, WS2_LINEAR, 6, 4, 1),
        ws::param("shift4", "Shift 4", -24, 24, -12, "st", kStepped, WS2_LINEAR, 7, 4, 1)};

    static constexpr uint32_t kActivations = 16, kNotes = 5, kPending = 80, kSeqMask = (1u << 20) - 1;
    struct Voice {
        int key = 0;
        int32_t id = -1;
    };
    struct Activation {
        bool used = false;
        uint32_t source = 0;
        int32_t channel = 0, input_id = -1, key = 0;
        uint64_t order = 0;
        uint32_t count = 0;
        std::array<Voice, kNotes> voices{};
    };
    std::array<Activation, kActivations> active{};
    std::array<ws2_event, kPending> pending{};
    uint32_t pending_count = 0, tag = 0, seq = 0;
    uint64_t order = 0;

    // Control-side adapter supplies a unique graph instance number as four LE bytes.
    // Without it, a fan-in of two Chord instances could alias note identities.
    bool prepare(const ws::Setup &s) {
        if (!s.config.data || s.config.bytes != 4) return false;
        tag = uint32_t(s.config.data[0]) | (uint32_t(s.config.data[1]) << 8) |
              (uint32_t(s.config.data[2]) << 16) | (uint32_t(s.config.data[3]) << 24);
        tag &= 2047u;
        return true;
    }
    void reset() {
        active = {};
        pending = {};
        pending_count = 0;
        seq = 0;
        order = 0;
    }
    uint32_t active_voice_count() const {
        uint32_t n = 0;
        for (const auto &a : active)
            if (a.used) n += a.count;
        return n;
    }
    bool reserved(int32_t id) const {
        for (const auto &a : active)
            if (a.used)
                for (uint32_t i = 0; i < a.count; ++i)
                    if (a.voices[i].id == id) return true;
        for (uint32_t i = 0; i < pending_count; ++i)
            if (pending[i].note_id == id) return true;
        return false;
    }
    int32_t next_id() {
        // At most 160 active or queued identities can be reserved. No unbounded search.
        for (uint32_t i = 0; i < 2 * kPending + 1; ++i) {
            seq = (seq + 1) & kSeqMask;
            const auto id = static_cast<int32_t>((tag << 20) | seq);
            if (!reserved(id)) return id;
        }
        return -1;
    }
    static ws2_event outbound(const ws2_event &input, const Voice &voice, uint32_t type) {
        auto e = input;
        e.struct_size = sizeof(e);
        e.port = 0;
        e.type = type;
        e.key = voice.key;
        e.note_id = voice.id;
        e.value = type == WS2_NOTE_ON ? input.value : 0.f;
        e.ump_words = 0;
        e.bytes = 0;
        e.data = nullptr;
        return e;
    }
    void flush(const ws::Block &b) {
        uint32_t keep = 0;
        for (uint32_t i = 0; i < pending_count; ++i) {
            auto e = pending[i];
            e.offset = 0;
            if (!b.emit(0, e)) pending[keep++] = e;
        }
        pending_count = keep;
    }
    void release(const ws::Block &b, Activation &a, const ws2_event &cause) {
        for (uint32_t i = 0; i < a.count; ++i) {
            const auto e = outbound(cause, a.voices[i], WS2_NOTE_OFF);
            // Override the NOTE_OFF's identity with the activation's exact source/channel.
            auto off = e;
            off.source_id = a.source;
            off.channel = a.channel;
            if (!b.emit(0, off) && pending_count < kPending) pending[pending_count++] = off;
        }
        a = {};
    }
    Activation *find(const ws2_event &e) {
        Activation *match = nullptr;
        for (auto &a : active) {
            if (!a.used || a.source != e.source_id || a.channel != e.channel) continue;
            if (e.note_id >= 0 && a.input_id == e.note_id) return &a;
            if (e.note_id < 0 && a.input_id < 0 && a.key == e.key && (!match || a.order > match->order)) match = &a;
        }
        return e.note_id < 0 ? match : nullptr;
    }
    void note_on(const ws::Block &b, const ws2_event &e) {
        if (e.key < 0 || e.key > 127 || e.channel < 0 || e.channel > 15) return;
        if (e.note_id >= 0)
            if (auto *prior = find(e)) release(b, *prior, e);

        int keys[kNotes]{e.key};
        uint32_t count = 1;
        for (uint32_t i = 0; i < 4; ++i) {
            if (b.param(2 * i).value(0, e.offset) < 0.5f) continue;
            const int delta = std::clamp(static_cast<int>(std::lround(b.param(2 * i + 1).value(0, e.offset))), -24, 24);
            const int key = e.key + delta;
            if (key >= 0 && key <= 127) keys[count++] = key;
        }
        // Every accepted attack reserves future release space, even if the engine
        // subsequently refuses several NOTE_ON events in this chord.
        if (pending_count + active_voice_count() + count > kPending) return;
        Activation *slot = nullptr;
        for (auto &a : active)
            if (!a.used) {
                slot = &a;
                break;
            }
        if (!slot) return;
        Activation a{};
        a.used = true;
        a.source = e.source_id;
        a.channel = e.channel;
        a.input_id = e.note_id;
        a.key = e.key;
        a.order = ++order;
        for (uint32_t i = 0; i < count; ++i) {
            const int32_t id = next_id();
            if (id < 0) break;
            const auto out = outbound(e, {keys[i], id}, WS2_NOTE_ON);
            if (b.emit(0, out)) a.voices[a.count++] = {keys[i], id};
        }
        if (a.count) *slot = a;
    }
    void other(const ws::Block &b, const ws2_event &e) {
        if (e.type == WS2_NOTE_OFF) {
            if (auto *a = find(e)) release(b, *a, e);
        } else if (e.type == WS2_NOTE_CHOKE) {
            for (auto &a : active)
                if (a.used && (e.source_id == UINT32_MAX || a.source == e.source_id))
                    release(b, a, e);
        } else if (e.type == WS2_NOTE_EXPRESSION) {
            if (e.note_id < 0) {
                // Channel-wide expression keeps source/channel and anonymous ID.
                auto out = e;
                out.port = 0;
                b.emit(0, out);
            } else if (const auto *a = find(e)) {
                for (uint32_t i = 0; i < a->count; ++i) {
                    auto out = e;
                    out.port = 0;
                    out.note_id = a->voices[i].id;
                    out.key = a->voices[i].key;
                    b.emit(0, out);
                }
            }
        }
    }
    void process(ws::Block &b) {
        b.out(0).set_channels(0);
        flush(b);
        const auto events = b.events_in(0);
        auto it = events.begin(), end = events.end();
        while (it && it != end) {
            const auto *last = it + 1;
            while (last != end && last->offset == it->offset) ++last;
            // Release/express before new attacks at the same sample offset.
            for (auto *p = it; p != last; ++p)
                if (p->type != WS2_NOTE_ON) other(b, *p);
            for (auto *p = it; p != last; ++p)
                if (p->type == WS2_NOTE_ON) note_on(b, *p);
            it = last;
        }
    }
    int save_state(ws::Writer w) { return w.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) {
        uint32_t version = 0;
        if (!r.u32(version)) return WS2_INVALID;
        if (version != 1) return WS2_UNSUPPORTED;
        reset(); // Active notes and pending releases are transient; never resurrect.
        return WS2_OK;
    }
};

} // namespace chord
#ifdef CHORD_RESTORE_EMIT
#pragma pop_macro("emit")
#undef CHORD_RESTORE_EMIT
#endif
