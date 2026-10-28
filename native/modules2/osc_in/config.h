// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
namespace wirescape::osc {
struct Statistics { std::atomic<uint32_t> dropped{0}, malformed{0}, received{0}; };
struct RowConfig {
    char pattern[256]{};
    uint32_t argument = 0, kind = 0; // cv/gate/trigger
    float input_min = 0, input_max = 1, output_min = 0, output_max = 1;
    float smoothing_ms = 5, timeout_ms = 1000;
    uint32_t stale = 0; // hold/zero
};
struct Configuration {
    uint32_t version = 1, port = 9000, lan = 0;
    std::array<RowConfig, 8> rows{};
    Statistics* statistics = nullptr; // private host-owned telemetry, never JSON
    Configuration() { for (uint32_t i = 0; i < rows.size(); ++i) std::snprintf(rows[i].pattern, sizeof(rows[i].pattern), "/pico1/knob/%u", i + 1); }
};
inline bool configuration(const uint8_t* data, uint32_t bytes, Configuration& c) {
    if (bytes) { if (!data || bytes != sizeof(c)) return false; std::memmove(&c, data, sizeof(c)); }
    if (c.version != 1 || c.port < 1 || c.port > 65535 || c.lan > 1) return false;
    for (const auto& r : c.rows) if (!std::memchr(r.pattern, 0, sizeof(r.pattern)) || r.pattern[0] != '/' || r.argument >= 32 || r.kind > 2 || r.stale > 1 || !std::isfinite(r.input_min) || !std::isfinite(r.input_max) || r.input_max <= r.input_min || !std::isfinite(r.output_min) || !std::isfinite(r.output_max) || !std::isfinite(r.smoothing_ms) || r.smoothing_ms < 0 || r.smoothing_ms > 2000 || !std::isfinite(r.timeout_ms) || r.timeout_ms < 0 || r.timeout_ms > 3600000) return false;
    return true;
}
}
