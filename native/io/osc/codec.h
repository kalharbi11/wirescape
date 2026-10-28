// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <string_view>
namespace wirescape::osc {
struct Argument { char type = 0; int32_t integer = 0; float real = 0; const uint8_t* data = nullptr; uint32_t size = 0; };
struct Message { std::string_view address; std::array<Argument, 32> arguments{}; uint32_t count = 0; };
inline uint32_t big32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
inline void put32(uint8_t* p, uint32_t v) { for (uint32_t i = 0; i < 4; ++i) p[i] = uint8_t(v >> ((3 - i) * 8)); }
inline bool string(const uint8_t* bytes, size_t size, size_t& at, std::string_view& result) {
    if (at >= size) return false; const size_t begin = at;
    while (at < size && bytes[at]) ++at; if (at == size) return false;
    result = {reinterpret_cast<const char*>(bytes + begin), at - begin}; const size_t end = (at + 4) & ~size_t(3);
    if (end > size) return false; for (; at < end; ++at) if (bytes[at]) return false; return true;
}
template<class Emit> bool parse(const uint8_t* bytes, size_t size, Emit emit, uint32_t depth = 0) {
    if (!bytes || !size || size > 65507 || (size & 3) || depth > 4) return false;
    if (size >= 8 && std::memcmp(bytes, "#bundle\0", 8) == 0) {
        if (size < 16 || depth == 4) return false;
        for (size_t at = 16; at < size;) { if (size - at < 4) return false; const uint32_t n = big32(bytes + at); at += 4; if (!n || n > size - at || !parse(bytes + at, n, emit, depth + 1)) return false; at += n; }
        return true; // timetags deliberately ignored for J
    }
    Message message; size_t at = 0; std::string_view tags;
    if (!string(bytes, size, at, message.address) || message.address.empty() || message.address.size() > 255 || message.address[0] != '/' || !string(bytes, size, at, tags) || tags.empty() || tags[0] != ',' || tags.size() > 33) return false;
    for (char c : message.address) if (static_cast<unsigned char>(c) < 33) return false;
    message.count = uint32_t(tags.size() - 1);
    for (uint32_t i = 0; i < message.count; ++i) {
        auto& arg = message.arguments[i]; arg.type = tags[i + 1];
        if (arg.type == 'i' || arg.type == 'f') {
            if (size - at < 4) return false; uint32_t bits = big32(bytes + at); at += 4;
            if (arg.type == 'i') std::memcpy(&arg.integer, &bits, 4); else std::memcpy(&arg.real, &bits, 4);
        } else if (arg.type == 's') { std::string_view text; if (!string(bytes, size, at, text)) return false; arg.data = reinterpret_cast<const uint8_t*>(text.data()); arg.size = uint32_t(text.size()); }
        else if (arg.type == 'b') {
            if (size - at < 4) return false; const uint32_t n = big32(bytes + at); at += 4; if (n > size - at) return false;
            arg.data = bytes + at; arg.size = n; const size_t end = at + ((size_t(n) + 3) & ~size_t(3)); if (end > size) return false;
            for (size_t pad = at + n; pad < end; ++pad) if (bytes[pad]) return false; at = end;
        } else if (arg.type != 'T' && arg.type != 'F' && arg.type != 'N' && arg.type != 'I') return false; // arrays rejected
    }
    if (at != size) return false; emit(message); return true;
}
// Encode a valid message into the ABI payload bound, truncating only string/blob
// contents while retaining a valid complete packet and all numeric arguments.
inline size_t encode(const Message& m, uint8_t* out, size_t capacity, bool& truncated) {
    if (capacity < 1024 || m.address.size() > 255 || m.count > 32) return 0;
    size_t at = 0;
    auto text = [&](const uint8_t* data, size_t n) { const size_t padded = (n + 4) & ~size_t(3); if (padded > capacity - at) return false; if (n) std::memcpy(out + at, data, n); std::memset(out + at + n, 0, padded - n); at += padded; return true; };
    if (!text(reinterpret_cast<const uint8_t*>(m.address.data()), m.address.size())) return 0;
    uint8_t tags[34]{}; tags[0] = ','; for (uint32_t i = 0; i < m.count; ++i) tags[i + 1] = uint8_t(m.arguments[i].type); if (!text(tags, m.count + 1)) return 0;
    for (uint32_t i = 0; i < m.count; ++i) {
        const auto& arg = m.arguments[i]; const size_t reserve = (m.count - i - 1) * 8;
        if (arg.type == 'i' || arg.type == 'f') { uint32_t bits; if (arg.type == 'i') std::memcpy(&bits, &arg.integer, 4); else std::memcpy(&bits, &arg.real, 4); put32(out + at, bits); at += 4; }
        else if (arg.type == 's') { const size_t n = std::min<size_t>(arg.size, (capacity - at - reserve) / 4 * 4 - 1); truncated = truncated || n != arg.size; if (!text(arg.data, n)) return 0; }
        else if (arg.type == 'b') { const size_t n = std::min<size_t>(arg.size, (capacity - at - reserve - 4) / 4 * 4); truncated = truncated || n != arg.size; put32(out + at, uint32_t(n)); at += 4; if (n) std::memcpy(out + at, arg.data, n); const size_t padded = (n + 3) & ~size_t(3); std::memset(out + at + n, 0, padded - n); at += padded; }
    }
    return at;
}
inline bool numeric(const Argument& arg, double& value) {
    if (arg.type == 'i') value = arg.integer; else if (arg.type == 'f') value = arg.real; else if (arg.type == 'T' || arg.type == 'F') value = arg.type == 'T' ? 1 : 0; else return false; return true;
}
}
