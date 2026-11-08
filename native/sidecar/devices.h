// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "midi_backend.h"
#include "protocol.h"
#include <array>
#include <atomic>
#include <chrono>
namespace wirescape::sidecar {
// Decorates the existing backend; callbacks update bounded atomics before
// forwarding unchanged packets to the existing MIDI service.
class ObservedMidi final : public midi::Backend {
    struct Slot {
        ObservedMidi *owner = nullptr;
        uint8_t index = 0;
        midi::Endpoint endpoint;
        midi::Callback downstream;
        std::array<std::atomic<uint16_t>, 128> notes{};
        std::atomic<uint64_t> activity{0};
    };
    std::unique_ptr<midi::Backend> backend_;
    std::array<Slot, 16> slots_{};
    // One latest-wins observation; no queue, callbacks never allocate or lock.
    static_assert(std::atomic<uint64_t>::is_always_lock_free, "MIDI Learn requires a lock-free packed atomic");
    std::atomic<bool> learn_armed_{false};
    std::atomic<uint64_t> learn_packet_{0};
    std::atomic<uint32_t> learn_sequence_{0};
    std::atomic<uint8_t> learn_epoch_{0};
    std::chrono::steady_clock::time_point learn_deadline_{};
    void capture_cc(uint8_t slot, uint8_t channel, uint8_t controller, uint8_t value) noexcept;
    bool fake_;
    std::mutex mutex_;
    std::vector<midi::Endpoint> observed_;
    std::vector<midi::Endpoint> fake_endpoints_{{"M8 MIDI 1", "M8:M8 MIDI 1"}};
    static void message(void *, const uint8_t *, size_t, uint64_t);
    static void ump(void *, const uint32_t *, size_t, uint64_t);
    static void note(Slot &, unsigned, unsigned, bool);

  public:
    explicit ObservedMidi(bool fake, bool authorized);
    std::vector<midi::Endpoint> endpoints() override;
    std::vector<midi::Endpoint> observed();
    bool open(const midi::Endpoint &, midi::Callback) override;
    void close(const midi::Endpoint &) override;
    Json held() const;
    Json activity();
    void arm_learning(bool armed); // control thread only
    void expire_learning(std::chrono::steady_clock::time_point now); // control thread only
    Json learning() const; // control thread only; null when unarmed
    void clear_notes();
    bool set_fake_endpoints(std::vector<midi::Endpoint>); // internal fake test surface only
    bool inject(const midi::Endpoint &, const uint8_t *, size_t, uint64_t);
    bool inject_ump(const midi::Endpoint &, const uint32_t *, size_t, uint64_t);
};
Json audio_devices(const std::string &directory, bool fake);
struct MidiDeviceListing {
    std::vector<midi::Endpoint> available, enabled;
};
Json midi_devices(const MidiDeviceListing &, bool authorized);
} // namespace wirescape::sidecar
