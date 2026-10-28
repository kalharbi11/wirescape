// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../io/midi/module_test_support.h"
#include "../../engine2/engine2.h"
#include "../midi_in/dsp.h"
#include "../midi_cc_to_cv/dsp.h"
#include "../vco/dsp.h"
#include "../vcf/dsp.h"
#include "../adsr/dsp.h"
#include "../vca/dsp.h"
#include "../output/dsp.h"
using namespace wirescape; using namespace midi;
static void basic_and_steal() {
    MidiRig<MidiToCv> r; r.values[0] = 3;
    r.run({midi1(0x90, 60, 127), midi1(0x90, 69, 64), midi1(0x90, 72, 1)});
    REQUIRE(r.sample(0, 0) == 0 && r.sample(0, 1) == .75f && r.sample(0, 2) == 1);
    REQUIRE(r.sample(2, 0) == 1 && r.sample(2, 1) == 64.f / 127 && r.sample(2, 2) == 1.f / 127);
    REQUIRE(r.emitted.size() == 3 && r.emitted[0].note_id != r.emitted[1].note_id);
    MidiRig<MidiToCv> steal; steal.values[0] = 2;
    steal.run({midi1(0x90, 60, 127, 0), midi1(0x90, 64, 127, 5), midi1(0x90, 67, 127, 10), midi1(0x80, 60, 0, 20)});
    REQUIRE(steal.emitted.size() == 4 && steal.emitted[2].type == WS2_NOTE_CHOKE && steal.emitted[3].type == WS2_NOTE_ON);
    REQUIRE(steal.emitted[2].offset == 10 && steal.emitted[3].offset == 10 && steal.sample(1, 0, 20) == 1);
    for (uint32_t voices : {1u, 4u}) {
        MidiRig<MidiToCv> same; same.values[0] = float(voices);
        same.run({midi1(0x90, 60, 127, 0), midi1(0x90, 60, 127, 1), midi1(0x80, 60, 0, 2)});
        REQUIRE(same.sample(1, voices == 1 ? 0 : 1) == 1); same.run({midi1(0x80, 60, 0)});
        for (uint32_t c = 0; c < voices; ++c) REQUIRE(same.sample(1, c) == 0);
    }
}
static void sustain_bend_expression() {
    MidiRig<MidiToCv> s;
    s.run({midi1(0x90, 60, 127, 0, 1), midi1(0x90, 64, 127, 0, 2), midi1(0x91, 67, 127, 0, 1), midi1(0xb0, 64, 127, 3, 1), midi1(0x80, 60, 0, 5, 1), midi1(0x80, 64, 0, 5, 2), midi1(0x81, 67, 0, 5, 1), midi1(0xb0, 64, 0, 12, 1)});
    REQUIRE(s.sample(1, 0, 11) == 1 && s.sample(1, 0, 12) == 0 && s.sample(1, 1, 11) == 0 && s.sample(1, 2, 11) == 0);
    MidiRig<MidiToCv> bend;
    bend.run({midi1(0x90, 60, 127), midi1(0xe0, 0, 64, 1), midi1(0xe0, 0, 0, 2), midi1(0xe0, 127, 127, 3)});
    REQUIRE(bend.sample(0, 0, 1) == 0 && bend.sample(0, 0, 2) == -1.f / 6 && bend.sample(0, 0, 3) == 1.f / 6);
    REQUIRE(bend.sample(5, 0, 2) == -1 && bend.sample(5, 0, 3) == 1);
    bend.values[8] = 0; bend.run({midi1(0xe0, 0, 0)}); REQUIRE(bend.sample(0) == 0 && bend.sample(5) == -1);
    MidiRig<MidiToCv> expression;
    expression.run({midi1(0x91, 60, 127), midi1(0xd1, 64, 0, 1), midi1(0xa1, 60, 100, 2), midi1(0xb1, 1, 127, 3)});
    REQUIRE(expression.sample(3) == 64.f / 127 && expression.sample(4) == 100.f / 127 && expression.sample(6) == 1);
    MidiRig<MidiToCv> zero;
    zero.run({midi1(0x90, 60, 127), midi1(0x90, 60, 0, 1), midi2(0x90, 60, 0, 2)});
    REQUIRE(zero.sample(1, 0, 1) == 0 && zero.sample(1, 1, 2) == 1 && zero.sample(2, 1, 2) == 0);
    MidiRig<MidiToCv> channel;
    channel.run({midi1(0x90, 60, 127), midi1(0x91, 64, 127), midi1(0xb0, 64, 127, 1), midi1(0xb0, 123, 0, 2)});
    REQUIRE(channel.sample(1, 0) == 0 && channel.sample(1, 1) == 1);
}
static void modes_state_and_bounds() {
    for (uint32_t mode = 0; mode < 4; ++mode) for (uint32_t priority = 0; priority < 3; ++priority) {
        MidiRig<MidiToCv> r; r.values[1] = float(mode); r.values[3] = float(priority);
        voice::Config cfg; cfg.max_voices = 4; cfg.mode = static_cast<voice::Mode>(mode); cfg.mono_priority = static_cast<voice::Priority>(priority); voice::Allocator oracle(cfg);
        for (int key : {60, 62, 64}) oracle.note_on({0, key, 0, int16_t(key), 1}); oracle.note_off({0, 62, 0, 62, 0}); oracle.note_on({0, 66, 0, 66, 1});
        r.run({midi1(0x90, 60, 127), midi1(0x90, 62, 127), midi1(0x90, 64, 127), midi1(0x80, 62, 0), midi1(0x90, 66, 127)});
        for (int c = 0; c < oracle.voice_count(); ++c) { REQUIRE(r.sample(0, c) == oracle.voice(c).pitch); REQUIRE(r.sample(1, c) == (oracle.voice(c).gate ? 1.f : 0.f)); }
    }
    MidiRig<MidiToCv> legato; legato.values[1] = 3; legato.values[4] = 1;
    legato.run({midi1(0x90, 60, 127), midi1(0x90, 64, 127, 8), midi1(0x80, 64, 0, 16)});
    REQUIRE(legato.sample(0, 0, 0) == 0 && legato.sample(0, 0, 8) == 1.f / 3 && legato.sample(0, 0, 16) == 0);
    for (uint32_t i = 0; i < 32; ++i) REQUIRE(legato.sample(1, 0, i) == 1);
    for (double rate : {44100., 48000., 96000.}) for (uint32_t voices : {1u, 4u, 16u}) for (uint32_t frames : {1u, 17u, 128u, 256u}) {
        MidiRig<MidiToCv> r(nullptr, 0, rate); r.values[0] = float(voices); r.run({midi1(0x90, 60, 127, 0), source_choke(UINT32_MAX, frames - 1)}, frames);
        REQUIRE(r.outputs[0].channels == voices); for (uint32_t c = 0; c < voices; ++c) REQUIRE(r.sample(1, c, frames - 1) == 0);
    }
    MidiRig<MidiToCv> r; r.run({midi1(0x90, 60, 127), midi1(0xb0, 64, 127, 1)});
    Bytes bytes; ws2_writer writer{sizeof(writer), &bytes, sink}; REQUIRE(r.d->save_state(r.instance, &writer) == WS2_OK);
    Source source_context{&bytes, 0}; ws2_reader reader{sizeof(reader), &source_context, source, uint32_t(bytes.size())}; REQUIRE(r.d->load_state(r.instance, &reader) == WS2_OK); r.run(); for (uint32_t c = 0; c < 4; ++c) REQUIRE(r.sample(1, c) == 0);
    bytes[0] = 99; source_context.pos = 0; REQUIRE(r.d->load_state(r.instance, &reader) == WS2_UNSUPPORTED);
    bytes.clear(); source_context.pos = 0; reader.bytes = 0; REQUIRE(r.d->load_state(r.instance, &reader) == WS2_INVALID);
}
static void graph_fixtures() {
    using namespace engine2;
    for (uint32_t voices : {1u, 4u, 16u}) {
        Config config; config.max_block = 128; Engine e(config); const int producer = e.register_producer();
        REQUIRE(e.add_node(1, ws::Module<MidiIn>::descriptor()) && e.add_node(2, ws::Module<MidiToCv>::descriptor()));
        REQUIRE(e.add_node(3, ws::Module<ws2ports::Vco>::descriptor()) && e.add_node(4, ws::Module<ws2ports::Adsr>::descriptor()) && e.add_node(5, ws::Module<ws2ports::Vca>::descriptor()) && e.add_node(6, ws::Module<ws2ports::Output>::descriptor()));
        REQUIRE(e.connect(1, 0, 2, 0) && e.connect(2, 0, 3, 0) && e.connect(2, 1, 4, 0) && e.connect(3, 0, 5, 0) && e.connect(4, 0, 5, 1) && e.connect(5, 0, 6, 0) && e.output(6, 0, 0));
        REQUIRE(e.set_parameter(2, 0, float(voices)) && e.prepare());
        for (uint8_t key : {60, 64, 67, 72}) REQUIRE(e.enqueue(uint32_t(producer), 1, WS2_DIRECT_PORT, midi1(0x90, key, 127), 0) == WS2_OK);
        float data[128]{}; float* out[]{data}; { allocation_watch::Scope scope; REQUIRE(e.render(out, 1, 128) == WS2_OK); REQUIRE(scope.allocations() == 0); }
        REQUIRE(e.output_channels(2, 0) == voices && e.output_channels(6, 0) == 1);
        REQUIRE(e.enqueue(uint32_t(producer), 1, WS2_DIRECT_PORT, source_choke(), e.meter().frame) == WS2_OK); REQUIRE(e.render(out, 1, 128) == WS2_OK);
        for (uint32_t c = 0; c < voices; ++c) REQUIRE(e.output_sample(2, 1, c, 127) == 0);
    }
}
int main() { basic_and_steal(); sustain_bend_expression(); modes_state_and_bounds(); graph_fixtures(); }
