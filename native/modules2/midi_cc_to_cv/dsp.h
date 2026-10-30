// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
#include "config.h"
namespace wirescape::midi {
struct CcToCv {
    static constexpr ws::Info info{"midi.cc_to_cv", "CC to CV", 1};
    static constexpr std::array inputs{ws::port("midi", "MIDI", WS2_EV_MIDI, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array outputs{ws::port("cc1", "CC 1", WS2_CV_BI), ws::port("cc2", "CC 2", WS2_CV_BI), ws::port("cc3", "CC 3", WS2_CV_BI), ws::port("cc4", "CC 4", WS2_CV_BI), ws::port("cc5", "CC 5", WS2_CV_BI), ws::port("cc6", "CC 6", WS2_CV_BI), ws::port("cc7", "CC 7", WS2_CV_BI), ws::port("cc8", "CC 8", WS2_CV_BI)};
    static constexpr std::array<ws2_param, 0> params{};
    CcConfiguration config;
    // Match the service's full 16-endpoint x 16-channel identity space.
    static constexpr size_t identity_capacity = 16 * 16;
    struct Row {
        double current = 0, target = 0, coefficient = 1;
        std::array<uint8_t, identity_capacity> msb{}, lsb{};
    };
    std::array<Row, 8> rows{};
    struct Source { bool used = false; uint32_t id = 0, channel = 0; };
    std::array<Source, identity_capacity> sources{};
    uint64_t dropped = 0;
    int token(uint32_t id, uint32_t channel) {
        int free = -1;
        for (uint32_t i = 0; i < sources.size(); ++i) { if (sources[i].used && sources[i].id == id && sources[i].channel == channel) return int(i); if (!sources[i].used && free < 0) free = int(i); }
        if (free < 0) return -1;
        sources[free] = {true, id, channel}; return free;
    }
    bool prepare(const ws::Setup& s) {
        if (!cc_configuration(s.config.data, s.config.bytes, config)) return false;
        for (uint32_t i = 0; i < rows.size(); ++i) rows[i].coefficient = config.rows[i].smoothing_ms > 0 ? 1. - std::exp(-1000. / (config.rows[i].smoothing_ms * s.sample_rate())) : 1.;
        return true;
    }
    void reset() { sources = {}; dropped = 0; for (uint32_t i = 0; i < rows.size(); ++i) { const double k = rows[i].coefficient; rows[i] = {}; rows[i].coefficient = k; rows[i].current = rows[i].target = config.rows[i].bipolar ? -1 : 0; } }
    void event(const ws2_event& e) {
        if (e.type == WS2_NOTE_CHOKE) {
            for (uint32_t t = 0; t < sources.size(); ++t) if (sources[t].used && (e.source_id == UINT32_MAX || sources[t].id == e.source_id)) { sources[t] = {}; for (auto& row : rows) row.msb[t] = row.lsb[t] = 0; }
            return; // continuous controls hold their last target on disconnect
        }
        if (e.type != WS2_MIDI || !e.ump_words) return;
        const uint32_t type = e.ump[0] >> 28;
        if ((e.ump[0] >> 24) & 15) return; // J maps group zero; other groups remain available on raw MIDI.
        if ((type != 2 && type != 4) || ((e.ump[0] >> 20) & 15) != 11 || (type == 4 && e.ump_words < 2)) return;
        const uint32_t channel = (e.ump[0] >> 16) & 15, controller = (e.ump[0] >> 8) & 127;
        for (uint32_t i = 0; i < rows.size(); ++i) {
            const auto& cfg = config.rows[i]; auto& row = rows[i];
            if (cfg.channel && channel != cfg.channel - 1) continue;
            double normalized;
            if (type == 4) { if (controller != cfg.controller) continue; normalized = double(e.ump[1]) / 4294967295.; }
            else if (cfg.fourteen_bit) {
                if (controller != cfg.controller && controller != cfg.controller + 32) continue;
                const int source = token(e.source_id, channel); if (source < 0) { ++dropped; continue; }
                if (controller == cfg.controller) row.msb[source] = uint8_t(e.ump[0] & 127); else row.lsb[source] = uint8_t(e.ump[0] & 127);
                normalized = double(uint32_t(row.msb[source]) * 128 + row.lsb[source]) / 16383.;
            } else { if (controller != cfg.controller) continue; normalized = double(e.ump[0] & 127) / 127.; }
            row.target = cfg.bipolar ? normalized * 2 - 1 : normalized;
        }
    }
    void process(ws::Block& b) {
        for (uint32_t p = 0; p < 8; ++p) { b.out(p).set_channels(1); b.out(p).mark_constant(false); }
        b.split([&](uint32_t first, uint32_t end) {
            for (uint32_t p = 0; p < 8; ++p) for (uint32_t i = first; i < end; ++i) {
                auto& row = rows[p]; row.current += row.coefficient * (row.target - row.current);
                if (std::abs(row.current - row.target) < 1e-7) row.current = row.target;
                b.out(p).ch(0)[i] = float(row.current);
            }
        }, [&](const ws2_event& e) { event(e); });
    }
    int save_state(ws::Writer w) { return w.u32(1) ? WS2_OK : WS2_CAPACITY; }
    int load_state(ws::Reader r) { uint32_t v; if (!r.u32(v)) return WS2_INVALID; if (v != 1) return WS2_UNSUPPORTED; reset(); return WS2_OK; }
};
}
