// SPDX-License-Identifier: GPL-3.0-only
#include "monitor_log.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) std::abort();                                                                                        \
    } while (0)
int main() {
    AppMonitorTap tap;
    tap.node = 12;
    tap.prefix = "controller";
    tap.tap = std::make_shared<wirescape::monitor::Tap>();
    wirescape::monitor::Entry e;
    e.type = WS2_MESSAGE;
    const uint8_t packet[] = {'/', 'k', 'n', 'o', 'b', 0, 0, 0, ',', 'f', 0, 0, 0x3f, 0, 0, 0};
    std::memcpy(e.payload, packet, sizeof(packet));
    e.bytes = sizeof(packet);
    auto text = format_monitor_entry(tap, e);
    CHECK(text.find("OSC /knob f:0.5") != std::string::npos);
    e.type = WS2_MIDI;
    uint32_t word = 0x20b04a40;
    std::memcpy(e.payload, &word, 4);
    e.bytes = 4;
    CHECK(format_monitor_entry(tap, e).find("CC 74") != std::string::npos);
    e.type = wirescape::monitor::signal_type | 1;
    e.key = 2;
    float f = .25;
    std::memcpy(e.payload, &f, 4);
    CHECK(format_monitor_entry(tap, e).find("ch 3 = 0.25") != std::string::npos);
    e.bytes = 0x80000000u;
    CHECK(format_monitor_entry(tap, e).find("[truncated]") != std::string::npos);
    for (uint32_t type : {WS2_MESSAGE, WS2_MIDI, WS2_NOTE_ON})
        for (uint32_t bytes = 0; bytes <= 104; ++bytes) {
            e = {};
            e.type = type;
            e.bytes = bytes;
            CHECK(!format_monitor_entry(tap, e).empty());
        }
    std::puts("PASS control-only Monitor OSC/MIDI/signal decoding and short prefixes");
}
