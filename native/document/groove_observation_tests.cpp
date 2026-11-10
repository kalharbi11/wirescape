// SPDX-License-Identifier: GPL-3.0-only
// Device-free observation contract, including deliberately refused release egress.
#include "../modules2/groove/dsp.h"
#include "../tests2/allocation_watch.h"
#include <array>
#include "../engine2/engine2.h"
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "Groove observation line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)
struct Rig {
    groove::Sequencer sequencer;
    std::array<float, 18> values{};
    std::array<float *, 18> rows{};
    std::array<ws2_signal_out, 18> outputs{};
    std::array<ws2_event_out, 18> events{};
    std::array<float, 10> controls{};
    std::array<const float *, 10> control_rows{};
    std::array<ws2_signal, 10> params{};
    ws2_process raw{};
    bool refuse_off = false;
    uint32_t ons = 0, offs = 0;
    int32_t note_id = -1;
    static int WS2_CALL push(void *context, uint32_t port, const ws2_event *event) {
        auto &rig = *static_cast<Rig *>(context);
        CHECK(port == 4);
        if (event->type == WS2_NOTE_OFF) {
            if (rig.refuse_off) return WS2_CAPACITY;
            CHECK(event->note_id == rig.note_id);
            ++rig.offs;
        } else if (event->type == WS2_NOTE_ON) {
            rig.note_id = event->note_id;
            ++rig.ons;
        }
        return WS2_OK;
    }
    Rig() {
        for (uint32_t i = 0; i < outputs.size(); ++i) {
            rows[i] = &values[i];
            outputs[i] = {sizeof(ws2_signal_out), &rows[i], 1, 0, 1};
            events[i] = {sizeof(ws2_event_out), this, i, push};
        }
        for (uint32_t i = 0; i < params.size(); ++i) {
            control_rows[i] = &controls[i];
            params[i] = {sizeof(ws2_signal), &control_rows[i], 1, WS2_CONSTANT};
        }
        raw.frames = 1; raw.output_count = 18; raw.param_count = 10;
        raw.outputs = outputs.data(); raw.params = params.data(); raw.event_outputs = events.data();
        auto &track = sequencer.pattern.tracks[0];
        for (auto &step : track.steps) step.kind = groove::Tie;
        track.steps[0].kind = groove::Note; track.steps[0].velocity = 1;
        sequencer.reset();
        sequencer.samples_per_step = 1;
    }
    void render() {
        ws::Block block(raw);
        allocation_watch::ThreadScope watch;
        sequencer.process(block);
        CHECK(watch.allocations() == 0);
    }
};
struct SnapshotProbe {
    static constexpr ws::Info info{"test.groove_snapshot", "Snapshot", 1};
    static constexpr std::array<ws2_port, 0> inputs{};
    static constexpr std::array<ws2_param, 0> params{};
    static constexpr std::array outputs{ws::port("a", "A"), ws::port("b", "B"),
        ws::port("c", "C"), ws::port("d", "D"), ws::port("e", "E"),
        ws::port("f", "F"), ws::port("g", "G"), ws::port("h", "H")};
    uint32_t counter = 0;
    void prepare(const ws::Setup &) {}
    void reset() { counter = 0; }
    void process(ws::Block &block) {
        const bool odd = (++counter & 1u) != 0;
        const float a[]{7, -1, 65535, 1, 1, 0, 1, 0};
        const float b[]{31, 9, 0, 65535, 0, 1, 2, 65535};
        for (uint32_t port = 0; port < 8; ++port) {
            block.out(port).set_channels(1);
            block.out(port).mark_constant(true);
            block.out(port).ch()[0] = odd ? a[port] : b[port];
        }
    }
};
static void coherent_snapshots() {
    using namespace wirescape::engine2;
    Engine engine;
    CHECK(engine.add_node(1, ws::Module<SnapshotProbe>::descriptor()));
    std::vector<Tap> taps;
    for (uint32_t port = 0; port < 8; ++port) taps.push_back({Tap::Output, 1, port, Tap::First});
    CHECK(engine.set_telemetry_taps(taps) && engine.prepare());
    std::atomic<bool> done{false};
    std::thread render([&] {
        for (uint32_t i = 0; i < 50000; ++i) CHECK(engine.render(nullptr, 0, 1) == WS2_OK);
        done.store(true, std::memory_order_release);
    });
    uint32_t observed = 0;
    do {
        float values[8]{}; uint32_t counts[8]{}; uint64_t frame = 0;
        CHECK(engine.read_telemetry(values, counts, 8, frame) == 8);
        if (!frame) continue;
        const float a[]{7, -1, 65535, 1, 1, 0, 1, 0};
        const float b[]{31, 9, 0, 65535, 0, 1, 2, 65535};
        const float *expected = (frame & 1u) ? a : b;
        for (uint32_t i = 0; i < 8; ++i) CHECK(values[i] == expected[i]);
        ++observed;
    } while (!done.load(std::memory_order_acquire));
    render.join();
    CHECK(observed > 0);
}
int main() {
    coherent_snapshots();
    const char *ports[]{"t1_pitch", "t1_gate", "t1_mod1", "t1_mod2", "t1_notes",
        "t2_pitch", "t2_gate", "t2_mod1", "t2_mod2", "t2_notes", "t1_pos", "t2_pos",
        "t1_loop_lo", "t2_loop_lo", "t1_fired", "t2_fired", "t1_loop_hi", "t2_loop_hi"};
    CHECK(groove::Sequencer::outputs.size() == 18);
    for (uint32_t i = 0; i < 18; ++i) CHECK(std::strcmp(ports[i], groove::Sequencer::outputs[i].id) == 0);
    Rig rig;
    rig.render();
    CHECK(rig.values[10] == -1 && rig.values[11] == -1 && rig.values[14] == 0);
    rig.controls[0] = 1; rig.render();
    CHECK(rig.ons == 1 && rig.values[10] == 0 && rig.values[14] == 1);
    rig.render();
    CHECK(rig.values[10] == 1 && rig.values[14] == 1); // a passed trig's continued tie
    // Every bit of large counters survives the float32 observation ports.
    for (uint32_t loop : {0x01000001u, 0xffffffffu}) {
        rig.sequencer.players[0].loop = loop;
        rig.render();
        const auto measured = uint32_t(rig.values[12]) | (uint32_t(rig.values[16]) << 16u);
        CHECK(measured == loop);
    }
    rig.refuse_off = true;
    rig.controls[1] = 1; rig.render();
    CHECK(rig.values[10] >= 0); // queued STOP is not complete
    for (uint32_t i = 0; i < 32; ++i) rig.render();
    CHECK(!rig.sequencer.running && rig.sequencer.players[0].pending.on);
    CHECK(rig.values[1] == 0 && rig.values[10] >= 0 && rig.offs == 0 && rig.ons == 1);
    rig.refuse_off = false; rig.render();
    CHECK(rig.offs == 1 && rig.ons == 1 && rig.values[10] == -1 && rig.values[14] == 0);
    rig.sequencer.reset(); rig.render();
    CHECK(rig.values[10] == -1 && rig.values[12] == 1 && rig.values[16] == 0);
    // Global transport running with both tracks disarmed is not a completed STOP.
    Rig idle;
    idle.render();
    for (auto &p : idle.sequencer.players) p.armed = false;
    idle.controls[0] = 1; idle.render();
    CHECK(idle.sequencer.running && idle.ons == 0);
    CHECK(idle.values[10] >= 0 && idle.values[11] >= 0);
    // A velocity-0 note is a rest: neither it nor its following tie has fired.
    Rig silent;
    silent.sequencer.pattern.tracks[0].steps[0].velocity = 0;
    silent.render();
    silent.controls[0] = 1; silent.render();
    CHECK(silent.ons == 0 && silent.values[14] == 0);
    silent.render();
    CHECK(silent.values[14] == 0);
}
