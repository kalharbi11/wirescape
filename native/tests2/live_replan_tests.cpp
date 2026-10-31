// SPDX-License-Identifier: GPL-3.0-only
#include "allocation_watch.h"
#include "engine2.h"
#include "wirescape_sdk.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <thread>
using namespace wirescape::engine2;
using Clock = std::chrono::steady_clock;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            std::fprintf(stderr, "live replan line %d: %s\n", __LINE__, #x);                                           \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (0)
static uint64_t now_ns() {
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}
static constexpr uint32_t frames = 256;
struct Voice {
    static constexpr ws::Info info{"test.live_voice", "Live voice", 1};
    static constexpr std::array inputs{ws::port("events", "Events", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array outputs{ws::port("signal", "Signal"),
                                        ws::port("events", "Events", WS2_EV_NOTES, WS2_POLY, 0, nullptr, WS2_EVENT)};
    static constexpr std::array params{ws::param("gain", "Gain", 0, 1, 1, "", WS2_AUTOMATABLE | WS2_MODULATABLE)};
    float held = 0;
    void prepare(const ws::Setup &) {}
    void reset() { held = 0; }
    void process(ws::Block &b) {
        b.out(0).set_channels(1);
        b.out(0).mark_constant(false);
        b.out(1).set_channels(0);
        b.split(
            [&](uint32_t first, uint32_t end) {
                for (uint32_t i = first; i < end; ++i) b.out(0).ch(0)[i] = held * b.param(0).value(0, i);
            },
            [&](const ws2_event &e) {
                if (e.type == WS2_NOTE_ON) held = e.value;
                else if (e.type == WS2_NOTE_OFF || e.type == WS2_NOTE_CHOKE) held = 0;
                else return; // Gain automation belongs to this voice, not its note destination.
                CHECK(b.emit(1, e));
            });
    }
};
struct Pass {
    static constexpr ws::Info info{"test.live_pass", "Live pass", 1};
    static constexpr std::array inputs{ws::port("signal", "Signal")};
    static constexpr std::array outputs{ws::port("signal", "Signal")};
    static constexpr std::array<ws2_param, 0> params{};
    void prepare(const ws::Setup &) {}
    void reset() {}
    void process(ws::Block &b) {
        b.out(0).set_channels(1);
        b.out(0).mark_constant(false);
        for (uint32_t i = 0; i < b.frames(); ++i) b.out(0).ch(0)[i] = b.in(0).value(0, i);
    }
};
struct Feedback : Pass {
    static constexpr ws::Info info{"test.live_feedback", "Live feedback", 1};
    void process(ws::Block &b) {
        b.out(0).set_channels(1);
        b.out(0).mark_constant(false);
        for (uint32_t i = 0; i < b.frames(); ++i) b.out(0).ch(0)[i] = b.in(0).value(0, i) + 1;
    }
};
static Config config() {
    Config c;
    c.max_block = frames;
    c.event_budget = 256;
    c.release_reserve = 64;
    c.ring_capacity = 512;
    c.future_capacity = 2048;
    return c;
}
static std::unique_ptr<Engine> graph(bool pass, bool event_edge, bool fourth) {
    auto e = std::make_unique<Engine>(config());
    CHECK(e->add_node(1, ws::Module<Voice>::descriptor()));
    CHECK(e->add_node(2, ws::Module<Voice>::descriptor()));
    if (pass) CHECK(e->add_node(3, ws::Module<Pass>::descriptor()));
    if (fourth) CHECK(e->add_node(4, ws::Module<Voice>::descriptor()));
    if (event_edge) CHECK(e->connect(1, 1, 2, 0));
    if (pass) CHECK(e->connect(2, 0, 3, 0));
    CHECK(e->output(pass ? 3 : 2, 0, 0));
    CHECK(e->output(1, 0, 1));
    if (fourth) CHECK(e->output(4, 0, 2));
    CHECK(e->register_producer() == 0);
    CHECK(e->register_producer() == 1);
    CHECK(e->prepare());
    return e;
}
static ws2_event note(uint32_t type = WS2_NOTE_ON, float value = .75f) {
    ws2_event e{};
    e.struct_size = sizeof(e);
    e.type = type;
    e.channel = 0;
    e.note_id = 7;
    e.key = 60;
    e.value = value;
    return e;
}
struct EventSink : Voice {
    static constexpr ws::Info info{"test.live_sink", "Live sink", 1};
    void process(ws::Block &b) {
        b.out(0).set_channels(1);
        b.out(0).mark_constant(false);
        b.out(1).set_channels(0);
        b.split(
            [&](uint32_t first, uint32_t end) {
                for (uint32_t i = first; i < end; ++i) b.out(0).ch(0)[i] = held;
            },
            [&](const ws2_event &e) {
                if (e.type == WS2_NOTE_ON) held = e.value;
                else if (e.type == WS2_NOTE_OFF || e.type == WS2_NOTE_CHOKE) held = 0;
            });
    }
};
struct EventEmitter : Voice {
    static constexpr ws::Info info{"test.live_emitter", "Live emitter", 1};
    void process(ws::Block &b) {
        b.out(0).set_channels(1);
        b.out(0).mark_constant(true);
        b.out(0).ch(0)[0] = 1;
        b.out(1).set_channels(0);
        auto e = note();
        e.offset = b.frames() - 1;
        CHECK(b.emit(1, e));
    }
};
// The marker bus stays at one until the acknowledgement callback fades it out.
// Stop callbacks there so a queued feedback note cannot arrive before the edit.
static void replan_at_fade_out(Engine &live, Engine &candidate) {
    std::atomic<bool> done{false}, success{false};
    std::thread edit([&] {
        success = live.replan(candidate, uint64_t(frames) * 3 * 1000000000 / 48000);
        done = true;
    });
    float samples[frames]{}, marker[frames]{};
    float *out[]{samples, marker};
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    bool acknowledged = false;
    while (!done.load() && Clock::now() < deadline) {
        {
            allocation_watch::ThreadScope watch;
            CHECK(live.render(out, 2, frames) == WS2_OK);
            CHECK(watch.allocations() == 0);
        }
        Outgoing outgoing;
        while (live.pop_egress(outgoing)) {
        }
        if (marker[frames - 1] == 0) {
            acknowledged = true;
            break;
        }
        std::this_thread::yield();
    }
    edit.join();
    CHECK(acknowledged && success.load());
}
static void removed_event_routes_release() {
    for (bool replace_source : {false, true}) {
        auto make = [&](bool candidate) {
            auto e = std::make_unique<Engine>(config());
            CHECK(e->add_node(1, ws::Module<EventSink>::descriptor()));
            CHECK(e->add_node(2, ws::Module<EventEmitter>::descriptor(),
                              candidate && replace_source ? std::vector<uint8_t>{1} : std::vector<uint8_t>{}));
            CHECK(e->connect(1, 1, 2, 0));
            if (!candidate || replace_source) CHECK(e->connect(2, 1, 1, 0));
            CHECK(e->output(1, 0, 0));
            CHECK(e->output(2, 0, 1));
            CHECK(e->register_producer() == 0);
            CHECK(e->prepare());
            return e;
        };
        auto live = make(false), candidate = make(true);
        float samples[frames]{}, marker[frames]{};
        float *out[]{samples, marker};
        CHECK(live->render(out, 2, frames) == WS2_OK);
        CHECK(samples[frames - 1] == .75f);
        const auto stale = live->meter().stale;
        replan_at_fade_out(*live, *candidate);
        candidate.reset();
        // The old source's final back-edge note must not restart the destination
        // after its resume choke, even if the numeric cable survives source reuse.
        // Check replacement before newly emitted back-edge notes can arrive.
        {
            allocation_watch::ThreadScope watch;
            CHECK(live->render(out, 2, 1) == WS2_OK);
            CHECK(watch.allocations() == 0);
        }
        CHECK(samples[0] == 0);
        if (!replace_source) {
            CHECK(live->render(out, 2, frames) == WS2_OK);
            for (float sample : samples) CHECK(sample == 0);
        }
        CHECK(live->meter().stale > stale);
    }
}
struct Audio {
    Engine &engine;
    std::atomic<bool> run{true}, enabled{true}, in_render{false}, measure{false};
    std::atomic<uint64_t> blocks{0}, gap_ns{0};
    std::atomic<float> value{0}, source{0}, fourth{0};
    std::thread thread;
    explicit Audio(Engine &e)
        : engine(e), thread([this] {
              float a[frames]{}, b[frames]{}, c[frames]{};
              float *out[]{a, b, c};
              uint64_t silence_start = 0;
              auto next = Clock::now();
              while (run.load()) {
                  if (enabled.load()) {
                      in_render = true;
                      if (enabled.load()) {
                          const auto begin = now_ns();
                          {
                              allocation_watch::ThreadScope watch;
                              CHECK(engine.render(out, 3, frames, begin) == WS2_OK);
                              CHECK(watch.allocations() == 0);
                          }
                          value = a[frames - 1];
                          source = b[frames - 1];
                          fourth = c[frames - 1];
                          ++blocks;
                          if (measure.load()) {
                              if (b[frames - 1] == 0 && !silence_start) silence_start = begin;
                              if (b[frames - 1] != 0 && silence_start) {
                                  auto gap = now_ns() - silence_start + uint64_t(frames) * 1000000000 / 48000;
                                  auto previous = gap_ns.load();
                                  while (gap > previous && !gap_ns.compare_exchange_weak(previous, gap)) {
                                  }
                                  silence_start = 0;
                              }
                          } else silence_start = 0;
                      }
                      in_render = false;
                  }
                  next += std::chrono::nanoseconds(uint64_t(frames) * 1000000000 / 48000);
                  std::this_thread::sleep_until(next);
              }
          }) {}
    ~Audio() {
        run = false;
        thread.join();
    }
    void wait(float expected, float expected_source) {
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while ((value.load() != expected || source.load() != expected_source) && Clock::now() < deadline)
            std::this_thread::yield();
        CHECK(value.load() == expected);
        CHECK(source.load() == expected_source);
    }
    void pause() {
        enabled = false;
        while (in_render.load()) std::this_thread::yield();
    }
    void resume() { enabled = true; }
};
static void allocation_scope_is_thread_local() {
    std::atomic<bool> go{false}, done{false};
    std::thread control([&] {
        while (!go.load()) std::this_thread::yield();
        auto *p = ::operator new(19);
        ::operator delete(p);
        done = true;
    });
    {
        allocation_watch::ThreadScope watch;
        go = true;
        while (!done.load()) std::this_thread::yield();
        CHECK(watch.allocations() == 0);
        auto *p = ::operator new(23);
        CHECK(watch.allocations() == 1);
        ::operator delete(p);
    }
    control.join();
}
static void counted_target_rejection() {
    auto e = graph(false, true, false);
    const auto before = e->meter();
    CHECK(e->enqueue(0, 99, 0, note(), 0, Timing::Immediate) == WS2_OK);
    CHECK(e->enqueue(0, 1, 9, note(), 0, Timing::Immediate) == WS2_OK);
    auto parameter = note();
    parameter.type = WS2_PARAM;
    parameter.index = 9;
    parameter.value = .5f;
    CHECK(e->enqueue(0, 1, WS2_DIRECT_PORT, parameter, 0, Timing::Immediate) == WS2_OK);
    parameter.index = 0;
    parameter.value = 2;
    CHECK(e->enqueue(0, 1, WS2_DIRECT_PORT, parameter, 0, Timing::Immediate) == WS2_OK);
    float a[frames]{}, b[frames]{}, c[frames]{};
    float *out[]{a, b, c};
    {
        allocation_watch::ThreadScope watch;
        CHECK(e->render(out, 3, frames) == WS2_OK);
        CHECK(watch.allocations() == 0);
    }
    CHECK(e->meter().stale == before.stale + 1);
    CHECK(e->meter().dropped == before.dropped + 3);
    CHECK(e->meter().events == before.events);
}
static void feedback_delay_survives() {
    auto cfg = config();
    cfg.feedback_block = 1;
    auto make = [&](bool additional) {
        auto e = std::make_unique<Engine>(cfg);
        CHECK(e->add_node(1, ws::Module<Feedback>::descriptor()));
        CHECK(e->connect(1, 0, 1, 0));
        CHECK(e->output(1, 0, 0));
        if (additional) CHECK(e->add_node(2, ws::Module<Pass>::descriptor()));
        CHECK(e->prepare());
        return e;
    };
    auto live = make(false), candidate = make(true);
    const auto generation = live->meter().generation;
    float data[frames]{};
    float *out[]{data};
    auto render = [&] {
        allocation_watch::ThreadScope watch;
        CHECK(live->render(out, 1, frames, now_ns()) == WS2_OK);
        CHECK(watch.allocations() == 0);
    };
    for (unsigned i = 0; i < 4; ++i) render();
    std::atomic<bool> done{false}, success{false};
    std::thread edit([&] {
        success = live->replan(*candidate, uint64_t(frames) * 3 * 1000000000 / 48000);
        done = true;
    });
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (!done.load() && Clock::now() < deadline) {
        render();
        if (data[frames - 1] == 0) break; // Stop after fade-out so the next callback is the first resumed render.
        std::this_thread::yield();
    }
    edit.join();
    CHECK(success.load());
    candidate.reset();
    const float previous = live->output_sample(1, 0, 0, frames - 1); // Quiescent: renderer is this thread; edit joined.
    render();
    CHECK(live->output_sample(1, 0, 0, 0) == previous + 1);
    CHECK(live->meter().generation == generation);
    CHECK(live->feedback_edges() == 1);
}
static void ramp_survives() {
    auto live = graph(false, true, false), reference = graph(false, true, false);
    auto candidate = graph(true, true, false);
    float a[1]{}, b[1]{}, x[1]{}, y[1]{};
    float *out[]{a, b}, *expected[]{x, y};
    auto render = [&] {
        allocation_watch::ThreadScope watch;
        CHECK(live->render(out, 2, 1) == WS2_OK);
        CHECK(reference->render(expected, 2, 1) == WS2_OK);
        CHECK(watch.allocations() == 0);
    };
    for (auto *e : {live.get(), reference.get()}) CHECK(e->enqueue(0, 1, 0, note(), 0, Timing::Immediate) == WS2_OK);
    render(); // Start the host ramp before requesting automation.
    std::atomic<bool> done{false}, success{false};
    std::thread edit([&] {
        success = live->replan(*candidate, uint64_t(frames) * 3 * 1000000000 / 48000);
        done = true;
    });
    auto gain = note();
    gain.type = WS2_PARAM;
    gain.channel = -1;
    gain.index = 0;
    unsigned callbacks = 0;
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (!done.load() && Clock::now() < deadline) {
        gain.value = (++callbacks & 1) ? .25f : .75f; // Keep the ramp moving even while edit staging runs.
        for (auto *e : {live.get(), reference.get()})
            CHECK(e->enqueue(0, 1, WS2_DIRECT_PORT, gain, 0, Timing::Immediate) == WS2_OK);
        render();
        if (b[0] == 0) break; // Stop at acknowledged fade-out; no parked or resumed callback can mask a reset.
        std::this_thread::yield();
    }
    edit.join();
    CHECK(success.load());
    CHECK(callbacks > 0);
    candidate.reset();
    CHECK(live->output_sample(1, 0, 0, 0) == reference->output_sample(1, 0, 0, 0));
    render();
    CHECK(live->output_sample(1, 0, 0, 0) == reference->output_sample(1, 0, 0, 0));
    CHECK(b[0] == y[0]);
}
static double live_edits() {
    auto live = graph(false, true, true);
    auto &e = *live;
    const auto generation = e.meter().generation;
    CHECK(e.enqueue(0, 1, 0, note(), 0, Timing::Immediate) == WS2_OK);
    ws2_event gain{};
    gain.struct_size = sizeof(gain);
    gain.type = WS2_PARAM;
    gain.channel = -1;
    gain.index = 0;
    gain.value = .5f;
    CHECK(e.enqueue(0, 1, WS2_DIRECT_PORT, gain, 0, Timing::Immediate) == WS2_OK);
    Audio audio(e);
    audio.wait(.75f, .375f);
    audio.measure = true;
    std::atomic<bool> producing{true};
    std::atomic<uint64_t> accepted{0};
    std::thread producer([&] {
        auto mod = gain;
        mod.type = WS2_PARAM_MOD;
        mod.value = 0;
        while (producing.load()) {
            auto r = e.enqueue(1, 1, WS2_DIRECT_PORT, mod, 0, Timing::Immediate);
            CHECK(r == WS2_OK || r == WS2_CAPACITY);
            if (r == WS2_OK) ++accepted;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    auto edit = [&](bool pass, bool edge, bool fourth) {
        auto next = graph(pass, edge, fourth);
        const auto before = audio.blocks.load();
        CHECK(e.replan(*next, uint64_t(frames) * 3 * 1000000000 / 48000));
        next.reset(); // No render callback may retain a staging owner/context.
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while (audio.blocks.load() < before + 3 && Clock::now() < deadline) std::this_thread::yield();
        CHECK(audio.blocks.load() >= before + 3);
        CHECK(e.meter().generation == generation);
    };
    edit(true, true, true);
    audio.wait(.75f, .375f); // Add and connect without resetting held DSP/ramp state.
    edit(false, true, true);
    audio.wait(.75f, .375f); // Disconnect and remove the signal pass.
    const auto future = e.meter().frame + frames * 40;
    CHECK(e.enqueue(0, 4, 0, note(WS2_NOTE_ON, .25f), future, Timing::Frames) == WS2_OK);
    const auto stale = e.meter().stale;
    edit(false, true, false);
    edit(false, true, true); // Numeric ID reuse cannot resurrect the removed node's future note.
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (e.meter().frame < future + frames * 2 && Clock::now() < deadline) std::this_thread::yield();
    CHECK(e.meter().frame >= future + frames * 2);
    CHECK(audio.fourth.load() == 0);
    CHECK(e.meter().stale > stale);
    edit(false, false, true);
    audio.wait(0, .375f); // Removed event edge chokes former destination, not unrelated source.
    edit(false, true, true);
    CHECK(e.enqueue(0, 1, 0, note(), 0, Timing::Immediate) == WS2_OK);
    audio.wait(.75f, .375f);
    audio.pause();
    auto next = graph(true, true, true);
    const auto before = e.meter();
    CHECK(!e.replan(*next, 1000000));
    CHECK(e.meter().generation == before.generation);
    CHECK(e.meter().frame == before.frame);
    const auto paused_blocks = audio.blocks.load();
    audio.resume();
    const auto resumed_deadline = Clock::now() + std::chrono::seconds(2);
    while (audio.blocks.load() == paused_blocks && Clock::now() < resumed_deadline) std::this_thread::yield();
    CHECK(audio.blocks.load() > paused_blocks);
    audio.wait(.75f, .375f); // Timeout retains the old graph and resumes it.
    edit(true, true, true);
    audio.wait(.75f, .375f); // A cancelled request cannot poison a subsequent acknowledged edit.
    bool has_pass = true;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        auto racing = graph(!has_pass, true, true);
        if (e.replan(*racing, 1)) has_pass = !has_pass; // Either CAS winner is valid; only an ack permits mutation.
        racing.reset();
        audio.pause();
        CHECK((e.descriptor(3) != nullptr) == has_pass);
        CHECK(e.meter().generation == generation);
        const auto block = audio.blocks.load();
        audio.resume();
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        while (audio.blocks.load() == block && Clock::now() < deadline) std::this_thread::yield();
        CHECK(audio.blocks.load() > block);
        audio.wait(.75f, .375f);
    }
    producing = false;
    producer.join();
    CHECK(accepted.load() > 0);
    CHECK(e.meter().generation == generation);
    CHECK(e.meter().faults == 0);
    CHECK(audio.gap_ns.load() > 0);
    audio.measure = false;
    CHECK(e.enqueue(0, 1, 0, note(WS2_NOTE_OFF, 0), 0, Timing::Immediate) == WS2_OK);
    audio.wait(0, 0);
    return double(audio.gap_ns.load()) / 1000000.;
}
int main(int argc, char **argv) {
    double max_gap_ms = 0;
    if (argc == 3 && std::strcmp(argv[1], "--max-gap-ms") == 0) max_gap_ms = std::strtod(argv[2], nullptr);
    else CHECK(argc == 1);
    allocation_scope_is_thread_local();
    counted_target_rejection();
    removed_event_routes_release();
    feedback_delay_survives();
    ramp_survives();
    const auto gap = live_edits();
    std::printf(
        "PASS live add/connect/disconnect/remove, state/release/ingress/cancel/noalloc; 256-frame gap %.3f ms\n", gap);
    if (max_gap_ms > 0) CHECK(gap <= max_gap_ms);
}
