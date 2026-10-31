// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../io/osc/codec.h"
#include "../../io/osc/pattern.h"
#include "config.h"
#include "wirescape_sdk.h"
namespace wirescape::osc {
struct OscIn {
    static constexpr ws::Info info{"osc.in", "OSC In", 1};
    static constexpr std::array<ws2_port, 0> inputs{};
    static constexpr std::array outputs{
        ws::port("row1", "Row 1", WS2_CV_BI),
        ws::port("row2", "Row 2", WS2_CV_BI),
        ws::port("row3", "Row 3", WS2_CV_BI),
        ws::port("row4", "Row 4", WS2_CV_BI),
        ws::port("row5", "Row 5", WS2_CV_BI),
        ws::port("row6", "Row 6", WS2_CV_BI),
        ws::port("row7", "Row 7", WS2_CV_BI),
        ws::port("row8", "Row 8", WS2_CV_BI),
        ws::port("messages", "Messages", WS2_EV_MESSAGE, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array<ws2_param, 0> params{};
    struct Voice {
        int32_t index = 0;
        double current = 0, target = 0;
        uint64_t seen = UINT64_MAX;
        uint32_t pulse = 0;
        int32_t last_event = -1;
    };
    struct Row {
        Pattern pattern;
        std::array<Voice, 16> voices{};
        uint32_t count = 1, triggers = 0;
        bool poly = false;
        double coefficient = 1;
    };
    Configuration config;
    Statistics local_statistics;
    Statistics *statistics = &local_statistics;
    std::array<Row, 8> rows{};
    uint32_t maximum = 16;
    double rate = 48000;
    bool prepare(const ws::Setup &s) {
        if (!configuration(s.config.data, s.config.bytes, config)) return false;
        maximum = s.max_channels();
        rate = s.sample_rate();
        statistics = config.statistics ? config.statistics : &local_statistics;
        for (uint32_t i = 0; i < 8; ++i) {
            if (!rows[i].pattern.compile(config.rows[i].pattern)) return false;
            const char *component = std::strrchr(config.rows[i].pattern, '/');
            rows[i].poly = component && std::strpbrk(component, "*?[{");
            rows[i].coefficient =
                config.rows[i].smoothing_ms > 0 ? 1. - std::exp(-1000. / (config.rows[i].smoothing_ms * rate)) : 1;
        }
        return true;
    }
    void reset() {
        for (auto &r : rows) {
            r.voices = {};
            r.count = r.poly ? 0 : 1;
            r.triggers = 0;
        }
    }
    static int32_t index(std::string_view address) {
        const auto slash = address.find_last_of('/');
        const auto component = address.substr(slash == std::string_view::npos ? 0 : slash + 1);
        if (component.empty() || component.size() > 10) return -1;
        uint64_t value = 0;
        for (char c : component) {
            if (c < '0' || c > '9') return -1;
            value = value * 10 + uint32_t(c - '0');
            if (value > INT32_MAX) return -1;
        }
        return int32_t(value);
    }
    int slot(Row &row, std::string_view address, bool create) {
        if (!row.poly) return 0;
        const int32_t wanted = index(address);
        if (wanted < 0) return -1;
        uint32_t at = 0;
        while (at < row.count && row.voices[at].index < wanted) ++at;
        if (at < row.count && row.voices[at].index == wanted) return int(at);
        if (!create || row.count == maximum) return -1;
        for (uint32_t i = row.count; i > at; --i) row.voices[i] = row.voices[i - 1];
        row.voices[at] = {};
        row.voices[at].index = wanted;
        ++row.count;
        return int(at);
    }
    template <class F> static bool message(const ws2_event &e, F f) {
        return e.type == WS2_MESSAGE && e.message_kind == WS2_MSG_OSC && parse(e.data, e.bytes, f);
    }
    void process(ws::Block &b) {
        // Discover numeric wildcard indices first, so channel ordering is stable for
        // the entire block, including samples preceding a newly arrived index.
        for (const auto &e : b.events())
            message(e, [&](const Message &m) {
                for (auto &row : rows)
                    if (row.pattern.match(m.address) && slot(row, m.address, true) < 0) ++statistics->dropped;
            });
        for (auto &row : rows) {
            row.triggers = 0;
            for (auto &v : row.voices) v.last_event = -1;
        }
        int32_t sequence = 0;
        for (const auto &e : b.events()) {
            message(e, [&](const Message &m) {
                for (uint32_t r = 0; r < 8; ++r)
                    if (config.rows[r].kind != 2 && rows[r].pattern.match(m.address)) {
                        const int c = slot(rows[r], m.address, false);
                        if (c >= 0) rows[r].voices[c].last_event = sequence;
                    }
            });
            ++sequence;
        }
        for (uint32_t r = 0; r < 8; ++r) {
            b.out(r).set_channels(std::max(1u, rows[r].count));
            b.out(r).mark_constant(false);
        }
        b.out(8).set_channels(0);
        sequence = 0;
        b.split(
            [&](uint32_t first, uint32_t end) {
                for (uint32_t r = 0; r < 8; ++r)
                    for (uint32_t c = 0; c < std::max(1u, rows[r].count); ++c) {
                        auto &v = rows[r].voices[c];
                        const auto &cfg = config.rows[r];
                        for (uint32_t i = first; i < end; ++i) {
                            if (v.seen != UINT64_MAX && cfg.timeout_ms > 0 && cfg.stale &&
                                b.frame_position() + i - v.seen >= uint64_t(std::ceil(cfg.timeout_ms * rate / 1000.)))
                                v.target = 0;
                            float output;
                            if (cfg.kind == 2) {
                                output = v.pulse ? 1.f : 0.f;
                                if (v.pulse) --v.pulse;
                            } else if (cfg.kind == 1) output = float(v.target);
                            else {
                                v.current += rows[r].coefficient * (v.target - v.current);
                                if (std::abs(v.current - v.target) < 1e-7) v.current = v.target;
                                output = float(v.current);
                            }
                            b.out(r).ch(c)[i] = output;
                        }
                    }
            },
            [&](const ws2_event &e) {
                bool valid = message(e, [&](const Message &m) {
                    ++statistics->received;
                    for (uint32_t r = 0; r < 8; ++r) {
                        auto &row = rows[r];
                        const auto &cfg = config.rows[r];
                        if (!row.pattern.match(m.address)) continue;
                        const int c = slot(row, m.address, false);
                        if (c < 0) continue;
                        auto &v = row.voices[c];
                        if (cfg.kind == 2) {
                            if (row.triggers++ < 64) {
                                v.pulse = std::max(1u, uint32_t(std::ceil(rate * .001)));
                                v.seen = b.frame_position() + e.offset;
                            } else ++statistics->dropped;
                            continue;
                        }
                        if (v.last_event != sequence || cfg.argument >= m.count) continue;
                        double value;
                        if (!numeric(m.arguments[cfg.argument], value) || !std::isfinite(value)) {
                            ++statistics->malformed;
                            continue;
                        }
                        v.target =
                            cfg.kind == 1
                                ? (value > .5 ? 1 : 0)
                                : double(cfg.output_min) +
                                      std::clamp((value - cfg.input_min) / (double(cfg.input_max) - cfg.input_min), 0.,
                                                 1.) *
                                          (double(cfg.output_max) - cfg.output_min);
                        v.seen = b.frame_position() + e.offset;
                    }
                });
                if (e.type == WS2_MESSAGE && e.message_kind == WS2_MSG_OSC) {
                    if (!valid) ++statistics->malformed;
                    else if (!b.emit(8, e)) ++statistics->dropped;
                }
                ++sequence;
            });
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
} // namespace wirescape::osc
