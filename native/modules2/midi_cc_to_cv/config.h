// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace wirescape::midi {
// Patch persistence is JSON; typed configuration is a quiescent runtime adapter.
// Keep reading historical 68-byte v1 configurations on every supported host.
struct CcRowV1 {
    uint8_t channel = 0, controller = 1, fourteen_bit = 0, bipolar = 0;
    float smoothing_ms = 5;
};
struct CcConfigurationV1 {
    uint32_t version = 1;
    std::array<CcRowV1, 8> rows{};
    CcConfigurationV1() {
        for (uint32_t i = 0; i < rows.size(); ++i) rows[i].controller = uint8_t(i + 1);
    }
};
static_assert(sizeof(CcRowV1) == 8 && sizeof(CcConfigurationV1) == 68,
              "Legacy CC configuration bytes must remain compatible");

struct CcRow {
    uint8_t channel = 0, controller = 1, fourteen_bit = 0, bipolar = 0;
    float smoothing_ms = 5;
    double min = 0, max = 1;
    uint8_t has_min = 0, has_max = 0;
    double minimum() const { return has_min ? min : (bipolar ? -1. : 0.); }
    double maximum() const { return has_max ? max : 1.; }
};
struct CcConfiguration {
    uint32_t version = 2;
    std::array<CcRow, 8> rows{};
    CcConfiguration() {
        for (uint32_t i = 0; i < rows.size(); ++i) rows[i].controller = uint8_t(i + 1);
    }
};
inline bool cc_configuration(const uint8_t* data, uint32_t bytes, CcConfiguration& config) {
    if (bytes) {
        if (!data || bytes < sizeof(uint32_t)) return false;
        uint32_t version = 0;
        std::memcpy(&version, data, sizeof(version));
        if (version == 1 && bytes == sizeof(CcConfigurationV1)) {
            CcConfigurationV1 legacy;
            std::memcpy(&legacy, data, sizeof(legacy));
            config = CcConfiguration{};
            for (size_t i = 0; i < config.rows.size(); ++i) {
                auto& row = config.rows[i]; const auto& old = legacy.rows[i];
                row.channel = old.channel; row.controller = old.controller;
                row.fourteen_bit = old.fourteen_bit; row.bipolar = old.bipolar;
                row.smoothing_ms = old.smoothing_ms;
            }
        } else if (version == 2 && bytes == sizeof(config)) {
            std::memcpy(&config, data, sizeof(config));
        } else return false;
    }
    if (config.version != 2) return false;
    for (const auto& row : config.rows)
        if (row.channel > 16 || row.controller > 127 || row.fourteen_bit > 1 ||
            row.bipolar > 1 || row.has_min > 1 || row.has_max > 1 ||
            (row.fourteen_bit && row.controller > 31) ||
            !std::isfinite(row.smoothing_ms) || row.smoothing_ms < 0 || row.smoothing_ms > 2000 ||
            !std::isfinite(row.min) || !std::isfinite(row.max)) return false;
    return true;
}
} // namespace wirescape::midi
