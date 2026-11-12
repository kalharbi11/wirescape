// SPDX-License-Identifier: GPL-3.0-only
// Device-off libFuzzer target for the production OSC message/bundle parser.
#include "codec.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint32_t delivered = 0;
    const bool valid = wirescape::osc::parse(data, size, [&](const wirescape::osc::Message &message) {
        if (message.address.empty() || message.address.size() > 255 || message.count > 32) std::abort();
        const auto base = reinterpret_cast<std::uintptr_t>(data);
        const auto inside = [&](const void *ptr, size_t bytes) {
            const auto address = reinterpret_cast<std::uintptr_t>(ptr);
            return address >= base && address - base <= size && bytes <= size - (address - base);
        };
        if (!inside(message.address.data(), message.address.size())) std::abort();
        for (uint32_t i = 0; i < message.count; ++i) {
            const auto &arg = message.arguments[i];
            if (arg.type == 's' || arg.type == 'b') {
                if (!inside(arg.data, arg.size)) std::abort();
            } else {
                const bool scalar = arg.type == 'i' || arg.type == 'f' || arg.type == 'T' || arg.type == 'F';
                const bool marker = arg.type == 'N' || arg.type == 'I';
                if (!scalar && !marker) std::abort();
            }
        }
        ++delivered;
    });
    // The parser validates the whole bundle before delivering any message.
    if (!valid && delivered != 0) std::abort();
    return 0;
}
