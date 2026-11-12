// SPDX-License-Identifier: GPL-3.0-only
// Two device-off Clang libFuzzer targets sharing the production OSC headers.
#include "codec.h"
#include "pattern.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace wirescape::osc::fuzz {
void codec_input(const uint8_t *data, size_t size) {
    uint32_t delivered = 0;
    const bool valid = parse(data, size, [&](const Message &message) {
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
    // Bundles are fully validated before any callback can deliver a message.
    if (!valid && delivered != 0) std::abort();
}

void pattern_input(const uint8_t *data, size_t size) {
    if (!data || !size) return;
    // J5 seeds use LF as the boundary; mutations without LF exercise both halves.
    const auto *separator = static_cast<const uint8_t *>(std::memchr(data, '\n', size));
    const size_t split = separator ? size_t(separator - data) : size / 2;
    const size_t delimiter = separator ? 1 : 0;
    const std::string_view pattern(reinterpret_cast<const char *>(data), split);
    const char *address_data = reinterpret_cast<const char *>(data + split + delimiter);
    const std::string_view address(address_data, size - split - delimiter);
    Pattern first, reference;
    if (first.compile(pattern)) {
        if (!reference.compile(pattern) || first.match(address) != reference.match(address)) std::abort();
    }
}
} // namespace wirescape::osc::fuzz

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
#ifdef WIRESCAPE_OSC_PATTERN
    wirescape::osc::fuzz::pattern_input(data, size);
#else
    wirescape::osc::fuzz::codec_input(data, size);
#endif
    return 0;
}
