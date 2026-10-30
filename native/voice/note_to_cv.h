// SPDX-License-Identifier: MIT
#pragma once
#include "allocator.h"
#include "wirescape/module2.h"
#include <array>
namespace wirescape::voice {
// Single render owner. Source/channel pairs get bounded internal identities so
// the allocator's source-scoped sustain remains independent per MIDI channel.
class NoteToCv {
    struct Source {
        bool used = false;
        uint32_t id = 0;
        int16_t channel = 0;
    };
    std::array<Source, kMaxHeldNotes> sources_{};
    Allocator allocator_;
    struct Anonymous {
        bool used = false;
        uint32_t source = 0;
        int channel = 0, key = 0;
        int32_t id = 0;
        uint64_t order = 0;
        bool retired = false;
    };
    std::array<Anonymous, kMaxHeldNotes> anonymous_{};
    uint32_t serial_ = 0;
    uint64_t activation_ = 0;
    int32_t next_id() noexcept {
        // At most 128 anonymous notes and 16 voices can reserve IDs.
        for (int attempt = 0; attempt < kMaxHeldNotes + kMaxVoices + 1; ++attempt) {
            serial_ = (serial_ + 1) & 0x7fffffffu;
            int32_t id = static_cast<int32_t>(serial_);
            bool used = false;
            for (const auto &a : anonymous_)
                if (a.used && a.id == id) used = true;
            for (int c = 0; c < allocator_.voice_count(); ++c)
                if (allocator_.voice(c).gate && allocator_.voice(c).note_id == id) used = true;
            if (!used) return id;
        }
        return -1;
    }
    int token(uint32_t source, int channel, bool create) noexcept {
        int empty = -1;
        for (int i = 0; i < kMaxHeldNotes; ++i) {
            const auto &s = sources_[i];
            if (s.used && s.id == source && s.channel == channel) return i;
            if (!s.used && empty < 0) empty = i;
        }
        if (!create) return -1;
        if (empty < 0) {
            // Reclaim only between events, after synchronous release emission.
            // Host output queues hold original source IDs, not these tokens.
            std::array<bool, kMaxHeldNotes> reserved{};
            allocator_.visit_sources_in_use([&](uint32_t source) {
                if (source < reserved.size()) reserved[source] = true;
            });
            for (const auto &a : anonymous_)
                if (a.used) reserved[a.source] = true;
            for (int i = 0; i < kMaxHeldNotes; ++i)
                if (!reserved[i]) {
                    empty = i;
                    break;
                }
        }
        if (empty < 0) return -1;
        sources_[empty] = {true, source, static_cast<int16_t>(channel)};
        return empty;
    }
    template <class Emit>
    void changes(const std::array<Voice, kMaxVoices> &before, const ws2_event &cause, Emit emit, bool choke) {
        // Releases precede replacement attacks at the same offset.
        for (int c = 0; c < allocator_.voice_count(); ++c) {
            const auto &a = before[c];
            const auto &b = allocator_.voice(c);
            if (a.gate && (!b.gate || a.start_order != b.start_order)) {
                auto out = cause;
                out.type = choke || b.gate ? WS2_NOTE_CHOKE : WS2_NOTE_OFF;
                out.channel = c;
                out.key = a.key;
                out.note_id = a.note_id;
                out.source_id = sources_[a.source_id].id;
                out.value = 0;
                emit(out);
            }
        }
        for (int c = 0; c < allocator_.voice_count(); ++c) {
            const auto &a = before[c];
            const auto &b = allocator_.voice(c);
            if (b.gate && (!a.gate || a.start_order != b.start_order)) {
                auto out = cause;
                out.type = WS2_NOTE_ON;
                out.channel = c;
                out.key = b.key;
                out.note_id = b.note_id;
                out.source_id = sources_[b.source_id].id;
                out.value = b.velocity;
                emit(out);
            }
        }
    }
    std::array<Voice, kMaxVoices> snapshot() const noexcept {
        std::array<Voice, kMaxVoices> r{};
        for (int c = 0; c < allocator_.voice_count(); ++c) r[c] = allocator_.voice(c);
        return r;
    }

  public:
    Status configure(Config config) noexcept {
        anonymous_ = {};
        serial_ = 0;
        activation_ = 0;
        sources_ = {};
        return allocator_.configure(config);
    }
    void reset() noexcept {
        allocator_.reset();
        sources_ = {};
        anonymous_ = {};
        serial_ = 0;
        activation_ = 0;
    }
    uint32_t original_source(uint32_t token) const noexcept {
        return token < sources_.size() ? sources_[token].id : UINT32_MAX;
    }
    Allocator &allocator() noexcept { return allocator_; }
    const Allocator &allocator() const noexcept { return allocator_; }
    template <class Emit> void sustain(uint32_t source, int channel, bool down, const ws2_event &e, Emit emit) {
        int t = token(source, channel, down);
        if (t < 0) return;
        auto before = snapshot();
        allocator_.sustain(static_cast<uint32_t>(t), down);
        changes(before, e, emit, false);
    }
    template <class Emit> void release_channel(uint32_t source, int channel, const ws2_event &e, Emit emit) {
        int t = token(source, channel, false);
        if (t < 0) return;
        auto before = snapshot();
        allocator_.release_all_from_source(static_cast<uint32_t>(t));
        changes(before, e, emit, true);
        for (auto &a : anonymous_)
            if (a.used && a.source == static_cast<uint32_t>(t)) a = {};
    }
    template <class Emit> void release_all(uint32_t source, const ws2_event &e, Emit emit) {
        auto before = snapshot();
        if (source == UINT32_MAX) allocator_.reset();
        else
            for (int t = 0; t < kMaxHeldNotes; ++t)
                if (sources_[t].used && sources_[t].id == source) allocator_.release_all_from_source(t);
        changes(before, e, emit, true);
        for (auto &a : anonymous_)
            if (a.used && (source == UINT32_MAX || sources_[a.source].id == source)) a = {};
    }
    template <class Emit> void event(const ws2_event &e, Emit emit) {
        if (e.type == WS2_NOTE_CHOKE) {
            release_all(e.source_id, e, emit);
            return;
        }
        int channel = e.channel < 0 ? 0 : e.channel;
        if (channel > 15) return;
        int t = token(e.source_id, channel, e.type == WS2_NOTE_ON);
        if (t < 0) return;
        Note n{static_cast<uint32_t>(t), e.note_id, static_cast<int16_t>(channel), static_cast<int16_t>(e.key),
               e.value};
        auto before = snapshot();
        if (e.type == WS2_NOTE_ON) {
            int slot = -1;
            if (n.note_id < 0) {
                for (int i = 0; i < kMaxHeldNotes; ++i)
                    if (!anonymous_[i].used) {
                        slot = i;
                        break;
                    }
                if (slot < 0) return;
                n.note_id = next_id();
                if (n.note_id < 0) return;
            }
            auto result = allocator_.note_on(n);
            if (result.status == Status::Ok && slot >= 0)
                anonymous_[slot] = {true, n.source_id, channel, n.key, n.note_id, ++activation_};
            if (result.status == Status::Ok && slot < 0)
                for (auto &a : anonymous_)
                    if (a.used && a.source == n.source_id && a.id == n.note_id) a.retired = true;
        } else if (e.type == WS2_NOTE_OFF) {
            int slot = -1;
            if (n.note_id < 0) {
                for (int i = 0; i < kMaxHeldNotes; ++i) {
                    const auto &a = anonymous_[i];
                    if (a.used && a.source == n.source_id && a.channel == channel && a.key == n.key &&
                        (slot < 0 || a.order < anonymous_[slot].order))
                        slot = i;
                }
                if (slot < 0) return;
                n.note_id = anonymous_[slot].id;
                bool retired = anonymous_[slot].retired;
                anonymous_[slot] = {};
                if (retired) return;
            }
            // A stolen activation remains in our FIFO until its own Note Off.
            // Its allocator record may be gone; that off must not close its replacement.
            allocator_.note_off(n);
        } else if (e.type == WS2_NOTE_EXPRESSION) {
            for (int c = 0; c < allocator_.voice_count(); ++c) {
                const auto &v = allocator_.voice(c);
                if (v.source_id == n.source_id &&
                    ((n.note_id >= 0 && n.note_id == v.note_id) || (n.note_id < 0 && n.key == v.key))) {
                    Note addressed = n;
                    addressed.note_id = v.note_id;
                    allocator_.expression(addressed, e.expression == WS2_EX_PRESSURE ? e.value : v.pressure,
                                          e.expression == WS2_EX_TIMBRE ? e.value : v.timbre);
                    auto out = e;
                    out.channel = c;
                    out.note_id = v.note_id;
                    emit(out);
                    if (n.note_id >= 0) break;
                }
            }
        }
        changes(before, e, emit, false);
    }
};
} // namespace wirescape::voice
