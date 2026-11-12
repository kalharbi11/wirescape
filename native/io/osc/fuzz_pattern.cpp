// SPDX-License-Identifier: GPL-3.0-only
// Device-off libFuzzer target for the production OSC pattern compiler/matcher.
#include "pattern.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (!data || !size) return 0;
    // J5 corpus seeds put a single LF between the pattern and its address.
    // Mutated inputs without LF still exercise both sides of the matcher.
    const auto *separator = static_cast<const uint8_t *>(std::memchr(data, '\n', size));
    const size_t split = separator ? size_t(separator - data) : size / 2;
    const size_t delimiter = separator ? 1 : 0;
    const std::string_view pattern(reinterpret_cast<const char *>(data), split);
    const char *address_data = reinterpret_cast<const char *>(data + split + delimiter);
    const std::string_view address(address_data, size - split - delimiter);
    wirescape::osc::Pattern first, reference;
    if (first.compile(pattern)) {
        // Compile and match are deterministic, including invalid UTF-8 bytes.
        if (!reference.compile(pattern) || first.match(address) != reference.match(address)) std::abort();
    }
    return 0;
}
