// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// J5 C2 (#604): bounded, ingress-owned Monitor observations. No render access.
#include "../io/osc/codec.h"
#include "wirescape/module2.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>

namespace wirescape::monitor {
class Capture {
    struct Entry {
        double time = 0;
        char address[65]{};
        std::array<double, 4> args{};
        int note = -1;
        uint8_t count = 0, kind = 0; // 1=OSC, 2=MIDI
    };
    struct Monitor {
        uint32_t id = 0, written = 0, count = 0;
        std::array<Entry, 32> entries{};
    };
    struct Link {
        uint32_t source = 0, monitor = 0;
        uint8_t kind = 0;
    };
    std::array<Monitor, 64> monitors_{};
    std::array<Link, 128> links_{};
    uint32_t monitor_count_ = 0, link_count_ = 0;
    uint64_t epoch_ns_ = 0;
    mutable std::mutex mutex_;

    static uint64_t now_ns() noexcept {
        return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    void append(uint32_t source, uint8_t kind, Entry entry, uint64_t ns) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        entry.time = ns >= epoch_ns_ ? double(ns - epoch_ns_) / 1e9 : 0.;
        for (uint32_t i = 0; i < link_count_; ++i) {
            const auto &link = links_[i];
            if (link.source != source || link.kind != kind) continue;
            auto &m = monitors_[link.monitor];
            m.entries[m.written % m.entries.size()] = entry;
            ++m.written;
            if (m.count < m.entries.size()) ++m.count;
        }
    }

  public:
    // Called by the session owner only after a successful graph commit.
    // No new strings/containers are allocated on subsequent ingress callbacks.
    void configure(const nlohmann::json &patch) {
        std::lock_guard<std::mutex> lock(mutex_);
        monitors_ = {};
        links_ = {};
        monitor_count_ = link_count_ = 0;
        epoch_ns_ = now_ns();
        const auto &modules = patch.at("modules");
        for (const auto &m : modules) {
            if (m.value("module", std::string{}) != "monitor") continue;
            if (monitor_count_ == monitors_.size()) break;
            monitors_[monitor_count_++].id = m.at("id").get<uint32_t>();
        }
        for (const auto &c : patch.at("connections")) {
            if (link_count_ == links_.size() || c.value("kind", std::string{}) != "event") continue;
            const auto &from = c.at("from"), &to = c.at("to");
            if (to.value("port", std::string{}) != "events") continue;
            const uint32_t source = from.at("module").get<uint32_t>();
            const uint32_t target = to.at("module").get<uint32_t>();
            const auto *src = [&]() -> const nlohmann::json * {
                for (const auto &m : modules)
                    if (m.at("id").get<uint32_t>() == source) return &m;
                return nullptr;
            }();
            if (!src) continue;
            const auto type = src->value("module", std::string{});
            const auto port = from.value("port", std::string{});
            const uint8_t kind = type == "osc.in" && port == "messages" ? 1 :
                                 type == "midi.in" && port == "midi" ? 2 : 0;
            if (!kind) continue;
            for (uint32_t i = 0; i < monitor_count_; ++i)
                if (monitors_[i].id == target) {
                    links_[link_count_++] = {source, i, kind};
                    break;
                }
        }
    }

    // OSC service worker; its packet is already parsed/bounded on ingress.
    void osc(uint32_t source, const ws2_event &event, uint64_t timestamp_ns) noexcept {
        if (event.type != WS2_MESSAGE || event.message_kind != WS2_MSG_OSC || !event.data || !event.bytes)
            return;
        wirescape::osc::parse(static_cast<const uint8_t *>(event.data), event.bytes, [&](const wirescape::osc::Message &message) {
            Entry entry;
            entry.kind = 1;
            const size_t size = std::min<size_t>(64, message.address.size());
            std::memcpy(entry.address, message.address.data(), size);
            for (uint32_t i = 0; i < message.count && entry.count < entry.args.size(); ++i) {
                double value = 0;
                if (!wirescape::osc::numeric(message.arguments[i], value) || !std::isfinite(value)) continue;
                entry.args[entry.count++] = value;
            }
            append(source, 1, entry, timestamp_ns);
        });
    }

    // MIDI dispatch worker, not the audio render or device callback.
    void midi(uint32_t source, const ws2_event &event, uint64_t timestamp_ns) noexcept {
        if (event.type != WS2_MIDI || !event.ump_words) return;
        const uint32_t word = event.ump[0], type = word >> 28;
        if (type != 2 && type != 4) return;
        Entry entry;
        entry.kind = 2;
        entry.note = int((word >> 8) & 127);
        entry.args[0] = double((word >> 20) & 15); // MIDI status, 8=off 9=on 11=CC.
        entry.args[1] = type == 2 ? double(word & 127) :
                        event.ump_words > 1 ? double(event.ump[1] >> 16) / 65535. : 0.;
        entry.count = 2;
        append(source, 2, entry, timestamp_ns);
    }

    // The session control thread builds JSON at the existing telemetry rate.
    nlohmann::json snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        nlohmann::json out = nlohmann::json::object();
        for (uint32_t i = 0; i < monitor_count_; ++i) {
            const auto &m = monitors_[i];
            if (!m.count) continue; // explicitly absent, never a fabricated [].
            auto entries = nlohmann::json::array();
            for (uint32_t j = 0; j < m.count; ++j) {
                const auto &e = m.entries[(m.written - m.count + j) % m.entries.size()];
                nlohmann::json args = nlohmann::json::array();
                for (uint32_t a = 0; a < e.count; ++a) args.push_back(e.args[a]);
                nlohmann::json item = {{"t", e.time}, {"kind", e.kind == 1 ? "osc" : "midi"},
                                       {"args", std::move(args)}};
                if (e.kind == 1) item["address"] = e.address;
                else item["note"] = e.note;
                entries.push_back(std::move(item));
            }
            out[std::to_string(m.id)] = std::move(entries);
        }
        return out;
    }
};
} // namespace wirescape::monitor
