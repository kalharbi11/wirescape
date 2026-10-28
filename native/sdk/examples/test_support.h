// SPDX-License-Identifier: MIT
// Small helpers shared by the example tests: a fixed host arena, a fixed-capacity buffer
// (the portable-source guard bans <vector> under native/sdk) and state byte sinks/sources.
#pragma once
#include "wirescape_sdk.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#define REQUIRE(x) do { if (!(x)) { std::fprintf(stderr, "example check line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)

// Host arena handed to create/prepare through ws2_host::alloc. Lives until after destroy.
struct Pool { alignas(64) unsigned char bytes[16384]; size_t used = 0; };
inline void* WS2_CALL allocate(void* ctx, size_t bytes, size_t alignment, uint32_t) {
    auto& pool = *static_cast<Pool*>(ctx);
    pool.used = (pool.used + alignment - 1) & ~(alignment - 1);
    if (pool.used + bytes > sizeof(pool.bytes)) return nullptr;
    void* result = pool.bytes + pool.used;
    pool.used += bytes;
    return result;
}

inline bool same_element(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }   // bit identity
template<class T> bool same_element(const T& a, const T& b) { return a == b; }

// Fixed-capacity array with a count. Elements start zeroed.
template<class T, size_t N> struct Fixed {
    T items[N]{};
    size_t count = 0;
    size_t size() const { return count; }
    bool empty() const { return count == 0; }
    T& operator[](size_t i) { return items[i]; }
    const T& operator[](size_t i) const { return items[i]; }
    T& back() { return items[count - 1]; }
    const T* begin() const { return items; }
    const T* end() const { return items + count; }
    void push_back(const T& x) { REQUIRE(count < N); items[count++] = x; }
    void append(const Fixed& other) { for (const T& x : other) push_back(x); }
    void clear() { count = 0; }
    void pop_back() { --count; }
    void resize(size_t n) { REQUIRE(n <= N); for (size_t i = count; i < n; ++i) items[i] = T{}; count = n; }
    bool operator==(const Fixed& o) const {
        if (count != o.count) return false;
        for (size_t i = 0; i < count; ++i) if (!same_element(items[i], o.items[i])) return false;
        return true;
    }
};

// save_state / load_state plumbing.
using Bytes = Fixed<uint8_t, 4096>;
inline int WS2_CALL sink(void* ctx, const void* p, uint32_t n) {
    auto& bytes = *static_cast<Bytes*>(ctx);
    for (uint32_t i = 0; i < n; ++i) bytes.push_back(static_cast<const uint8_t*>(p)[i]);
    return WS2_OK;
}
struct Source { const Bytes* bytes; size_t pos; };
inline int WS2_CALL source(void* ctx, void* p, uint32_t n) {
    auto& s = *static_cast<Source*>(ctx);
    if (s.pos + n > s.bytes->size()) return WS2_INVALID;     // short data: the reader reports failure
    std::memcpy(p, s.bytes->items + s.pos, n);
    s.pos += n;
    return WS2_OK;
}
