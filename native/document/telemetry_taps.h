// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// Host-side telemetry topology and non-render reader shared by the sidecar and web.
// No DSP state or module internals are inspected here.
#include "app_engine.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace wirescape::document::telemetry {
using Json = nlohmann::json;
using Tap = engine2::Tap;
enum class Field { Effective, Envelope, Phase, EventActivity };
struct Slot {
    Field field;
    uint32_t node;
    std::string name;
};
struct Plan {
    std::vector<Tap> taps;
    std::vector<Slot> slots;
};

// The map is deliberately host-owned: only cabled parameters, the two envelope
// modules and every event output are observed. Phase is enabled by B-wire (#359).
inline Plan plan(const Json &patch, AppEngine *catalogue, bool phase_enabled = false) {
    Plan result;
    std::map<uint32_t, const ws2_descriptor *> modules;
    for (const auto &m : patch.at("modules")) {
        const auto id = m.at("id").get<uint32_t>();
        const auto type = m.at("module").get<std::string>();
        auto *descriptor = app_engine_native_descriptor(catalogue, type.c_str());
        if (!descriptor) throw std::runtime_error("Telemetry module unavailable");
        modules.emplace(id, descriptor);
    }
    auto add = [&](Tap tap, Field field, std::string name) {
        result.taps.push_back(tap);
        result.slots.push_back({field, tap.node, std::move(name)});
    };
    std::set<std::pair<uint32_t, uint32_t>> parameters;
    for (const auto &c : patch.at("connections")) {
        if (c.at("kind") != "modulation") continue;
        auto id = c.at("to").at("module").get<uint32_t>();
        const auto name = c.at("to").at("port").get<std::string>();
        auto *descriptor = modules.at(id);
        for (uint32_t i = 0; i < descriptor->param_count; ++i)
            if (name == descriptor->params[i].id && parameters.emplace(id, i).second)
                add({Tap::Param, id, i, Tap::First}, Field::Effective, name);
    }
    for (const auto &[id, descriptor] : modules) {
        const std::string type = descriptor->id;
        const bool envelope = type == "essentials.adsr" || type == "essentials.function_envelope";
        for (uint32_t i = 0; i < descriptor->output_count; ++i) {
            const auto &port = descriptor->outputs[i];
            if (envelope && port.kind == WS2_SIGNAL && std::string(port.id) == "envelope")
                add({Tap::Output, id, i, Tap::Maximum}, Field::Envelope, "");
            if (phase_enabled && type == "essentials.lfo" && port.kind == WS2_SIGNAL &&
                std::string(port.id) == "phase")
                add({Tap::Output, id, i, Tap::First}, Field::Phase, "");
            if (port.kind == WS2_EVENT)
                add({Tap::EventOut, id, i, Tap::Maximum}, Field::EventActivity, port.id);
        }
    }
    return result;
}

class Sampler {
    Plan plan_;
    std::vector<float> values_, pulse_;
    std::vector<uint32_t> counts_, previous_;
    uint64_t last_frame_ = 0;

  public:
    Sampler() = default;
    explicit Sampler(Plan p)
        : plan_(std::move(p)), values_(plan_.taps.size()), pulse_(plan_.taps.size()),
          counts_(plan_.taps.size()), previous_(plan_.taps.size()) {}
    const std::vector<Tap> &taps() const noexcept { return plan_.taps; }
    // A graph commit replaces every cursor, even when node IDs and tap keys survive.
    // Engine2 resets its cumulative EventOut counts to zero at that same commit.
    void swap(Sampler &other) noexcept {
        plan_.taps.swap(other.plan_.taps);
        plan_.slots.swap(other.plan_.slots);
        values_.swap(other.values_);
        pulse_.swap(other.pulse_);
        counts_.swap(other.counts_);
        previous_.swap(other.previous_);
        std::swap(last_frame_, other.last_frame_);
    }
    Json sample(AppEngine *engine) {
        Json out = {{"effective", Json::object()}, {"phase", Json::object()},
                    {"envelope", Json::object()}, {"eventActivity", Json::object()}};
        if (!engine) return out;
        uint64_t frame = 0;
        const auto count = app_engine_native(engine).read_telemetry(
            values_.data(), counts_.data(), static_cast<uint32_t>(values_.size()), frame);
        if (count != plan_.taps.size() || frame == 0 || frame == last_frame_) return out;
        last_frame_ = frame;
        for (size_t i = 0; i < plan_.slots.size(); ++i) {
            const auto &slot = plan_.slots[i];
            const auto id = std::to_string(slot.node);
            if (slot.field == Field::EventActivity) {
                pulse_[i] = counts_[i] != previous_[i] ? 1.f : pulse_[i] * 0.6f;
                previous_[i] = counts_[i];
                if (pulse_[i] >= 0.05f) out["eventActivity"][id + ":" + slot.name] = pulse_[i];
            } else if (std::isfinite(values_[i])) {
                if (slot.field == Field::Effective) out["effective"][id][slot.name] = values_[i];
                if (slot.field == Field::Envelope)
                    out["envelope"][id] = std::clamp(values_[i], 0.f, 1.f);
                if (slot.field == Field::Phase && values_[i] >= 0.f && values_[i] < 1.f)
                    out["phase"][id] = values_[i];
            }
        }
        return out;
    }
};
} // namespace wirescape::document::telemetry
