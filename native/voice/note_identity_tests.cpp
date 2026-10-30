// SPDX-License-Identifier: MIT
#include "note_to_cv.h"
#include <array>
#include <cstdio>
#include <cstdlib>
using namespace wirescape::voice;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                                       \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (0)
static void source_reuse_tests() {
    NoteToCv core;
    Config config;
    config.max_voices = 2;
    CHECK(core.configure(config) == Status::Ok);
    ws2_event e{};
    e.struct_size = sizeof(e);
    e.key = 60;
    e.value = 1;
    e.note_id = -1;
    ws2_event last{};
    auto emit = [&](const ws2_event &out) { last = out; };
    struct NoteSpec {
        uint32_t source;
        uint16_t type;
        int id = -1;
    };
    auto note = [&](const NoteSpec &spec) {
        e.source_id = spec.source;
        e.type = spec.type;
        e.note_id = spec.id;
        core.event(e, emit);
    };
    auto churn = [&](uint32_t first) {
        for (uint32_t source = first; source < first + 256; ++source) {
            note({source, WS2_NOTE_ON, 7});
            CHECK(core.allocator().active_count() > 0 && last.source_id == source);
            note({source, WS2_NOTE_OFF, 7});
            CHECK(last.type == WS2_NOTE_OFF && last.source_id == source);
        }
    };
    churn(1000);
    CHECK(core.allocator().active_count() == 0);
    note({1, WS2_NOTE_ON});
    core.sustain(1, 0, true, e, emit);
    note({1, WS2_NOTE_OFF});
    churn(2000);
    CHECK(core.allocator().active_count() == 1);
    core.sustain(1, 0, false, e, emit);
    CHECK(core.allocator().active_count() == 0 && last.source_id == 1);
    note({2, WS2_NOTE_ON});
    core.release_all(2, e, emit);
    CHECK(last.type == WS2_NOTE_CHOKE && last.source_id == 2);
    churn(3000);
    config.max_voices = 1;
    core.configure(config);
    note({11, WS2_NOTE_ON});
    note({22, WS2_NOTE_ON});
    CHECK(last.source_id == 22);
    churn(4000);
    note({33, WS2_NOTE_ON, 9});
    note({11, WS2_NOTE_OFF}); // stolen anonymous tombstone must retain its identity
    CHECK(core.allocator().active_count() == 1);
    note({22, WS2_NOTE_OFF});
    CHECK(core.allocator().active_count() == 1);
    note({33, WS2_NOTE_OFF, 9});
    CHECK(core.allocator().active_count() == 0 && last.source_id == 33);
    config.mode = Mode::Mono;
    core.configure(config);
    note({1, WS2_NOTE_ON, 1});
    note({2, WS2_NOTE_ON, 2});
    for (uint32_t source = 5000; source < 5256; ++source) {
        note({source, WS2_NOTE_ON, 7});
        note({source, WS2_NOTE_OFF, 7});
        CHECK(core.allocator().active_count() == 1 && last.source_id == 2);
    }
    note({2, WS2_NOTE_OFF, 2});
    CHECK(core.allocator().active_count() == 1 && last.source_id == 1);
    note({1, WS2_NOTE_OFF, 1});
    CHECK(core.allocator().active_count() == 0 && last.source_id == 1);
}
int main() {
    source_reuse_tests();
    for (int voices : {1, 4}) {
        NoteToCv core;
        Config config;
        config.max_voices = static_cast<uint8_t>(voices);
        CHECK(core.configure(config) == Status::Ok);
        std::array<ws2_event, 16> log{};
        int count = 0;
        auto emit = [&](const ws2_event &e) {
            CHECK(count < 16);
            log[count++] = e;
        };
        ws2_event e{};
        e.struct_size = sizeof(e);
        e.type = WS2_NOTE_ON;
        e.note_id = -1;
        e.source_id = 12;
        e.channel = 0;
        e.key = 60;
        e.value = 1;
        core.event(e, emit);
        CHECK(count == 1);
        int32_t oldest = log[0].note_id;
        CHECK(oldest >= 0);
        core.event(e, emit);
        int32_t newest = log[count - 1].note_id;
        CHECK(newest >= 0 && newest != oldest);
        if (voices == 1) {
            CHECK(count == 3);
            CHECK(log[1].type == WS2_NOTE_CHOKE && log[1].note_id == oldest);
            CHECK(log[2].type == WS2_NOTE_ON);
        }
        e.type = WS2_NOTE_OFF;
        core.event(e, emit);
        CHECK(core.allocator().active_count() == 1);
        CHECK(core.allocator().voice(voices == 1 ? 0 : 1).note_id == newest);
        core.event(e, emit);
        CHECK(core.allocator().active_count() == 0);
        core.reset();
        count = 0;
        e.type = WS2_NOTE_ON;
        core.event(e, emit);
        CHECK(log[0].note_id == oldest);
        core.release_all(12, e, emit);
        CHECK(core.allocator().active_count() == 0);
    }
    std::puts("PASS anonymous activation identity, FIFO, steal tombstones, reset");
}
