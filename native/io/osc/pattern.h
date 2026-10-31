// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
namespace wirescape::osc {
// A bounded NFA indexed by pattern bytes. No regex engine, recursion or allocation.
class Pattern {
    enum Kind : uint8_t { Literal, Any, Star, Class, Alternatives, Epsilon, End, Unused };
    struct Node {
        Kind kind = Unused;
        uint16_t next = 0;
        uint8_t literal = 0;
        std::array<uint64_t, 4> mask{};
    };
    std::array<Node, 257> nodes_{};
    std::array<uint16_t, 257> alternatives_{};
    uint16_t size_ = 0;
    void closure(uint16_t start, std::array<uint16_t, 257> &active, uint16_t &count,
                 std::array<bool, 257> &seen) const {
        std::array<uint16_t, 257> queue{};
        uint16_t used = 0;
        auto add = [&](uint16_t p) {
            if (p <= size_ && !seen[p]) {
                seen[p] = true;
                queue[used++] = p;
            }
        };
        add(start);
        for (uint16_t at = 0; at < used; ++at) {
            const auto p = queue[at];
            const auto &n = nodes_[p];
            if (n.kind == Alternatives) {
                for (uint16_t q = p + 1; q != UINT16_MAX; q = alternatives_[q]) add(q);
            } else if (n.kind == Epsilon) add(n.next);
            else if (n.kind != Unused) {
                active[count++] = p;
                if (n.kind == Star) add(n.next);
            }
        }
    }

  public:
    bool compile(std::string_view pattern) {
        if (pattern.size() > 255) return false;
        size_ = uint16_t(pattern.size());
        nodes_ = {};
        alternatives_.fill(UINT16_MAX);
        std::array<uint16_t, 4> stack{};
        std::array<uint16_t, 257> last{}, owner{};
        owner.fill(UINT16_MAX);
        uint32_t depth = 0;
        for (uint16_t p = 0; p < size_; ++p) {
            auto &n = nodes_[p];
            n.next = p + 1;
            const uint8_t c = uint8_t(pattern[p]);
            if (c == '{') {
                if (depth == stack.size()) return false;
                n.kind = Alternatives;
                stack[depth++] = p;
                last[p] = p + 1;
            } else if (c == ',') {
                if (!depth) return false;
                n.kind = Epsilon;
                const auto open = stack[depth - 1];
                alternatives_[last[open]] = p + 1;
                last[open] = p + 1;
                owner[p] = open;
            } else if (c == '}') {
                if (!depth) return false;
                const auto open = stack[--depth];
                n.kind = Epsilon;
                for (uint16_t q = open + 1; q < p; ++q)
                    if (owner[q] == open) nodes_[q].next = p + 1;
            } else if (c == '*') n.kind = Star;
            else if (c == '?') n.kind = Any;
            else if (c == '[') {
                n.kind = Class;
                uint16_t q = p + 1;
                bool negate = q < size_ && pattern[q] == '!';
                if (negate) ++q;
                bool any = false;
                for (; q < size_ && pattern[q] != ']'; ++q) {
                    uint8_t lo = uint8_t(pattern[q]), hi = lo;
                    if (q + 2 < size_ && pattern[q + 1] == '-' && pattern[q + 2] != ']') {
                        hi = uint8_t(pattern[q + 2]);
                        q += 2;
                        if (hi < lo) return false;
                    }
                    for (uint32_t v = lo; v <= hi; ++v) n.mask[v / 64] |= uint64_t(1) << (v % 64);
                    any = true;
                }
                if (!any || q == size_) return false;
                if (negate)
                    for (auto &word : n.mask) word = ~word;
                n.mask['/' / 64] &= ~(uint64_t(1) << ('/' % 64));
                n.next = q + 1;
                p = q;
            } else if (c == ']') return false;
            else {
                n.kind = Literal;
                if (c == '\\') {
                    if (++p == size_) return false;
                    n.next = p + 1;
                    n.literal = uint8_t(pattern[p]);
                } else n.literal = c;
            }
        }
        if (depth) return false;
        nodes_[size_].kind = End;
        return true;
    }
    bool match(std::string_view address) const {
        if (address.size() > 255) return false;
        std::array<uint16_t, 257> active{}, next{};
        std::array<bool, 257> seen{};
        uint16_t count = 0;
        closure(0, active, count, seen);
        for (uint8_t c : address) {
            uint16_t n = 0;
            seen.fill(false);
            for (uint16_t i = 0; i < count; ++i) {
                const auto p = active[i];
                const auto &token = nodes_[p];
                if ((token.kind == Literal && token.literal == c) || (token.kind == Any && c != '/') ||
                    (token.kind == Class && ((token.mask[c / 64] >> (c % 64)) & 1)))
                    closure(token.next, next, n, seen);
                else if (token.kind == Star && c != '/') closure(p, next, n, seen);
            }
            active = next;
            count = n;
            if (!count) return false;
        }
        for (uint16_t i = 0; i < count; ++i)
            if (nodes_[active[i]].kind == End) return true;
        return false;
    }
};
} // namespace wirescape::osc
