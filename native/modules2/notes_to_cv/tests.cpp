// SPDX-License-Identifier: GPL-3.0-only
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "../../engine2/engine2.h"
#include "../host_keyboard/dsp.h"
#include "../vco/dsp.h"
#include "../adsr/dsp.h"
#include "../vca/dsp.h"
#include "../output/dsp.h"
#include "dsp.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <memory>

using notes_to_cv::NotesToCv;
static constexpr uint32_t kFrames = 256;

static ws2_event note(uint32_t type, int key, int32_t id = 0, uint32_t source = 1,
                      int channel = 0, uint32_t offset = 0) {
    ws2_event e{};
    e.struct_size = sizeof(e);
    e.type = type;
    e.port = 0;
    e.key = key;
    e.note_id = id;
    e.source_id = source;
    e.channel = channel;
    e.offset = offset;
    e.value = .75f;
    return e;
}
struct Rig {
    Pool pool{};
    ws2_host host{sizeof(host), &pool, allocate};
    const ws2_descriptor *desc = ws::Module<NotesToCv>::descriptor();
    void *state = nullptr;
    struct Buffers { float output[3][16][kFrames]{}; };
    std::unique_ptr<Buffers> storage = std::make_unique<Buffers>();
    float *out_rows[3][16]{};
    ws2_signal_out outs[3]{};
    ws2_signal input[1]{};
    float voice_param = 4;
    const float *voice_rows[1]{&voice_param};
    ws2_signal params[1]{{sizeof(ws2_signal), voice_rows, 1, WS2_CONSTANT}};
    ws2_event_list incoming[1]{};
    ws2_process ctx{};

    explicit Rig(double rate = 48000) {
        const ws2_config cfg{sizeof(cfg), rate, kFrames, 16, 42, nullptr, 0};
        state = desc->create(&host);
        REQUIRE(state && desc->prepare(state, &cfg, &host) == WS2_OK);
        for (int p = 0; p < 3; ++p) {
            for (uint32_t c = 0; c < 16; ++c) out_rows[p][c] = storage->output[p][c];
            outs[p] = {sizeof(ws2_signal_out), out_rows[p], 0, 0, 16};
        }
        input[0] = {sizeof(ws2_signal), nullptr, 0, 0};
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = 1;
        ctx.input_count = 1;
        ctx.output_count = 3;
        ctx.param_count = 1;
        ctx.inputs = input;
        ctx.outputs = outs;
        ctx.params = params;
        ctx.event_inputs = incoming;
    }
    ~Rig() { desc->destroy(state); }

    void run(std::initializer_list<ws2_event> events = {}, uint32_t frames = 128) {
        REQUIRE(frames >= 1 && frames <= kFrames && events.size() <= 32);
        std::array<ws2_event, 32> buffer{};
        std::copy(events.begin(), events.end(), buffer.begin());
        incoming[0] = {sizeof(ws2_event_list), uint32_t(events.size()), buffer.data()};
        ctx.events = incoming[0];
        ctx.frames = frames;
        REQUIRE(desc->process(state, &ctx) == WS2_OK);
        ctx.frame_position += frames;
    }
    float value(uint32_t p, uint32_t voice, uint32_t frame) const {
        return storage->output[p][voice][frame];
    }
};
static void poly_triads_and_offs() {
    Rig a;
    a.run({note(WS2_NOTE_ON, 60, 0, 3, 2, 11), note(WS2_NOTE_ON, 64, 1, 3, 2, 11),
           note(WS2_NOTE_ON, 67, 2, 3, 2, 11)});
    REQUIRE(a.outs[0].channels == 4);
    REQUIRE(a.value(0, 0, 11) == 0.f);
    REQUIRE(std::abs(a.value(0, 1, 11) - 4.f / 12.f) < .00001f);
    REQUIRE(std::abs(a.value(0, 2, 11) - 7.f / 12.f) < .00001f);
    for (uint32_t c = 0; c < 3; ++c)
        REQUIRE(a.value(1, c, 10) == 0.f && a.value(1, c, 12) == 1.f &&
                a.value(2, c, 12) == .75f);
    a.run({note(WS2_NOTE_OFF, 64, 1, 3, 2, 19)});
    REQUIRE(a.value(1, 0, 20) == 1 && a.value(1, 1, 20) == 0 && a.value(1, 2, 20) == 1);
    a.run({note(WS2_NOTE_ON, 71, 3, 3, 2, 17)});
    REQUIRE(std::abs(a.value(0, 3, 20) - 11.f / 12.f) < .00001f);
    REQUIRE(a.value(1, 3, 20) == 1.f);
}
static void source_identity_and_choke() {
    Rig a;
    a.run({note(WS2_NOTE_ON, 60, 0, 10), note(WS2_NOTE_ON, 64, 0, 11)});
    a.run({note(WS2_NOTE_OFF, 60, 0, 10)});
    REQUIRE(a.value(1, 0, 1) == 0 && a.value(1, 1, 1) == 1);
    a.run({note(WS2_NOTE_ON, 67, 9, 10)});
    a.run({note(WS2_NOTE_CHOKE, 0, -1, 11)});
    REQUIRE(a.value(1, 1, 1) == 0 && a.value(1, 2, 1) == 1);
    a.run({note(WS2_NOTE_CHOKE, 0, -1, UINT32_MAX)});
    for (int c = 0; c < 4; ++c) REQUIRE(a.value(1, c, 1) == 0);
}
static void steal_and_reset() {
    Rig a;
    a.run({note(WS2_NOTE_ON, 60, 0), note(WS2_NOTE_ON, 64, 1),
           note(WS2_NOTE_ON, 67, 2), note(WS2_NOTE_ON, 71, 3)});
    a.run({note(WS2_NOTE_ON, 72, 4, 1, 0, 9)});
    REQUIRE(a.value(1, 0, 9) == 0 && a.value(1, 0, 10) == 1); // one-sample retrigger dip
    REQUIRE(a.value(0, 0, 10) == 1.f);
    a.run({note(WS2_NOTE_OFF, 60, 0)});
    REQUIRE(a.value(1, 0, 1) == 1); // old off cannot close the stolen voice
    a.desc->reset(a.state);
    a.run();
    for (uint32_t c = 0; c < 4; ++c) REQUIRE(a.value(1, c, 0) == 0);
}
static void block_matrix_and_allocations() {
    for (double rate : {44100., 48000., 96000.})
        for (uint32_t n : {1u, 17u, 128u, 256u}) {
            Rig a(rate);
            size_t allocations = 0;
            {
                allocation_watch::Scope scope;
                a.run({note(WS2_NOTE_ON, 60)}, n);
                a.run({note(WS2_NOTE_OFF, 60)}, n);
                allocations = scope.allocations();
            }
            REQUIRE(allocations == 0 && a.outs[0].channels == 4);
        }
}
static void native_keyboard_graph() {
    using namespace wirescape::engine2;
    Config cfg;
    cfg.sample_rate = 48000;
    cfg.max_block = 256;
    Engine e(cfg);
    const auto producer = e.register_producer();
    REQUIRE(e.add_node(1, ws::Module<ws2ports::HostKeyboard>::descriptor()));
    REQUIRE(e.add_node(2, ws::Module<NotesToCv>::descriptor()));
    REQUIRE(e.add_node(3, ws::Module<ws2ports::Vco>::descriptor()));
    REQUIRE(e.add_node(4, ws::Module<ws2ports::Adsr>::descriptor()));
    REQUIRE(e.add_node(5, ws::Module<ws2ports::Vca>::descriptor()));
    REQUIRE(e.add_node(6, ws::Module<ws2ports::Output>::descriptor()));
    REQUIRE(e.connect(1, 3, 2, 0) && e.connect(2, 0, 3, 0) &&
            e.connect(2, 1, 4, 0) && e.connect(3, 0, 5, 0) &&
            e.connect(4, 0, 5, 1) && e.connect(5, 0, 6, 0) &&
            e.output(6, 0, 0) && e.prepare());
    for (int key : {60, 64, 67, 71})
        REQUIRE(e.enqueue(uint32_t(producer), 1, WS2_DIRECT_PORT,
                          note(WS2_NOTE_ON, key, key), 0) == WS2_OK);
    float samples[256]{};
    float *out[]{samples};
    size_t allocations = 0;
    {
        allocation_watch::Scope scope;
        REQUIRE(e.render(out, 1, 256) == WS2_OK);
        allocations = scope.allocations();
    }
    REQUIRE(allocations == 0 && e.output_channels(2, 0) == 4);
    float peak = 0;
    for (const float v : samples) {
        REQUIRE(std::isfinite(v));
        peak = std::max(peak, std::abs(v));
    }
    REQUIRE(peak > .00001f); // real native audio through production modules
}
int main() {
    poly_triads_and_offs();
    source_identity_and_choke();
    steal_and_reset();
    block_matrix_and_allocations();
    native_keyboard_graph();
}
