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
    std::array<float, 3> input_values{};
    std::array<const float *, 3> input_rows{};
    std::array<ws2_signal, 3> inputs{};
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
        for (uint32_t i = 0; i < inputs.size(); ++i) {
            input_rows[i] = &input_values[i];
            inputs[i] = {sizeof(ws2_signal), &input_rows[i], 1, WS2_CONSTANT};
        }
        raw.frames = 1; raw.input_count = 3; raw.output_count = 18; raw.param_count = 10;
        raw.inputs = inputs.data();
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

struct PolyRig : Rig {
    std::array<ws2_event, 96> log{};
    std::array<uint32_t, 96> event_samples{};
    uint32_t sample = 0;
    uint32_t event_count = 0, ons_attempted = 0, offs_attempted = 0;
    uint32_t refuse_on_attempt = 0, refuse_off_attempt = 0;
    bool block_note_offs = false;
    static int WS2_CALL record(void *context, uint32_t port, const ws2_event *e) {
        auto &r = *static_cast<PolyRig *>(context);
        CHECK(port == 4 || port == 9);
        if (e->type == WS2_NOTE_ON && ++r.ons_attempted == r.refuse_on_attempt) return WS2_CAPACITY;
        if (e->type == WS2_NOTE_OFF) {
            ++r.offs_attempted;
            if (r.block_note_offs || r.offs_attempted == r.refuse_off_attempt) return WS2_CAPACITY;
        }
        CHECK(r.event_count < r.log.size());
        r.event_samples[r.event_count] = r.sample;
        r.log[r.event_count++] = *e;
        return WS2_OK;
    }
    PolyRig() {
        events[4] = {sizeof(ws2_event_out), this, 4, record};
        events[9] = {sizeof(ws2_event_out), this, 9, record};
        auto &track = sequencer.pattern.tracks[0];
        track.length = 8;
        for (auto &s : track.steps) s.kind = groove::Rest;
        auto &s = track.steps[0];
        s.kind = groove::Note;
        s.velocity = 0.8f;
        s.key = 60;
        s.key_count = 3;
        s.keys = {60, 64, 67, 0, 0};
        sequencer.reset();
    }
    void render_sample() {
        Rig::render();
        ++sample;
    }
    uint32_t count(uint32_t type) const {
        uint32_t result = 0;
        for (uint32_t i = 0; i < event_count; ++i) result += log[i].type == type;
        return result;
    }
    void start() {
        render_sample();
        controls[0] = 1.f;
        render_sample();
    }
};
static void chord_egress_contract() {
    PolyRig triad;
    triad.start();
    CHECK(triad.count(WS2_NOTE_ON) == 3 && triad.values[14] == 1);
    for (uint32_t k = 0; k < 3; ++k) {
        CHECK(triad.log[k].type == WS2_NOTE_ON);
        CHECK(triad.log[k].offset == 0 &&
              triad.log[k].key == int32_t(triad.sequencer.pattern.tracks[0].steps[0].keys[k]));
        CHECK(triad.log[k].value == 0.8f && triad.log[k].channel == 0);
        for (uint32_t j = 0; j < k; ++j) CHECK(triad.log[k].note_id != triad.log[j].note_id);
    }
    triad.controls[2] = 1.f;
    triad.render_sample();
    CHECK(triad.count(WS2_NOTE_OFF) == 3);
    for (uint32_t k = 0; k < 3; ++k) CHECK(triad.log[k + 3].note_id == triad.log[k].note_id);

    PolyRig five;
    auto &s = five.sequencer.pattern.tracks[0].steps[0];
    s.key_count = 5;
    s.keys = {60, 62, 64, 67, 71};
    five.refuse_on_attempt = 3; // Accepted voices 0, 1, 3 and 4 only.
    five.start();
    CHECK(five.ons_attempted == 5 && five.count(WS2_NOTE_ON) == 4);
    CHECK(five.sequencer.players[0].sounding.on);
    five.refuse_off_attempt = 2; // One refused release remains owed.
    five.controls[2] = 1.f;
    five.render_sample();
    CHECK(five.count(WS2_NOTE_OFF) == 3);
    CHECK(five.sequencer.players[0].pending.on);
    CHECK(five.values[10] >= 0);
    five.render_sample();
    CHECK(five.count(WS2_NOTE_OFF) == 4 && !five.sequencer.players[0].pending.on);
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t matched = 0;
        for (uint32_t j = 4; j < five.event_count; ++j)
            matched += five.log[i].note_id == five.log[j].note_id && five.log[j].type == WS2_NOTE_OFF;
        CHECK(matched == 1);
    }

    PolyRig root_refused;
    root_refused.refuse_on_attempt = 1;
    root_refused.start();
    CHECK(root_refused.count(WS2_NOTE_ON) == 2 && root_refused.values[1] == 1.f);
    root_refused.controls[2] = 1.f;
    root_refused.render_sample();
    CHECK(root_refused.count(WS2_NOTE_OFF) == 2);

    PolyRig gate;
    gate.sequencer.samples_per_step = 16;
    gate.sequencer.pattern.tracks[0].steps[0].gate_q = 1;
    gate.start();
    for (uint32_t i = 0; i < 4; ++i) gate.render_sample();
    CHECK(gate.count(WS2_NOTE_OFF) == 3 && gate.values[1] == 0.f);

    PolyRig skip;
    skip.sequencer.pattern.tracks[0].steps[0].cond_type = groove::AB;
    skip.sequencer.pattern.tracks[0].steps[0].cond_a = 2;
    skip.sequencer.pattern.tracks[0].steps[0].cond_b = 2;
    skip.sequencer.pattern.tracks[0].steps[1].kind = groove::Tie;
    skip.start();
    skip.render_sample();
    CHECK(skip.count(WS2_NOTE_ON) == 0 && skip.values[14] == 0);

    PolyRig tie;
    tie.sequencer.samples_per_step = 8;
    auto &track = tie.sequencer.pattern.tracks[0];
    track.length = 16;
    track.steps[0].kind = groove::Tie;
    track.steps[15].kind = groove::Note;
    track.steps[15].velocity = 0.8f;
    track.steps[15].key_count = 3;
    track.steps[15].keys = {60, 64, 67, 0, 0};
    tie.start();
    for (uint32_t i = 0; i < 16 * 8; ++i) tie.render_sample();
    CHECK(tie.count(WS2_NOTE_ON) == 3 && tie.count(WS2_NOTE_OFF) == 0);
    CHECK(tie.sequencer.step == 16 && tie.values[10] == 0); // Bar-wrap tie retains every voice.
}

static void speed_clock_contract() {
    // At 120 bpm and a device-free 120 Hz test rate, one sixteenth is 15 samples.
    constexpr uint32_t sixteenth = 15;
    for (uint32_t div : {2u, 4u, 8u}) {
        PolyRig timing;
        auto &track = timing.sequencer.pattern.tracks[0];
        track.div = div;
        for (auto &s : track.steps) {
            s = track.steps[0];
            s.kind = groove::Note;
        }
        timing.sequencer.samples_per_step = sixteenth;
        timing.start();
        CHECK(timing.count(WS2_NOTE_ON) == 3);
        for (uint32_t n = 1; n < sixteenth * div; ++n) {
            timing.render_sample();
            CHECK(timing.count(WS2_NOTE_ON) == 3);
        }
        timing.render_sample();
        CHECK(timing.count(WS2_NOTE_ON) == 6);
        CHECK(timing.event_samples[6] - timing.event_samples[0] == sixteenth * div);
        CHECK(timing.values[10] == 1); // Own step index, not absolute n.
    }

    PolyRig gate;
    auto &track = gate.sequencer.pattern.tracks[0];
    track.div = 4;
    track.steps[0].gate_q = 1;
    track.steps[1].kind = groove::Tie;
    track.steps[1].gate_q = 1;
    gate.sequencer.samples_per_step = sixteenth;
    gate.start();
    for (uint32_t n = 1; n < 75; ++n) {
        gate.render_sample();
        CHECK(gate.count(WS2_NOTE_OFF) == 0);
    }
    gate.render_sample();
    CHECK(gate.count(WS2_NOTE_OFF) == 3);
    CHECK(gate.event_samples[3] - gate.event_samples[0] == 75);

    PolyRig ab;
    ab.sequencer.pattern.tracks[0].div = 4;
    auto &note = ab.sequencer.pattern.tracks[0].steps[0];
    note.cond_type = groove::AB;
    note.cond_a = 2;
    note.cond_b = 2;
    ab.start();
    CHECK(ab.count(WS2_NOTE_ON) == 0);
    for (uint32_t n = 1; n < 32; ++n) ab.render_sample();
    CHECK(ab.count(WS2_NOTE_ON) == 0);
    ab.render_sample();
    CHECK(ab.count(WS2_NOTE_ON) == 3);
    CHECK(ab.values[12] == 2 && ab.values[10] == 0);
    CHECK(ab.log[0].key == 60 && ab.log[1].key == 64 && ab.log[2].key == 67);

    PolyRig master;
    master.sequencer.pattern.track_count = 2;
    master.sequencer.pattern.tracks[0].length = 16;
    master.sequencer.pattern.tracks[0].div = 4;
    master.sequencer.pattern.tracks[1].length = 16;
    master.sequencer.pattern.tracks[1].div = 1;
    master.sequencer.players[1].armed = true;
    master.start();
    CHECK(master.sequencer.master_steps() == 64);
    master.controls[1] = 1.f; // Global STOP at n=64, not the fast track's n=16.
    for (uint32_t n = 1; n < 64; ++n) {
        master.render_sample();
        CHECK(master.sequencer.running);
    }
    master.render_sample();
    CHECK(!master.sequencer.running && master.count(WS2_NOTE_OFF) == 3);

    // The largest permitted 64 × 8 track cycle realigns only after 512
    // absolute sixteenths; test the actual counter rollover and STOP gate.
    PolyRig maximum;
    maximum.sequencer.pattern.tracks[0].length = 64;
    maximum.sequencer.pattern.tracks[0].div = 8;
    maximum.start();
    CHECK(maximum.sequencer.master_steps() == 512);
    maximum.sequencer.step = 511;
    maximum.sequencer.step_t[0] = 63;
    maximum.sequencer.stop_queued = true;
    ws::Block maximum_block(maximum.raw);
    maximum.sequencer.boundary(maximum_block, 0);
    CHECK(maximum.sequencer.step == 0 && maximum.sequencer.step_t[0] == 0 && !maximum.sequencer.running &&
          !maximum.sequencer.stop_queued && maximum.count(WS2_NOTE_OFF) == 3);

    PolyRig track_stop;
    track_stop.sequencer.pattern.tracks[0].length = 8;
    track_stop.sequencer.pattern.tracks[0].div = 2;
    track_stop.start();
    track_stop.controls[5] = 1.f; // Track STOP at its n=16 cycle end.
    for (uint32_t n = 1; n < 16; ++n) {
        track_stop.render_sample();
        CHECK(track_stop.sequencer.players[0].playing);
    }
    track_stop.render_sample();
    CHECK(!track_stop.sequencer.players[0].playing && track_stop.sequencer.running);
}
static void chord_identity_lifetime() {
    PolyRig rig;
    rig.sequencer.pattern.track_count = 2;
    rig.sequencer.pattern.tracks[1] = rig.sequencer.pattern.tracks[0];
    auto &next = rig.sequencer.pattern.tracks[0].steps[1];
    next = rig.sequencer.pattern.tracks[0].steps[0];
    next.key = 62;
    next.keys = {62, 65, 69, 0, 0};
    rig.sequencer.players[0].playing = true;
    rig.sequencer.players[1].playing = true;
    ws::Block block(rig.raw);
    allocation_watch::ThreadScope watch;
    rig.sequencer.trigger(block, 0, 0);
    rig.sequencer.trigger(block, 1, 0);
    CHECK(rig.count(WS2_NOTE_ON) == 6);
    const int32_t pending_id = rig.sequencer.players[0].sounding.notes[0].id;
    const int32_t sounding_id = rig.sequencer.players[1].sounding.notes[0].id;

    // Retrigger a different chord while its predecessor's OFFs are refused and
    // the other track still has a chord sounding. Pending IDs cannot be reused.
    rig.block_note_offs = true;
    rig.sequencer.release(block, 0, 0);
    rig.sequencer.step_t[0] = 1;
    rig.sequencer.next_id = pending_id;
    rig.sequencer.trigger(block, 0, 0);
    CHECK(rig.count(WS2_NOTE_ON) == 6 && rig.count(WS2_NOTE_OFF) == 0);
    CHECK(rig.sequencer.players[0].pending.on);
    const int32_t available_id = rig.sequencer.unused_note_id();
    for (const auto &player : rig.sequencer.players) {
        for (const auto &voice : player.sounding.notes) CHECK(!voice.on || voice.id != available_id);
        for (const auto &voice : player.pending.notes) CHECK(!voice.on || voice.id != available_id);
    }

    // After accepted OFFs the old IDs may be reused, but only after their OFFs.
    rig.block_note_offs = false;
    rig.sequencer.next_id = pending_id;
    rig.sequencer.trigger(block, 0, 0);
    CHECK(rig.count(WS2_NOTE_OFF) == 3 && rig.count(WS2_NOTE_ON) == 9);
    CHECK(!rig.sequencer.players[0].pending.on);
    CHECK(rig.sequencer.players[0].sounding.notes[0].id == pending_id);

    // A reused counter must also skip IDs still sounding on the other track.
    rig.sequencer.release(block, 0, 0);
    rig.sequencer.next_id = sounding_id;
    rig.sequencer.trigger(block, 0, 0);
    rig.sequencer.release_all(block, 0);
    std::array<int32_t, 10> live{};
    live.fill(-1);
    bool reused = false;
    for (uint32_t i = 0; i < rig.event_count; ++i) {
        const auto &event = rig.log[i];
        if (event.type == WS2_NOTE_ON) {
            for (int32_t id : live) CHECK(id != event.note_id);
            for (uint32_t j = 0; j < i; ++j)
                reused |= rig.log[j].type == WS2_NOTE_OFF && rig.log[j].note_id == event.note_id;
            bool inserted = false;
            for (auto &id : live) {
                if (id != -1) continue;
                id = event.note_id;
                inserted = true;
                break;
            }
            CHECK(inserted);
        } else if (event.type == WS2_NOTE_OFF) {
            bool matched = false;
            for (auto &id : live) {
                if (id != event.note_id) continue;
                id = -1;
                matched = true;
                break;
            }
            CHECK(matched);
        }
    }
    CHECK(reused);
    for (int32_t id : live) CHECK(id == -1);
    CHECK(watch.allocations() == 0);
}

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
static void external_clock_contract() {
    const char *names[]{"clock", "run", "reset"};
    CHECK(groove::Sequencer::inputs.size() == 3);
    for (uint32_t i = 0; i < 3; ++i)
        CHECK(std::strcmp(names[i], groove::Sequencer::inputs[i].id) == 0);

    // The first pulse uses the authored BPM. The second supplies a real period.
    Rig clocked;
    clocked.sequencer.internal_samples_per_step = clocked.sequencer.samples_per_step = 16;
    clocked.sequencer.pattern.tracks[0].div = 2;
    clocked.inputs[0].flags |= WS2_CONNECTED;
    clocked.render();
    clocked.controls[0] = 1;
    clocked.render();
    CHECK(clocked.sequencer.running && clocked.sequencer.step == 0);
    clocked.input_values[0] = 1;
    clocked.render();
    CHECK(clocked.sequencer.step == 1 && clocked.sequencer.step_t[0] == 0);
    clocked.input_values[0] = 0;
    for (uint32_t i = 0; i < 4; ++i) clocked.render();
    clocked.input_values[0] = 1;
    clocked.render();
    CHECK(clocked.sequencer.step == 2 && clocked.sequencer.step_t[0] == 1);
    CHECK(clocked.sequencer.samples_per_step == 5);
    clocked.input_values[0] = 0;
    for (uint32_t i = 0; i < 40; ++i) clocked.render();
    CHECK(clocked.sequencer.step == 2); // Missing master pulses never free-run.
    clocked.inputs[0].flags &= ~uint32_t(WS2_CONNECTED);
    clocked.render();
    CHECK(clocked.sequencer.samples_per_step == 16); // Disconnect restores BPM.

    // A short note expires even when the external pulse train goes missing.
    Rig gate;
    gate.sequencer.internal_samples_per_step = gate.sequencer.samples_per_step = 16;
    for (auto &row : gate.sequencer.pattern.tracks[0].steps) row.kind = groove::Rest;
    gate.sequencer.pattern.tracks[0].steps[0].kind = groove::Note;
    gate.sequencer.pattern.tracks[0].steps[0].velocity = 1;
    gate.sequencer.pattern.tracks[0].steps[0].gate_q = 1;
    gate.inputs[0].flags |= WS2_CONNECTED;
    gate.render(); gate.controls[0] = 1; gate.render();
    CHECK(gate.ons == 1 && gate.offs == 0);
    for (uint32_t i = 0; i < 6; ++i) gate.render();
    CHECK(gate.offs == 1 && gate.values[1] == 0);

    // The run input starts on the NEXT pulse, unlike the existing manual START.
    Rig transport;
    transport.inputs[0].flags |= WS2_CONNECTED;
    transport.inputs[1].flags |= WS2_CONNECTED;
    transport.inputs[2].flags |= WS2_CONNECTED;
    transport.render();
    transport.input_values[1] = 1;
    transport.render();
    CHECK(!transport.sequencer.running && transport.ons == 0);
    transport.input_values[0] = 1;
    transport.render();
    CHECK(transport.sequencer.running && transport.sequencer.step == 0 && transport.ons == 1);
    transport.input_values[0] = 0;
    transport.render();
    transport.input_values[0] = 1;
    transport.render();
    CHECK(transport.sequencer.step == 1);
    transport.input_values[0] = 0;
    transport.render();
    transport.input_values[2] = 1;
    transport.input_values[0] = 1; // RESET wins over a simultaneous clock tick.
    transport.render();
    CHECK(transport.sequencer.step == 0 && transport.offs == 1);
    transport.input_values[2] = 0;
    transport.input_values[0] = 0;
    transport.render();
    transport.input_values[1] = 0;
    transport.render();
    CHECK(!transport.sequencer.running && transport.values[1] == 0);
    transport.input_values[0] = 1;
    transport.render();
    CHECK(!transport.sequencer.running);
}
int main() {
    external_clock_contract();
    chord_egress_contract();
    chord_identity_lifetime();
    speed_clock_contract();
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
