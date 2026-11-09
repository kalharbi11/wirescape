// SPDX-License-Identifier: GPL-3.0-only
#include "devices.h"
#include "../io/audio/portaudio_service.h"
#include <algorithm>
namespace wirescape::sidecar {
ObservedMidi::ObservedMidi(bool fake, bool authorized)
    : backend_(midi::real_backend(!fake && authorized)), fake_(fake) {}
std::vector<midi::Endpoint> ObservedMidi::endpoints() {
    auto found = backend_ ? backend_->endpoints() : std::vector<midi::Endpoint>{};
    std::lock_guard<std::mutex> lock(mutex_);
    if (fake_) found = fake_endpoints_;
    observed_ = found;
    return found;
}
bool ObservedMidi::set_fake_endpoints(std::vector<midi::Endpoint> endpoints) {
    if (!fake_) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    fake_endpoints_ = std::move(endpoints);
    return true;
}
std::vector<midi::Endpoint> ObservedMidi::observed() {
    std::lock_guard<std::mutex> lock(mutex_);
    return observed_;
}
void ObservedMidi::note(Slot &s, unsigned key, unsigned channel, bool on) {
    if (key > 127 || channel > 15) return;
    if (on) s.notes[key].fetch_or(uint16_t(1u << channel));
    else s.notes[key].fetch_and(uint16_t(~(1u << channel)));
}
void ObservedMidi::message(void *context, const uint8_t *bytes, size_t n, uint64_t time) {
    auto &s = *static_cast<Slot *>(context);
    s.activity.store(time);
    if (n >= 3) {
        unsigned status = bytes[0] >> 4, channel = bytes[0] & 15;
        if (status == 8 || status == 9) note(s, bytes[1], channel, status == 9 && bytes[2] != 0);
        if (status == 11 && (bytes[1] == 120 || bytes[1] == 123))
            for (auto &key : s.notes) key.fetch_and(uint16_t(~(1u << channel)));
    }
    if (s.downstream.message) s.downstream.message(s.downstream.context, bytes, n, time);
}
void ObservedMidi::ump(void *context, const uint32_t *words, size_t n, uint64_t time) {
    auto &s = *static_cast<Slot *>(context);
    s.activity.store(time);
    if (n) {
        auto word = words[0];
        unsigned type = word >> 28, status = (word >> 20) & 15, channel = (word >> 16) & 15;
        if (((word >> 24) & 15) == 0 && (type == 2 || (type == 4 && n >= 2))) {
            if (status == 8 || status == 9)
                note(s, (word >> 8) & 127, channel, status == 9 && (type == 4 || (word & 127) != 0));
            if (status == 11 && (((word >> 8) & 127) == 120 || ((word >> 8) & 127) == 123))
                for (auto &key : s.notes) key.fetch_and(uint16_t(~(1u << channel)));
        }
    }
    if (s.downstream.ump) s.downstream.ump(s.downstream.context, words, n, time);
}
bool ObservedMidi::open(const midi::Endpoint &e, midi::Callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &s : slots_)
        if (s.endpoint.name.empty()) {
            s.endpoint = e;
            s.downstream = cb;
            if (fake_) return true;
            if (backend_ && backend_->open(e, {&s, message, ump})) return true;
            s.endpoint = {};
            return false;
        }
    return false;
}
void ObservedMidi::close(const midi::Endpoint &e) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &s : slots_)
        if (s.endpoint.name == e.name && s.endpoint.system_name == e.system_name) {
            if (backend_) backend_->close(e);
            for (auto &key : s.notes) key.store(0);
            s.activity.store(0);
            s.endpoint = {};
            s.downstream = {};
        }
}
void ObservedMidi::clear_notes() {
    for (auto &s : slots_)
        for (auto &key : s.notes) key.store(0);
}
bool ObservedMidi::inject(const midi::Endpoint &e, const uint8_t *data, size_t size, uint64_t time) {
    if (!fake_) return false;
    for (auto &s : slots_)
        if (s.endpoint.name == e.name && s.endpoint.system_name == e.system_name) {
            message(&s, data, size, time);
            return true;
        }
    return false;
}
bool ObservedMidi::inject_ump(const midi::Endpoint &e, const uint32_t *data, size_t size, uint64_t time) {
    if (!fake_) return false;
    for (auto &s : slots_)
        if (s.endpoint.name == e.name && s.endpoint.system_name == e.system_name) {
            ump(&s, data, size, time);
            return true;
        }
    return false;
}
Json ObservedMidi::activity() {
    Json result = Json::object();
    auto now = app_engine_clock_ns();
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &s : slots_) {
        auto stamp = s.activity.load();
        if (!stamp || s.endpoint.name.empty()) continue;
        double age = now > stamp ? double(now - stamp) / 1e9 : 0.;
        if (age < 0.25) result[s.endpoint.name + "|" + s.endpoint.system_name] = 1. - age / 0.25;
    }
    return result;
}
Json ObservedMidi::held() const {
    auto held = Json::array();
    for (size_t key = 0; key < 128; ++key) {
        bool on = false;
        for (const auto &s : slots_) on = on || s.notes[key].load() != 0;
        if (on) held.push_back(key);
    }
    return held;
}
Json audio_devices(const std::string &directory, bool fake) {
    auto result = Json::array();
    if (fake) return Json::array({{{"id", "USB Stereo"}, {"name", "USB Stereo"}, {"kind", "audio"},
                                    {"index", 3}, {"backend", "FakeAudio"}, {"outputChannels", 2},
                                    {"sampleRates", Json::array({44100, 48000, 96000})}, {"supported", true}},
                                   {{"id", "Mono DAC"}, {"name", "Mono DAC"}, {"kind", "audio"},
                                    {"index", 4}, {"backend", "FakeAudio"}, {"outputChannels", 1},
                                    {"supported", false}, {"unsupportedReason", "mono-output"}}});
    for (const auto &d : audio::devices(directory)) {
        Json entry = {{"id", d.name}, {"name", d.name}, {"kind", "audio"}, {"index", d.index},
                      {"backend", d.backend}, {"outputChannels", d.output_channels}, {"supported", d.supported}};
        if (!d.sample_rates.empty()) entry["sampleRates"] = d.sample_rates;
        if (!d.supported) entry["unsupportedReason"] = d.unsupported_reason;
        result.push_back(std::move(entry));
    }
    return result;
}
Json midi_devices(const MidiDeviceListing &listing, bool authorized) {
    auto result = Json::array();
    for (const auto &e : listing.available) {
        bool on = authorized && std::any_of(listing.enabled.begin(), listing.enabled.end(), [&](const auto &s) {
                      return s.name == e.name && s.system_name == e.system_name;
                  });
        result.push_back({{"id", e.name + "|" + e.system_name},
                          {"name", e.name},
                          {"system_name", e.system_name},
                          {"kind", "midi"},
                          {"enabled", on}});
    }
    return result;
}
} // namespace wirescape::sidecar
