// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape/module2.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace module_cpu_probe {
struct OutputBounds {
    uint32_t frames;
    uint32_t channels;
};
struct OutputEvidence {
    double peak = 0;
    size_t faults = 0;
    void scan(const ws2_signal_out &output, OutputBounds bounds) {
        if (output.channels > bounds.channels) {
            ++faults;
            return;
        }
        const uint32_t count = (output.flags & WS2_CONSTANT) ? 1 : bounds.frames;
        for (uint32_t channel = 0; channel < output.channels; ++channel) {
            if (!output.data || !output.data[channel]) {
                ++faults;
                continue;
            }
            for (uint32_t i = 0; i < count; ++i) {
                const double sample = output.data[channel][i];
                if (!std::isfinite(sample)) ++faults;
                else peak = std::max(peak, std::abs(sample));
            }
        }
    }
};
} // namespace module_cpu_probe
