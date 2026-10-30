// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include <cstring>
#include <tuple>
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
    bend.values[7] = 0; bend.run({midi1(0xe0, 0, 0)}); REQUIRE(bend.sample(0) == 0 && bend.sample(5) == -1);
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

static void partitioned_graph_fixtures() {
    using namespace engine2;
    using Observed = std::tuple<uint32_t, uint32_t, uint64_t, uint32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t>;
    std::array<std::vector<Observed>, 3> reference;
    for (uint32_t fixture = 0; fixture < 3; ++fixture)
    for (double rate : {44100., 48000., 96000.}) for (uint32_t block : {1u, 17u, 128u}) {
        Config config; config.sample_rate = rate; config.max_block = 128; config.playing = false;
        Engine e(config); const auto producer = uint32_t(e.register_producer());
        REQUIRE(e.add_node(1, ws::Module<MidiIn>::descriptor()) && e.add_node(2, ws::Module<MidiToCv>::descriptor()));
        REQUIRE(e.add_node(3, ws::Module<ws2ports::Vco>::descriptor()) && e.add_node(4, ws::Module<ws2ports::Adsr>::descriptor()));
        REQUIRE(e.add_node(5, ws::Module<ws2ports::Vca>::descriptor()) && e.add_node(6, ws::Module<ws2ports::Output>::descriptor()));
        REQUIRE(e.connect(1, 0, 2, 0) && e.connect(2, 0, 3, 0) && e.connect(2, 1, 4, 0));
        REQUIRE(e.connect(3, 0, 5, 0) && e.connect(4, 0, 5, 1) && e.connect(5, 0, 6, 0) && e.output(6, 0, 0));
        const uint32_t voices = fixture == 1 ? 2 : 1;
        REQUIRE(e.set_parameter(2, 0, float(voices)) && e.prepare());
        auto send = [&](ws2_event event, uint64_t frame) { REQUIRE(e.enqueue(producer, 1, WS2_DIRECT_PORT, event, frame) == WS2_OK); };
        // Synthetic endpoints A/B provide immutable source IDs; backend acquisition
        // and physical hot-plug are independently exercised by fake Service tests.
        if (fixture == 1) {
            send(midi1(0xb0, 64, 127), 0);
            for (uint8_t key : {60, 64, 67}) send(midi1(0x90, key, 127), 1);
            send(midi1(0x90, 72, 127, 0, 2), 17); send(midi1(0x80, 67, 0), 20);
            send(source_choke(1), 35); send(midi1(0x80, 72, 0, 0, 2), 77);
        } else {
            send(midi1(0x90, 60, 127), 7);
            if (fixture == 2) {
                uint32_t rejected = 0;
                for (uint32_t n = 0; n < 400; ++n) {
                    const int status = e.enqueue(producer, 1, WS2_DIRECT_PORT, midi1(0xb0, 1, uint8_t(n % 128)), 0);
                    REQUIRE(status == WS2_OK || status == WS2_CAPACITY); rejected += status == WS2_CAPACITY;
                }
                REQUIRE(rejected > 0);
                // A saturated ordinary ingress must still reserve these releases.
                for (int n = 0; n < 4; ++n) send(midi1(0x80, 60, 0), 127);
            } else send(midi1(0x80, 60, 0), 77);
        }
        std::vector<Observed> observed; uint32_t attacks = 0, releases = 0, chokes = 0;
        float peak = 0; float data[128]{}; float* output[]{data};
        for (uint32_t first = 0; first < 160; first += block) {
            const auto frames = std::min(block, 160u - first);
            { allocation_watch::Scope scope; REQUIRE(e.render(output, 1, frames) == WS2_OK); REQUIRE(scope.allocations() == 0); }
            for (uint32_t i = 0; i < frames; ++i) {
                REQUIRE(std::isfinite(data[i])); peak = std::max(peak, std::abs(data[i]));
                const auto frame = first + i;
                if (fixture != 1) {
                    REQUIRE(e.output_sample(2, 0, 0, i) == 0);
                    REQUIRE(e.output_sample(2, 1, 0, i) == float(frame >= 7 && frame < (fixture == 2 ? 127u : 77u)));
                } else if (frame >= 35) {
                    uint32_t active = 0; bool b_alive = false;
                    for (uint32_t c = 0; c < voices; ++c) if (e.output_sample(2, 1, c, i) == 1) {
                        ++active; b_alive |= e.output_sample(2, 0, c, i) == 1;
                    }
                    REQUIRE(active == (frame < 77 ? 1u : 0u)); if (frame < 77) REQUIRE(b_alive);
                }
            }
            Outgoing item;
            while (e.pop_egress(item)) {
                const auto& event = item.event;
                // Compare accepted source timestamps even under bounded output loss;
                // normal fixture notes additionally require exact CHOKE/OFF ordering.
                if (fixture != 2 || item.node == 1)
                    observed.emplace_back(item.node, item.port, item.frame, event.type, event.channel,
                                          event.key, event.note_id, event.source_id, event.ump[0]);
                if (fixture != 2 && item.node == 2 && item.port == 7) {
                    REQUIRE(event.channel >= 0 && uint32_t(event.channel) < voices);
                    if (event.type == WS2_NOTE_ON) ++attacks;
                    else { REQUIRE(event.type == WS2_NOTE_CHOKE || event.type == WS2_NOTE_OFF);
                        ++releases; chokes += event.type == WS2_NOTE_CHOKE; }
                }
                if (fixture == 0 && item.node == 1) REQUIRE(item.frame == (((event.ump[0] >> 20) & 15) == 9 ? 7u : 77u));
            }
        }
        REQUIRE(peak > 0 && e.meter().faults == 0);
        for (uint32_t c = 0; c < voices; ++c) REQUIRE(e.output_sample(2, 1, c, (160u - 1) % block) == 0);
        if (fixture != 2) {
            REQUIRE(attacks == releases && attacks == (fixture == 1 ? 4u : 1u));
            for (const auto& attack : observed) if (std::get<0>(attack) == 2 && std::get<3>(attack) == WS2_NOTE_ON) {
                REQUIRE(std::count_if(observed.begin(), observed.end(), [&](const auto& release) {
                    return std::get<0>(release) == 2 && (std::get<3>(release) == WS2_NOTE_OFF || std::get<3>(release) == WS2_NOTE_CHOKE)
                        && std::get<4>(release) == std::get<4>(attack) && std::get<5>(release) == std::get<5>(attack)
                        && std::get<6>(release) == std::get<6>(attack) && std::get<7>(release) == std::get<7>(attack);
                }) == 1);
            }
            if (fixture == 1) REQUIRE(chokes >= 3);
        }
        // Egress interleaves per-node queues; compare logical absolute timestamps,
        // not dispatcher drain order. basic_and_steal asserts local release-before-attack.
        std::sort(observed.begin(), observed.end(), [](const auto& a, const auto& b) {
            return std::tie(std::get<2>(a), a) < std::tie(std::get<2>(b), b);
        });
        if (reference[fixture].empty()) reference[fixture] = observed;
        else REQUIRE(observed == reference[fixture]);
    }
}

static void metadata_contract() {
    REQUIRE(MidiToCv::params.size() == 9);
    for (const auto& parameter : MidiToCv::params) REQUIRE(std::strcmp(parameter.id, "retrigger") != 0);
    REQUIRE(MidiToCv::params[5].legacy_index == 6);
    REQUIRE(MidiToCv::params[8].legacy_index == 9);
}
int main() { metadata_contract(); basic_and_steal(); sustain_bend_expression(); modes_state_and_bounds(); graph_fixtures(); partitioned_graph_fixtures(); }
