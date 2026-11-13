// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../io/osc/out_bus.h"
#include <array>
#include <cstdint>

namespace wirescape::osc {
struct OutRowConfig { char address[64]{}; uint32_t kind = 0; /*0 cv, 1 gate*/ float deadband = 0.001f; uint32_t enabled = 1; };
struct OutConfiguration {
    uint32_t version = 1, port = 9001, rate_hz = 100;
    std::array<OutRowConfig, 8> rows{};          // defaults "/ws/row/<n>"
    OutRowConfig notes{};                        // defaults "/ws/note", enabled = 0; kind/deadband unused
    OutBus *bus = nullptr;                       // host-owned; outlives the module instance
};
bool out_configuration(const uint8_t *data, uint32_t bytes, OutConfiguration &c); // like configuration()
} // namespace wirescape::osc
