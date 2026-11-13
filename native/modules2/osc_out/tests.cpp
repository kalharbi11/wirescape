// SPDX-License-Identifier: GPL-3.0-only
#include "../../io/midi/module_test_support.h"
#include "dsp.h"
#include <array>
#include <cstring>
#include <vector>

using namespace wirescape;

struct Rig {
    MidiArena arena;
    ws2_host host{sizeof(host), &arena, MidiArena::alloc};
    const ws2_descriptor *desc = ws::Module<osc::OscOut>::descriptor();
    void *state = nullptr;
    float samples[8][2][1024]{};
    const float *channel_ptrs[8][2]{};
    ws2_signal inputs[9]{};
    ws2_event_list event_inputs[9]{};
    ws2_process ctx{};

    explicit Rig(const osc::OutConfiguration *config = nullptr) {
        state = desc->create(&host);
        REQUIRE(state);
        ws2_config setup{sizeof(setup),
                         48000.,
                         1024,
                         16,
                         1,
                         reinterpret_cast<const uint8_t *>(config),
                         config ? uint32_t(sizeof(*config)) : 0u};
        REQUIRE(desc->prepare(state, &setup, &host) == WS2_OK);
        for (uint32_t i = 0; i < 8; ++i) {
            channel_ptrs[i][0] = samples[i][0];
            channel_ptrs[i][1] = samples[i][1];
            inputs[i] = {sizeof(ws2_signal), channel_ptrs[i], 2, WS2_CONNECTED};
        }
        inputs[8] = {sizeof(ws2_signal), nullptr, 0, 0};
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = 48000.;
        ctx.channels = 1;
        ctx.input_count = 9;
        ctx.inputs = inputs;
        ctx.event_inputs = event_inputs;
    }
    ~Rig() { desc->destroy(state); }
    void run(const std::vector<ws2_event> &events = {}, uint32_t frames = 8) {
        ctx.frames = frames;
        ctx.events = {sizeof(ws2_event_list), uint32_t(events.size()), events.data()};
        event_inputs[8] = ctx.events;
        int result;
        {
            allocation_watch::Scope no_alloc;
            result = desc->process(state, &ctx);
            REQUIRE(no_alloc.allocations() == 0);
        }
        REQUIRE(result == WS2_OK);
        ctx.frame_position += frames;
    }
};

static ws2_event note(uint32_t kind, int32_t key) {
    ws2_event event{};
    event.struct_size = sizeof(event);
    event.type = kind;
    event.port = 8;
    event.key = key;
    event.channel = 2;
    event.note_id = key;
    event.source_id = 1;
    event.value = .75f;
    return event;
}
static void configuration_checks() {
    osc::OutConfiguration config;
    // The public type itself must be safe for the sender, even without a parser call.
    REQUIRE(config.version == 1 && config.port == 9001 && config.rate_hz == 100);
    REQUIRE(config.bus == nullptr && config.notes.enabled == 0);
    REQUIRE(std::strcmp(config.rows[0].address, "/ws/row/1") == 0);
    REQUIRE(std::strcmp(config.rows[7].address, "/ws/row/8") == 0);
    REQUIRE(std::strcmp(config.notes.address, "/ws/note") == 0);
    REQUIRE(osc::out_configuration(nullptr, 0, config));
    REQUIRE(config.port == 9001 && config.rate_hz == 100 && config.notes.enabled == 0);
    REQUIRE(std::strcmp(config.rows[0].address, "/ws/row/1") == 0);
    REQUIRE(std::strcmp(config.rows[7].address, "/ws/row/8") == 0);
    REQUIRE(std::strcmp(config.notes.address, "/ws/note") == 0);
    auto valid = config;
    auto check = [&](const osc::OutConfiguration &c) {
        osc::OutConfiguration decoded;
        return osc::out_configuration(reinterpret_cast<const uint8_t *>(&c), sizeof(c), decoded);
    };
    REQUIRE(check(valid));
    valid.version = 2;
    REQUIRE(!check(valid));
    valid = config;
    valid.port = 0;
    REQUIRE(!check(valid));
    valid = config;
    valid.rate_hz = 501;
    REQUIRE(!check(valid));
    valid = config;
    valid.rows[0].enabled = 2;
    REQUIRE(!check(valid));
    valid = config;
    valid.rows[0].kind = 2;
    REQUIRE(!check(valid));
    valid = config;
    valid.rows[0].deadband = -0.1f;
    REQUIRE(!check(valid));
    valid = config;
    valid.rows[0].deadband = 1.01f;
    REQUIRE(!check(valid));
    for (const char *bad : {"no/slash", "/a*", "/bad key", "/b[2]", "/c#1"}) {
        valid = config;
        std::strcpy(valid.rows[0].address, bad);
        REQUIRE(!check(valid));
    }
    valid = config;
    std::memset(valid.rows[0].address, 'x', sizeof(valid.rows[0].address));
    REQUIRE(!check(valid));
    valid = config;
    valid.notes.enabled = 2;
    REQUIRE(!check(valid));
    REQUIRE(!osc::out_configuration(nullptr, 1, valid));
}

static void rows_and_gate_edges() {
    osc::OutBus bus;
    osc::OutConfiguration cfg;
    REQUIRE(osc::out_configuration(nullptr, 0, cfg));
    cfg.rows[0].kind = 1;    // gate, with two short pulses within one callback
    cfg.rows[2].enabled = 0; // disabled rows do not publish
    cfg.bus = &bus;
    Rig r(&cfg);
    r.samples[0][0][1] = 1.f;
    r.samples[0][0][3] = 1.f;
    r.samples[1][0][7] = .25f;
    r.samples[1][1][7] = .99f; // FIRST ignores channel 1
    r.samples[2][0][7] = 1.f;
    r.run();
    REQUIRE(bus.rows[0].rises == 2 && bus.rows[0].falls == 2);
    REQUIRE(bus.rows[0].blocks == 1 && bus.rows[0].value == 0.f);
    REQUIRE(bus.rows[1].blocks == 1 && bus.rows[1].value == .25f);
    REQUIRE(bus.rows[2].blocks == 0);
    r.inputs[0].flags |= WS2_CONSTANT;
    r.samples[0][0][0] = 1.f;
    r.run();
    REQUIRE(bus.rows[0].rises == 3 && bus.rows[0].falls == 2);
    REQUIRE(bus.rows[0].blocks == 2 && bus.rows[0].value == 1.f);
    {
        allocation_watch::Scope no_alloc;
        r.desc->reset(r.state);
        REQUIRE(no_alloc.allocations() == 0);
    }
    REQUIRE(bus.rows[0].blocks == 2); // never zero consumer-visible counters on reset
}

static void gate_threshold_matches_sender() {
    osc::OutBus bus;
    osc::OutConfiguration cfg;
    cfg.rows[0].kind = 1;
    cfg.bus = &bus;
    Rig r(&cfg);
    // Sender §4 uses value >= 0.5; DSP must count that same threshold.
    r.samples[0][0][0] = .5f;
    r.samples[0][0][1] = .5f;
    r.samples[0][0][2] = .499f;
    r.run({}, 3);
    REQUIRE(bus.rows[0].rises == 1 && bus.rows[0].falls == 1);
    REQUIRE(bus.rows[0].blocks == 1 && bus.rows[0].value == .499f);
}

static void thousand_gate_crossings_in_one_block() {
    osc::OutBus bus;
    osc::OutConfiguration cfg;
    cfg.rows[0].kind = 1;
    cfg.bus = &bus;
    Rig r(&cfg);
    // A render block may contain far more crossings than the sender's per-poll budget.
    for (uint32_t i = 0; i < 1000; ++i) r.samples[0][0][i] = i % 2 == 0 ? 1.f : 0.f;
    r.run({}, 1001);
    REQUIRE(bus.rows[0].rises == 500);
    REQUIRE(bus.rows[0].falls == 500);
    REQUIRE(bus.rows[0].blocks == 1 && bus.rows[0].value == 0.f);
}

static void note_ring_and_no_bus() {
    osc::OutBus bus;
    osc::OutConfiguration cfg;
    REQUIRE(osc::out_configuration(nullptr, 0, cfg));
    cfg.notes.enabled = 1;
    cfg.bus = &bus;
    Rig r(&cfg);
    r.run({note(WS2_NOTE_ON, 60), note(WS2_NOTE_OFF, 60)}, 1);
    REQUIRE(bus.note_head == 2 && bus.note_tail == 0 && bus.dropped == 0);
    REQUIRE(bus.notes[0].key == 60 && bus.notes[0].channel == 2 && bus.notes[0].velocity == .75f);
    REQUIRE(bus.notes[1].key == 60 && bus.notes[1].velocity == 0.f);
    std::vector<ws2_event> burst;
    burst.reserve(64);
    for (int i = 0; i < 64; ++i) burst.push_back(note(WS2_NOTE_ON, i));
    r.run(burst, 1);
    REQUIRE(bus.note_head == 64 && bus.dropped == 2); // head 2 + 62 slots available
    bus.note_tail.store(64, std::memory_order_release);
    r.run({note(WS2_NOTE_CHOKE, 70)}, 1);
    REQUIRE(bus.note_head == 65 && bus.dropped == 2 && bus.notes[0].velocity == 0.f);

    cfg.bus = nullptr;
    Rig detached(&cfg);
    detached.samples[0][0][0] = .2f;
    detached.run({note(WS2_NOTE_ON, 60)}, 1); // private bus, no sender or device
    Rig defaults;
    defaults.run({}, 1); // no binary config also initializes defaults
}
int main() {
    configuration_checks();
    rows_and_gate_edges();
    gate_threshold_matches_sender();
    thousand_gate_crossings_in_one_block();
    note_ring_and_no_bus();
}
