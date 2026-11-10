// SPDX-License-Identifier: GPL-3.0-only
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "dsp.h"
#include <algorithm>
#include <array>
#include <memory>

using chord::Chord;
static constexpr uint32_t kFrames = 256;

static ws2_event note(uint32_t type, int key, int32_t id = 0, uint32_t source = 1,
                      int channel = 0, uint32_t offset = 0, float velocity = .8f) {
    ws2_event e{};
    e.struct_size = sizeof(e);
    e.type = type;
    e.port = 0;
    e.key = key;
    e.note_id = id;
    e.source_id = source;
    e.channel = channel;
    e.offset = offset;
    e.value = velocity;
    return e;
}
struct Recorder {
    std::array<ws2_event, 512> events{};
    uint32_t count = 0, accepted = 0, limit = 512;
    void clear(uint32_t cap = 512) { count = accepted = 0; limit = cap; }
};
static int WS2_CALL push(void *user, uint32_t port, const ws2_event *e) {
    auto &rec = *static_cast<Recorder *>(user);
    REQUIRE(port == 0 && e->port == 0 && e->struct_size == sizeof(ws2_event));
    if (rec.accepted >= rec.limit) return WS2_CAPACITY;
    REQUIRE(rec.count < rec.events.size());
    rec.events[rec.count++] = *e;
    ++rec.accepted;
    return WS2_OK;
}

struct Rig {
    Pool pool{};
    ws2_host host{sizeof(host), &pool, allocate};
    const ws2_descriptor *desc = ws::Module<Chord>::descriptor();
    void *state = nullptr;
    uint8_t instance[4]{};
    struct Buffers {
        float output[16][kFrames]{};
        Recorder recorder{};
    };
    std::unique_ptr<Buffers> storage = std::make_unique<Buffers>();
    Recorder &r = storage->recorder;
    float *rows[16]{};
    ws2_signal_out out[1]{};
    ws2_event_out event_out[1]{};
    ws2_signal input[1]{};
    float values[8]{};
    const float *params_data[8]{};
    ws2_signal params[8]{};
    ws2_event_list incoming[1]{};
    ws2_process process{};

    explicit Rig(uint32_t tag = 1, double rate = 48000) {
        for (int i = 0; i < 4; ++i) instance[i] = uint8_t(tag >> (i * 8));
        const ws2_config cfg{sizeof(cfg), rate, kFrames, 16, 42, instance, sizeof(instance)};
        state = desc->create(&host);
        REQUIRE(state && desc->prepare(state, &cfg, &host) == WS2_OK);
        for (uint32_t c = 0; c < 16; ++c) rows[c] = storage->output[c];
        out[0] = {sizeof(ws2_signal_out), rows, 0, 0, 16};
        event_out[0] = {sizeof(ws2_event_out), &r, 0, push};
        input[0] = {sizeof(ws2_signal), nullptr, 0, 0};
        for (uint32_t i = 0; i < 8; ++i) {
            values[i] = Chord::params[i].initial;
            params_data[i] = &values[i];
            params[i] = {sizeof(ws2_signal), &params_data[i], 1, WS2_CONSTANT};
        }
        process.struct_size = sizeof(process);
        process.sample_rate = rate;
        process.channels = 1;
        process.input_count = 1;
        process.output_count = 1;
        process.param_count = 8;
        process.inputs = input;
        process.outputs = out;
        process.params = params;
        process.event_inputs = incoming;
        process.event_outputs = event_out;
    }
    ~Rig() { desc->destroy(state); }

    void run(std::initializer_list<ws2_event> events = {}, uint32_t frames = 128) {
        REQUIRE(frames >= 1 && frames <= kFrames && events.size() <= 32);
        std::array<ws2_event, 32> e{};
        std::copy(events.begin(), events.end(), e.begin());
        incoming[0] = {sizeof(ws2_event_list), uint32_t(events.size()), e.data()};
        process.events = incoming[0];
        process.frames = frames;
        REQUIRE(desc->process(state, &process) == WS2_OK);
        process.frame_position += frames;
    }
};
static void major_and_held_edit() {
    Rig a;
    a.r.clear();
    a.run({note(WS2_NOTE_ON, 60, 0, 9, 2, 13)});
    REQUIRE(a.r.count == 3);
    const int expected[] = {60, 64, 67};
    int32_t ids[3]{};
    for (int i = 0; i < 3; ++i) {
        const auto &e = a.r.events[i];
        REQUIRE(e.type == WS2_NOTE_ON && e.key == expected[i] && e.offset == 13 &&
                e.source_id == 9 && e.channel == 2 && e.value == .8f);
        ids[i] = e.note_id;
    }
    REQUIRE(ids[0] != ids[1] && ids[1] != ids[2] && ids[0] != ids[2]);
    a.values[1] = -3; // held notes keep their original pitches
    a.r.clear();
    a.run({note(WS2_NOTE_OFF, 60, 0, 9, 2, 9)});
    REQUIRE(a.r.count == 3);
    for (int i = 0; i < 3; ++i)
        REQUIRE(a.r.events[i].type == WS2_NOTE_OFF && a.r.events[i].key == expected[i] &&
                a.r.events[i].note_id == ids[i] && a.r.events[i].offset == 9);
    a.r.clear();
    a.run({note(WS2_NOTE_ON, 60, 1, 9, 2)});
    REQUIRE(a.r.count == 3 && a.r.events[1].key == 57 && a.r.events[2].key == 67);
}
static void identity_and_expression() {
    Rig a;
    a.r.clear();
    a.run({note(WS2_NOTE_ON, 60, 5, 1), note(WS2_NOTE_ON, 60, 5, 2)});
    REQUIRE(a.r.count == 6);
    a.r.clear();
    a.run({note(WS2_NOTE_OFF, 60, 5, 1)});
    REQUIRE(a.r.count == 3);
    for (uint32_t i = 0; i < a.r.count; ++i) REQUIRE(a.r.events[i].source_id == 1);
    a.r.clear();
    ws2_event ex = note(WS2_NOTE_EXPRESSION, 60, 5, 2);
    ex.expression = WS2_EX_PRESSURE;
    ex.value = .6f;
    a.run({ex});
    REQUIRE(a.r.count == 3);
    REQUIRE(a.r.events[0].note_id != 5 && a.r.events[0].expression == WS2_EX_PRESSURE);
    a.r.clear();
    a.run({note(WS2_NOTE_CHOKE, 0, -1, 2)});
    REQUIRE(a.r.count == 3);
    for (uint32_t i = 0; i < a.r.count; ++i) REQUIRE(a.r.events[i].type == WS2_NOTE_OFF);
}
static void anonymous_lifo_and_duplicate() {
    Rig a;
    a.values[3] = 4;
    a.r.clear();
    a.run({note(WS2_NOTE_ON, 60, -1), note(WS2_NOTE_ON, 60, -1)});
    REQUIRE(a.r.count == 6);
    const auto newest_id = a.r.events[3].note_id;
    a.r.clear();
    a.run({note(WS2_NOTE_OFF, 60, -1)});
    REQUIRE(a.r.count == 3 && a.r.events[0].note_id == newest_id);
    a.r.clear();
    a.run({note(WS2_NOTE_OFF, 60, -1)});
    REQUIRE(a.r.count == 3);
    a.r.clear();
    a.run({note(WS2_NOTE_OFF, 60, -1)});
    REQUIRE(a.r.count == 0);
}
static void queue_recovery_and_instance_ids() {
    Rig a(17), b(18);
    a.r.clear(1);
    a.run({note(WS2_NOTE_ON, 60)});
    REQUIRE(a.r.count == 1);
    const int32_t accepted = a.r.events[0].note_id;
    a.r.clear(0);
    a.run({note(WS2_NOTE_OFF, 60)});
    REQUIRE(a.r.count == 0);
    a.r.clear();
    a.run({});
    REQUIRE(a.r.count == 1 && a.r.events[0].type == WS2_NOTE_OFF &&
            a.r.events[0].note_id == accepted && a.r.events[0].offset == 0);
    b.r.clear();
    b.run({note(WS2_NOTE_ON, 60)});
    REQUIRE(b.r.count == 3);
    REQUIRE((uint32_t(accepted) >> 20) == 17 && (uint32_t(b.r.events[0].note_id) >> 20) == 18);
}
static void frames_and_realtime() {
    for (uint32_t frames : {1u, 17u, 128u, 256u}) {
        Rig a;
        a.r.clear();
        size_t allocations = 0;
        {
            allocation_watch::Scope scope;
            a.run({note(WS2_NOTE_ON, 60, 0)});
            a.run({note(WS2_NOTE_OFF, 60, 0)}, frames);
            allocations = scope.allocations();
        }
        REQUIRE(allocations == 0 && a.r.count == 6);
        a.desc->reset(a.state);
        a.r.clear();
        a.run({note(WS2_NOTE_OFF, 60, 0)});
        REQUIRE(a.r.count == 0);
    }
}
int main() {
    major_and_held_edit();
    identity_and_expression();
    anonymous_lifo_and_duplicate();
    queue_recovery_and_instance_ids();
    frames_and_realtime();
}
