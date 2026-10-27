// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace wirescape::voice {

constexpr int kMaxVoices = 16;
constexpr int kMaxHeldNotes = 128;
constexpr int kMaxSustainSources = 32;

enum class Mode : uint8_t { Rotate, Reuse, Reset, Mono };
enum class Priority : uint8_t { Last, Low, High };
enum class Status : uint8_t { Ok, InvalidConfig, InvalidNote, NoteCapacity, SourceCapacity, NotFound };

struct Config {
    uint8_t max_voices = 8;
    Mode mode = Mode::Rotate;
    Priority mono_priority = Priority::Last;
    bool legato = false;
};

struct Note {
    uint32_t source_id = 0;
    int32_t note_id = -1; // -1: match by source, channel and key on note-off.
    int16_t channel = 0;
    int16_t key = 60;
    float velocity = 1.0f;
};

struct Result {
    Status status = Status::Ok;
    int8_t voice = -1;
    bool stolen = false;
};

struct Voice {
    bool assigned = false; // A released voice remains assigned until reused/reset.
    bool gate = false;
    bool trigger = false; // One-shot since the last clear_triggers().
    bool retrigger = false; // Same voice was already assigned when triggered.
    uint32_t source_id = 0;
    int32_t note_id = -1;
    int16_t channel = 0;
    int16_t key = 60;
    float pitch = 0.0f; // 1 V/oct; MIDI 60 is zero.
    float velocity = 0.0f;
    float pressure = 0.0f;
    float timbre = 0.0f;
    uint64_t start_order = 0;
};

// All storage is internal and fixed-size. Single-owner; call only from the
// control/render owner, never concurrently. No ABI2 or operating-system types.
class Allocator {
public:
    explicit Allocator(Config config = {}) noexcept;
    Status configure(Config config) noexcept; // Resets state.
    void reset() noexcept;
    Result note_on(Note note) noexcept;
    Status note_off(Note note) noexcept;
    Status sustain(uint32_t source_id, bool down) noexcept;
    void release_all_from_source(uint32_t source_id) noexcept;
    Status expression(Note note, float pressure, float timbre) noexcept;
    void clear_triggers() noexcept;

    const Voice& voice(int index) const noexcept { return voices_[index]; } // 0 <= index < voice_count()
    int voice_count() const noexcept { return config_.mode == Mode::Mono ? 1 : config_.max_voices; }
    int active_count() const noexcept;
    int release_count() const noexcept;
    int held_count() const noexcept;
    Config config() const noexcept { return config_; }

    // Reserved for a future MPE adapter. Per-note expression can already be
    // addressed by note_id; no MPE channel ownership or pitch bend is implied.
private:
    struct Held {
        bool used = false;
        bool down = false;
        Note note{};
        uint64_t order = 0;
        int8_t voice = -1;
    };
    Config config_{};
    Voice voices_[kMaxVoices]{};
    Held held_[kMaxHeldNotes]{};
    uint32_t sustain_sources_[kMaxSustainSources]{};
    bool sustain_used_[kMaxSustainSources]{};
    uint64_t order_ = 0;
    uint8_t cursor_ = 0;

    int find(Note note) const noexcept;
    bool sustained(uint32_t source_id) const noexcept;
    int choose_voice(Note note) const noexcept;
    int choose_mono() const noexcept;
    void assign(int voice, int held, bool retrigger) noexcept;
    void release_held(int held) noexcept;
    void refresh_mono(bool was_gate) noexcept;
};

} // namespace wirescape::voice
