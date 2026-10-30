// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace wirescape::midi {
// Runtime JSON adapter encodes these control-side settings; never serialize raw
// host structs into a patch file. JSON remains the stable document representation.
struct CcRow { uint8_t channel = 0, controller = 1, fourteen_bit = 0, bipolar = 0; float smoothing_ms = 5; };
struct CcConfiguration {
    uint32_t version = 1;
    std::array<CcRow, 8> rows{};
    CcConfiguration() { for (uint32_t i = 0; i < rows.size(); ++i) rows[i].controller = uint8_t(i + 1); }
};
static_assert(sizeof(CcRow) == 8 && sizeof(CcConfiguration) == 68, "CC control configuration layout");
inline bool cc_configuration(const uint8_t* data, uint32_t bytes, CcConfiguration& config) {
    if (bytes) { if (!data || bytes != sizeof(config)) return false; std::memmove(&config, data, sizeof(config)); }
    if (config.version != 1) return false;
    for (const auto& row : config.rows) if (row.channel > 16 || row.controller > 127 || row.fourteen_bit > 1 || row.bipolar > 1 || (row.fourteen_bit && row.controller > 31) || !std::isfinite(row.smoothing_ms) || row.smoothing_ms < 0 || row.smoothing_ms > 2000) return false;
    return true;
}
}
