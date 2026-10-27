// SPDX-License-Identifier: MIT
#include "allocator.h"

#include <cmath>

namespace wirescape::voice {

Allocator::Allocator(Config config) noexcept { configure(config); }

Status Allocator::configure(Config config) noexcept {
    if (config.max_voices < 1 || config.max_voices > kMaxVoices ||
        static_cast<unsigned>(config.mode) > static_cast<unsigned>(Mode::Mono) ||
        static_cast<unsigned>(config.mono_priority) > static_cast<unsigned>(Priority::High)) {
        return Status::InvalidConfig;
    }
    config_ = config;
    reset();
    return Status::Ok;
}

void Allocator::reset() noexcept {
    for (auto& v : voices_) v = {};
    for (auto& h : held_) h = {};
    for (auto& used : sustain_used_) used = false;
    for (auto& source : sustain_sources_) source = 0;
    order_ = 0;
    cursor_ = 0;
}

int Allocator::find(Note note) const noexcept {
    int result = -1;
    uint64_t newest = 0;
    for (int i = 0; i < kMaxHeldNotes; ++i) {
        const Held& h = held_[i];
        if (!h.used || h.note.source_id != note.source_id) continue;
        const bool same = note.note_id >= 0
            ? h.note.note_id == note.note_id
            : h.note.note_id < 0 && h.note.channel == note.channel && h.note.key == note.key;
        if (same && (result < 0 || h.order > newest)) { result = i; newest = h.order; }
    }
    return result;
}

bool Allocator::sustained(uint32_t source_id) const noexcept {
    for (int i = 0; i < kMaxSustainSources; ++i)
        if (sustain_used_[i] && sustain_sources_[i] == source_id) return true;
    return false;
}

int Allocator::choose_voice(Note note) const noexcept {
    const int count = voice_count();
    if (config_.mode == Mode::Reuse) {
        for (int i = 0; i < count; ++i) {
            const Voice& v = voices_[i];
            if (!v.gate && v.assigned && v.source_id == note.source_id &&
                v.channel == note.channel && v.key == note.key) return i;
        }
    }
    if (config_.mode == Mode::Rotate) {
        for (int offset = 0; offset < count; ++offset) {
            const int i = (cursor_ + offset) % count;
            if (!voices_[i].gate) return i;
        }
    } else {
        for (int i = 0; i < count; ++i) if (!voices_[i].gate) return i;
    }
    int oldest = 0;
    for (int i = 1; i < count; ++i)
        if (voices_[i].start_order < voices_[oldest].start_order) oldest = i;
    return oldest;
}

int Allocator::choose_mono() const noexcept {
    int best = -1;
    for (int i = 0; i < kMaxHeldNotes; ++i) {
        const Held& h = held_[i];
        if (!h.used) continue;
        if (best < 0) { best = i; continue; }
        const Held& b = held_[best];
        if (config_.mono_priority == Priority::Last) {
            if (h.order > b.order) best = i;
        } else if (config_.mono_priority == Priority::Low) {
            if (h.note.key < b.note.key || (h.note.key == b.note.key && h.order > b.order)) best = i;
        } else if (h.note.key > b.note.key || (h.note.key == b.note.key && h.order > b.order)) best = i;
    }
    return best;
}

void Allocator::assign(int index, int held, bool retrigger) noexcept {
    Voice& v = voices_[index];
    const Note& n = held_[held].note;
    const bool was_assigned = v.assigned;
    v.assigned = true;
    v.gate = true;
    v.trigger = retrigger;
    v.retrigger = retrigger && was_assigned;
    v.source_id = n.source_id;
    v.note_id = n.note_id;
    v.channel = n.channel;
    v.key = n.key;
    v.pitch = (static_cast<float>(n.key) - 60.0f) / 12.0f;
    v.velocity = n.velocity;
    v.pressure = 0.0f;
    v.timbre = 0.0f;
    v.start_order = held_[held].order;
    held_[held].voice = static_cast<int8_t>(index);
}

void Allocator::release_held(int index) noexcept {
    const int v = held_[index].voice;
    if (v >= 0 && voices_[v].gate && voices_[v].start_order == held_[index].order) {
        voices_[v].gate = false;
        voices_[v].trigger = false;
        voices_[v].retrigger = false;
    }
    held_[index] = {};
}

void Allocator::refresh_mono(bool was_gate) noexcept {
    const int selected = choose_mono();
    if (selected < 0) {
        voices_[0].gate = false;
        voices_[0].trigger = false;
        voices_[0].retrigger = false;
        return;
    }
    if (held_[selected].voice == 0 && voices_[0].gate) return;
    for (auto& h : held_) if (h.used) h.voice = -1;
    assign(0, selected, !(config_.legato && was_gate));
}

Result Allocator::note_on(Note note) noexcept {
    if (note.key < 0 || note.key > 127 || note.channel < 0 || note.channel > 15 ||
        note.note_id < -1 || !std::isfinite(note.velocity) || note.velocity < 0.0f || note.velocity > 1.0f)
        return {Status::InvalidNote, -1, false};
    // A repeated explicit note_id replaces its prior note within the same source.
    const int duplicate = note.note_id >= 0 ? find(note) : -1;
    int slot = duplicate;
    if (slot < 0) {
        for (int i = 0; i < kMaxHeldNotes; ++i) if (!held_[i].used) { slot = i; break; }
    }
    if (slot < 0) return {Status::NoteCapacity, -1, false};
    const bool mono_gate = voices_[0].gate;
    if (duplicate >= 0) release_held(duplicate);
    held_[slot].used = true;
    held_[slot].down = true;
    held_[slot].note = note;
    held_[slot].order = ++order_;
    held_[slot].voice = -1;
    if (config_.mode == Mode::Mono) {
        refresh_mono(mono_gate);
        return {Status::Ok, 0, false};
    }
    const int v = choose_voice(note);
    const bool stolen = voices_[v].gate;
    if (stolen) {
        for (int i = 0; i < kMaxHeldNotes; ++i)
            if (held_[i].used && held_[i].voice == v && i != slot) held_[i] = {};
    }
    assign(v, slot, true);
    cursor_ = static_cast<uint8_t>((v + 1) % voice_count());
    return {Status::Ok, static_cast<int8_t>(v), stolen};
}

Status Allocator::note_off(Note note) noexcept {
    const int index = find(note);
    if (index < 0) return Status::NotFound;
    if (sustained(held_[index].note.source_id)) {
        held_[index].down = false;
        return Status::Ok;
    }
    const bool was_gate = voices_[0].gate;
    release_held(index);
    if (config_.mode == Mode::Mono) refresh_mono(was_gate);
    return Status::Ok;
}

Status Allocator::sustain(uint32_t source_id, bool down) noexcept {
    int found = -1, free = -1;
    for (int i = 0; i < kMaxSustainSources; ++i) {
        if (sustain_used_[i] && sustain_sources_[i] == source_id) found = i;
        if (!sustain_used_[i] && free < 0) free = i;
    }
    if (down) {
        if (found >= 0) return Status::Ok;
        if (free < 0) return Status::SourceCapacity;
        sustain_used_[free] = true;
        sustain_sources_[free] = source_id;
        return Status::Ok;
    }
    if (found >= 0) sustain_used_[found] = false;
    const bool was_gate = voices_[0].gate;
    for (int i = 0; i < kMaxHeldNotes; ++i)
        if (held_[i].used && !held_[i].down && held_[i].note.source_id == source_id) release_held(i);
    if (config_.mode == Mode::Mono) refresh_mono(was_gate);
    return Status::Ok;
}

void Allocator::release_all_from_source(uint32_t source_id) noexcept {
    for (int i = 0; i < kMaxSustainSources; ++i)
        if (sustain_used_[i] && sustain_sources_[i] == source_id) sustain_used_[i] = false;
    const bool was_gate = voices_[0].gate;
    for (int i = 0; i < kMaxHeldNotes; ++i)
        if (held_[i].used && held_[i].note.source_id == source_id) release_held(i);
    if (config_.mode == Mode::Mono) refresh_mono(was_gate);
}

Status Allocator::expression(Note note, float pressure, float timbre) noexcept {
    if (!std::isfinite(pressure) || !std::isfinite(timbre) || pressure < 0 || pressure > 1 ||
        timbre < 0 || timbre > 1) return Status::InvalidNote;
    const int index = find(note);
    if (index < 0) return Status::NotFound;
    const int v = held_[index].voice;
    if (v >= 0 && voices_[v].gate) {
        voices_[v].pressure = pressure;
        voices_[v].timbre = timbre;
    }
    return Status::Ok;
}

void Allocator::clear_triggers() noexcept {
    for (auto& voice : voices_) { voice.trigger = false; voice.retrigger = false; }
}

int Allocator::active_count() const noexcept {
    int n = 0;
    for (int i = 0; i < voice_count(); ++i) if (voices_[i].gate) ++n;
    return n;
}

int Allocator::release_count() const noexcept {
    int n = 0;
    for (int i = 0; i < voice_count(); ++i) if (voices_[i].assigned && !voices_[i].gate) ++n;
    return n;
}

int Allocator::held_count() const noexcept {
    int n = 0;
    for (const auto& h : held_) if (h.used) ++n;
    return n;
}

} // namespace wirescape::voice
