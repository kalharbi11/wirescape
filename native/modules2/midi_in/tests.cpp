// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../io/midi/module_test_support.h"
using namespace wirescape::midi;
int main() {
    MidiRig<MidiIn> r; r.values[0] = 1;
    r.run({midi1(0x90, 60, 127, 6), midi1(0x91, 62, 127, 9), midi1(0xf8, 0, 0, 10)});
    REQUIRE(r.emitted.size() == 2 && r.emitted[0].offset == 6 && r.emitted[1].offset == 10);
    REQUIRE(r.emitted[0].ump[0] == 0x20903c7fu); r.emitted.clear(); r.values[0] = 0;
    const auto second = midi2(0x91, 60, 0, 9, 2);
    r.run({midi1(0x90, 60, 0, 6, 1), second}); REQUIRE(r.emitted.size() == 3 && r.emitted[0].type == WS2_NOTE_CHOKE && r.emitted[0].source_id == 1);
    REQUIRE(r.emitted[1].ump_words == 1 && r.emitted[2].ump_words == 2 && r.emitted[2].source_id == 2);
    r.emitted.clear(); r.event_limit = 0; r.run({midi1(0x80, 60, 0, 6)}); REQUIRE(r.emitted.empty());
    r.event_limit = 20000; r.run(); REQUIRE(r.emitted.size() == 1 && r.emitted[0].offset == 0 && ((r.emitted[0].ump[0] >> 20) & 15) == 8);
    r.emitted.clear(); r.run({source_choke(2, 7)}); REQUIRE(r.emitted.size() == 1 && r.emitted[0].type == WS2_NOTE_CHOKE && r.emitted[0].source_id == 2);
}
