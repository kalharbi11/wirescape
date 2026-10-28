// SPDX-License-Identifier: GPL-3.0-only
#include "codec.h"
#include "pattern.h"
#include "../../sdk/examples/test_support.h"
#include <vector>
using namespace wirescape::osc;
static std::vector<uint8_t> packet(const char* address, float value) {
    Message m; m.address = address; m.count = 1; m.arguments[0].type = 'f'; m.arguments[0].real = value;
    std::vector<uint8_t> out(1472); bool truncated = false; out.resize(encode(m, out.data(), out.size(), truncated)); REQUIRE(!truncated && !out.empty()); return out;
}
int main() {
    const auto valid = packet("/pico1/knob/3", .75f); int calls = 0;
    REQUIRE(parse(valid.data(), valid.size(), [&](const Message& m) { REQUIRE(m.address == "/pico1/knob/3" && m.count == 1 && m.arguments[0].real == .75f); ++calls; })); REQUIRE(calls == 1);
    for (size_t n = 0; n < valid.size(); ++n) REQUIRE(!parse(valid.data(), n, [](const Message&) {}));
    for (uint32_t n = 1; n < 2048; ++n) { std::vector<uint8_t> garbage(n, 0xab); REQUIRE(!parse(garbage.data(), garbage.size(), [](const Message&) {})); }
    auto bad = valid; bad[16] = '['; REQUIRE(!parse(bad.data(), bad.size(), [](const Message&) {}));
    Pattern p;
    for (const char* pattern : {"/pico1/knob/*", "/pico?/knob/[1-9]", "/{pico1,esp32}/knob/3"}) { REQUIRE(p.compile(pattern)); REQUIRE(p.match("/pico1/knob/3")); REQUIRE(!p.match("/pico1/knob/3/extra")); }
    REQUIRE(p.compile("/a/[!0-3]/{x,{y,z}}")); REQUIRE(p.match("/a/5/z") && !p.match("/a/1/z")); REQUIRE(!p.compile("/{oops")); REQUIRE(!p.compile("/[z-a]"));
    Message m; m.address = "/all"; m.count = 8; const char tags[] = "ifsbTFNI";
    std::vector<uint8_t> text(3000, 'x');
    for (uint32_t i = 0; i < 8; ++i) { m.arguments[i].type = tags[i]; m.arguments[i].integer = -123; m.arguments[i].real = -.5f; m.arguments[i].data = text.data(); m.arguments[i].size = uint32_t(text.size()); }
    uint8_t encoded[1472]{}; bool truncated = false; const size_t size = encode(m, encoded, sizeof(encoded), truncated); REQUIRE(truncated && size <= sizeof(encoded));
    REQUIRE(parse(encoded, size, [&](const Message& result) { REQUIRE(result.count == 8 && result.arguments[0].integer == -123 && result.arguments[1].real == -.5f && result.arguments[2].size < text.size()); }));
    std::vector<uint8_t> bundle(16); std::memcpy(bundle.data(), "#bundle\0", 8); const size_t first = bundle.size(); bundle.resize(first + 4 + valid.size()); put32(bundle.data() + first, uint32_t(valid.size())); std::copy(valid.begin(), valid.end(), bundle.begin() + first + 4);
    calls = 0; REQUIRE(parse(bundle.data(), bundle.size(), [&](const Message&) { ++calls; })); REQUIRE(calls == 1);
    auto nested = bundle;
    for (int depth = 0; depth < 4; ++depth) { std::vector<uint8_t> next(20 + nested.size()); std::memcpy(next.data(), "#bundle\0", 8); put32(next.data() + 16, uint32_t(nested.size())); std::copy(nested.begin(), nested.end(), next.begin() + 20); nested = std::move(next); }
    REQUIRE(!parse(nested.data(), nested.size(), [](const Message&) {}));
}
