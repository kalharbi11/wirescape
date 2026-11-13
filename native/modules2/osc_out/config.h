// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../io/osc/out_bus.h"
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wirescape::osc {
struct OutRowConfig { char address[64]{}; uint32_t kind = 0; /*0 cv, 1 gate*/ float deadband = 0.001f; uint32_t enabled = 1; };
struct OutConfiguration {
    uint32_t version = 1, port = 9001, rate_hz = 100;
    std::array<OutRowConfig, 8> rows{};          // defaults "/ws/row/<n>"
    OutRowConfig notes{};                        // defaults "/ws/note", enabled = 0; kind/deadband unused
    OutBus *bus = nullptr;                       // host-owned; outlives the module instance
};
inline bool out_configuration(const uint8_t *data, uint32_t bytes, OutConfiguration &c) {
    if (bytes) {
        if (!data || bytes != sizeof(c)) return false;
        std::memmove(&c, data, sizeof(c));
    } else {
        c = OutConfiguration{};
        for (uint32_t i = 0; i < c.rows.size(); ++i)
            std::snprintf(c.rows[i].address, sizeof(c.rows[i].address), "/ws/row/%u", i + 1);
        std::strcpy(c.notes.address, "/ws/note");
        c.notes.enabled = 0;
    }
    if (c.version != 1 || c.port < 1 || c.port > 65535 || c.rate_hz < 1 || c.rate_hz > 500) return false;
    auto address_ok = [](const char *text, size_t size) {
        const auto *end = static_cast<const char *>(std::memchr(text, 0, size));
        if (!end || end - text < 2 || text[0] != '/') return false;
        for (const char *p = text; p != end; ++p) {
            const unsigned char ch = static_cast<unsigned char>(*p);
            if (ch < 33 || ch > 126 || std::strchr("#*,?[]{}", ch)) return false;
        }
        return true;
    };
    for (const auto &row : c.rows)
        if (!address_ok(row.address, sizeof(row.address)) || row.kind > 1 || row.enabled > 1 ||
            !std::isfinite(row.deadband) || row.deadband < 0 || row.deadband > 1)
            return false;
    return address_ok(c.notes.address, sizeof(c.notes.address)) && c.notes.enabled <= 1;
}
} // namespace wirescape::osc
