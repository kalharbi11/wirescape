// SPDX-License-Identifier: GPL-3.0-only
#include "../../engine2/engine2.h"
#include "../../tests2/allocation_watch.h"
#include "../adsr/dsp.h"
#include "../output/dsp.h"
#include "../vca/dsp.h"
#include "../vco/dsp.h"
#include "dsp.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace wirescape;
static void check(bool value, const char *why) {
    if (!value) throw std::runtime_error(why);
}
struct NoteSpec {
    int type;
    int key;
    int id = -1;
    int channel = 0;
    uint32_t source = 1;
};
static ws2_event note(NoteSpec spec) {
    ws2_event e{};
    e.struct_size = sizeof(e);
    e.type = spec.type;
    e.key = spec.key;
    e.note_id = spec.id;
    e.channel = spec.channel;
    e.source_id = spec.source;
    e.value = 1;
    return e;
}
static void shared_tests() {
    using namespace voice;
    NoteToCv core;
    Config cfg;
    cfg.max_voices = 3;
    core.configure(cfg);
    std::vector<ws2_event> log;
    auto emit = [&](const ws2_event &e) { log.push_back(e); };
    for (int key : {60, 69, 72}) core.event(note({WS2_NOTE_ON, key, key}), emit);
    check(core.allocator().voice(0).pitch == 0 && core.allocator().voice(1).pitch == .75f &&
              core.allocator().voice(2).pitch == 1,
          "CV01 pitch");
    cfg.max_voices = 2;
    core.configure(cfg);
    log.clear();
    core.event(note({WS2_NOTE_ON, 60, 1}), emit);
    core.event(note({WS2_NOTE_ON, 64, 2}), emit);
    auto e = note({WS2_NOTE_ON, 67, 3});
    e.offset = 10;
    core.event(e, emit);
    check(log.size() == 4 && log[2].type == WS2_NOTE_CHOKE && log[2].key == 60 && log[2].offset == 10 &&
              log[3].type == WS2_NOTE_ON && log[3].offset == 10,
          "CV04 choke before on");
    core.event(note({WS2_NOTE_OFF, 60, 1}), emit);
    check(core.allocator().active_count() == 2, "stale off cannot close replacement");
    core.reset();
    core.event(note({WS2_NOTE_ON, 60}), emit);
    core.event(note({WS2_NOTE_ON, 60}), emit);
    core.event(note({WS2_NOTE_OFF, 60}), emit);
    check(!core.allocator().voice(0).gate && core.allocator().voice(1).gate, "anonymous repeated key FIFO");
    core.reset();
    core.event(note({WS2_NOTE_ON, 60, -1, 0}), emit);
    core.event(note({WS2_NOTE_ON, 60, -1, 1}), emit);
    e = note({WS2_NOTE_OFF, 60});
    core.sustain(1, 0, true, e, emit);
    core.event(e, emit);
    core.event(note({WS2_NOTE_OFF, 60, -1, 1}), emit);
    check(core.allocator().voice(0).gate && !core.allocator().voice(1).gate, "CV05 channel isolated sustain");
    e.offset = 12;
    core.sustain(1, 0, false, e, emit);
    check(core.allocator().active_count() == 0 && log.back().offset == 12 && log.back().type == WS2_NOTE_OFF,
          "pedal release emits off");
    cfg.mode = Mode::Mono;
    cfg.legato = true;
    core.configure(cfg);
    core.event(note({WS2_NOTE_ON, 60, 1}), emit);
    core.allocator().clear_triggers();
    core.event(note({WS2_NOTE_ON, 64, 2}), emit);
    check(core.allocator().voice(0).gate && !core.allocator().voice(0).trigger, "CV07 mono legato holds gate");
    core.event(note({WS2_NOTE_OFF, 64, 2}), emit);
    check(core.allocator().voice(0).key == 60 && core.allocator().voice(0).gate, "mono fallback");
    for (auto mode : {Mode::Rotate, Mode::Reuse, Mode::Reset, Mode::Mono}) {
        cfg.mode = mode;
        cfg.legato = false;
        core.configure(cfg);
        Allocator oracle(cfg);
        for (int key : {60, 62, 64, 66}) {
            auto event = note({WS2_NOTE_ON, key, key});
            core.event(event, emit);
            oracle.note_on({0, key, 0, static_cast<int16_t>(key), 1});
            for (int c = 0; c < oracle.voice_count(); ++c)
                check(core.allocator().voice(c).key == oracle.voice(c).key &&
                          core.allocator().voice(c).gate == oracle.voice(c).gate,
                      "CV09 allocator oracle");
        }
    }
    core.release_all(UINT32_MAX, e, emit);
    check(core.allocator().active_count() == 0, "release all");
}
static void graph_tests() {
    for (auto rate : {44100., 48000., 96000.})
        for (uint32_t voices : {1u, 4u, 16u})
            for (uint32_t frames : {1u, 17u, 128u, 256u}) {
                engine2::Config cfg;
                cfg.sample_rate = rate;
                cfg.max_block = 256;
                engine2::Engine engine(cfg);
                engine.register_producer();
                check(engine.add_node(1, ws::Module<ws2ports::HostKeyboard>::descriptor()), "keyboard add");
                check(engine.add_node(2, ws::Module<ws2ports::Vco>::descriptor()) &&
                          engine.add_node(3, ws::Module<ws2ports::Adsr>::descriptor()) &&
                          engine.add_node(4, ws::Module<ws2ports::Vca>::descriptor()) &&
                          engine.add_node(5, ws::Module<ws2ports::Output>::descriptor()),
                      "synth add");
                check(engine.connect(1, 0, 2, 0) && engine.connect(1, 1, 3, 0) && engine.connect(2, 0, 4, 0) &&
                          engine.connect(3, 0, 4, 1) && engine.connect(4, 0, 5, 0) && engine.output(5, 0, 0) &&
                          engine.prepare(),
                      "prepare graph");
                check(engine.set_parameter(1, 0, float(voices)), "voice count");
                for (int key : {60, 64, 67, 72}) {
                    auto e = note({WS2_NOTE_ON, key, key});
                    check(engine.enqueue(0, 1, WS2_DIRECT_PORT, e, 0) == WS2_OK, "note enqueue");
                }
                float samples[256]{};
                float *rows[] = {samples};
                int result;
                size_t allocations;
                {
                    allocation_watch::Scope scope;
                    result = engine.render(rows, 1, frames);
                    allocations = scope.allocations();
                }
                check(result == WS2_OK && allocations == 0, "callback renders without allocation");
                check(engine.output_channels(1, 0) == voices && engine.output_channels(2, 0) == voices,
                      "source propagates poly width");
                check(engine.output_channels(5, 0) == 1, "Output SUM collapses voices");
                if (voices == 1 && frames == 1)
                    check(engine.output_sample(1, 1, 0, 0) == 0, "stolen mono voice has retrigger gap");
                check(engine.render(rows, 1, frames) == WS2_OK, "settled chord render");
                int active = 0;
                for (uint32_t c = 0; c < voices; ++c) active += engine.output_sample(1, 1, c, frames - 1) > .5f;
                check(active == int(std::min(voices, 4u)), "four-note chord active channels");
                for (uint32_t i = 0; i < frames; ++i) check(std::isfinite(samples[i]), "finite audio");
                engine.panic();
                check(engine.render(rows, 1, frames) == WS2_OK, "panic render");
                for (uint32_t c = 0; c < voices; ++c)
                    check(engine.output_sample(1, 1, c, frames - 1) == 0, "panic clears every gate");
            }
}
int main() {
    try {
        shared_tests();
        graph_tests();
        std::cout << "keyboard/core matrices pass\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
