#include "provider_bridge.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

using wirescape::ProviderBridge;
namespace {
using Engine = std::unique_ptr<ws_engine, decltype(&ws_engine_destroy)>;
const char *modules;
void check(bool yes, const char *why) { if (!yes) throw std::runtime_error(why); }
Engine make(uint32_t events = 256) {
    ws_engine_config config{sizeof(config), 48000, 64, 16, events};
    Engine engine(ws_engine_create(&config), ws_engine_destroy);
    check(bool(engine), "create engine");
    check(ws_engine_build_essentials(engine.get(), modules) == WS_OK, "prepare Essentials");
    return engine;
}
ProviderBridge::Config config(uint32_t per_flush = 8, uint64_t horizon = 2048) {
    return {48000, 64, horizon, per_flush, 1000000000ull, 0};
}
ProviderBridge::Input input(uint64_t millis, uint32_t node, uint32_t type,
                            float value = 0, uint32_t parameter = 0,
                            uint32_t source = 1, uint32_t note = 1) {
    return {1000000000ull + millis * 1000000ull, node, type, parameter,
            value, source, note};
}
void direct(ws_engine *e, uint64_t frame, uint32_t node, uint32_t type,
            float value, uint32_t parameter, uint32_t source, uint32_t note) {
    ws_event ev{sizeof(ev), frame, node, type, parameter, value, source, note,
                ws_engine_generation(e)};
    check(ws_engine_enqueue(e, &ev) == WS_OK, "direct enqueue");
}
ws_meter meter(ws_engine *e) {
    ws_meter m{sizeof(m)};
    check(ws_engine_get_meter(e, &m) == WS_OK, "meter");
    return m;
}
void render(ws_engine *e) {
    std::array<float, 64> samples{};
    check(ws_engine_render(e, samples.data(), 64) == WS_OK, "offline render barrier");
}

void ordering_coalescing_identity() {
    auto actual = make(), reference = make();
    ProviderBridge bridge(actual.get(), config(2));
    check(bridge.status() == ProviderBridge::Status::active, "bridge active");
    // Arrival order differs from frame order. Equal-frame events retain arrival
    // order. Only the pending value from the same node/parameter/source is merged.
    check(bridge.submit(input(4, 2, WS_NOTE_OFF, 0, 0, 3, 9)), "future note off");
    check(bridge.submit(input(2, 3, WS_PARAMETER, .2f, 0, 7)), "first value");
    check(bridge.submit(input(2, 2, WS_NOTE_ON, 69, 0, 3, 9)), "note on");
    check(bridge.submit(input(2, 3, WS_PARAMETER, .5f, 0, 7)), "coalesced value");
    check(bridge.submit(input(3, 2, WS_NOTE_OFF, 0, 0, 8, 9)), "other source off");
    check(bridge.submit(input(2, 3, WS_PARAMETER, .6f, 0, 8)), "other source value");
    check(bridge.pending() == 5 && bridge.counts().coalesced == 1, "coalescing key");
    check(bridge.flush() == 2 && bridge.pending() == 3, "flush budget");
    check(bridge.flush() == 2 && bridge.pending() == 1, "second flush budget");
    check(bridge.flush() == 1 && bridge.pending() == 0, "complete flush");
    check(meter(actual.get()).queued_events == 5, "engine queue receives five");

    direct(reference.get(), 96, 2, WS_NOTE_ON, 69, 0, 3, 9);
    direct(reference.get(), 96, 3, WS_PARAMETER, .5f, 0, 7, 1);
    direct(reference.get(), 96, 3, WS_PARAMETER, .6f, 0, 8, 1);
    direct(reference.get(), 144, 2, WS_NOTE_OFF, 0, 0, 8, 9);
    direct(reference.get(), 192, 2, WS_NOTE_OFF, 0, 0, 3, 9);
    std::array<float, 256> a{}, b{};
    for (uint32_t offset = 0; offset < a.size(); offset += 64) {
        check(ws_engine_render(actual.get(), a.data() + offset, 64) == WS_OK, "bridge render");
        check(ws_engine_render(reference.get(), b.data() + offset, 64) == WS_OK, "reference render");
    }
    check(std::memcmp(a.data(), b.data(), sizeof(a)) == 0, "offline audio matches ordered reference");
    check(std::any_of(a.begin(), a.end(), [](float x) { return std::abs(x) > 0.00001f; }),
          "fixture is audible offline");
}

void late_horizon_and_bounds() {
    auto e = make();
    ProviderBridge bridge(e.get(), config(8, 96));
    check(!bridge.submit(input(3, 2, WS_NOTE_ON, 60)), "future horizon backpressure");
    check(bridge.counts().backpressure == 1 && bridge.pending() == 0,
          "future event never stored");
    check(bridge.submit(input(0, 3, WS_PARAMETER, .4f)), "late control input");
    check(bridge.submit(input(0, 2, WS_NOTE_ON, 60)), "late note input");
    check(bridge.flush() == 1, "control only forwarded");
    check(bridge.counts().late_control == 1 && bridge.counts().stale_note_on == 1,
          "late policies counted");
    render(e.get());
    check(meter(e.get()).late_events == 0, "lead avoids engine late event");

    auto burst = make();
    ProviderBridge budgeted(burst.get(), config(2));
    for (uint32_t i = 0; i < 5; ++i)
        check(budgeted.submit(input(0, 2, WS_NOTE_ON, 60, 0, 1, i + 1)),
              "stale burst admission");
    check(budgeted.flush() == 0 && budgeted.pending() == 3 &&
          budgeted.counts().stale_note_on == 2, "first flush examines at most two");
    check(budgeted.flush() == 0 && budgeted.pending() == 1 &&
          budgeted.counts().stale_note_on == 4, "second flush examines at most two");

    auto release = make();
    ProviderBridge unsafe(release.get(), config(8, 96));
    const auto release_gen = ws_engine_generation(release.get());
    check(!unsafe.submit(input(3, 2, WS_NOTE_OFF, 0)), "unretryable future release");
    check(unsafe.status() == ProviderBridge::Status::stopped &&
          unsafe.counts().loss_panics == 1 &&
          ws_engine_generation(release.get()) == release_gen + 1,
          "release admission loss panics");

    auto malformed = make();
    ProviderBridge rejected(malformed.get(), config());
    auto bad = input(2, 2, WS_GATE, 0);
    bad.value = std::numeric_limits<float>::quiet_NaN();
    check(!rejected.submit(bad) && rejected.counts().loss_panics == 1,
          "malformed release panics");

    auto full = make();
    ProviderBridge bounded(full.get(), config());
    for (std::size_t i = 0; i < ProviderBridge::capacity; ++i)
        check(bounded.submit(input(2, 2, WS_NOTE_ON, 60, 0, 1,
                                   static_cast<uint32_t>(i + 1))), "fill pending");
    check(bounded.pending() == ProviderBridge::capacity, "fixed storage bound");
    const auto before = ws_engine_generation(full.get());
    check(!bounded.submit(input(2, 2, WS_NOTE_ON, 60, 0, 1, 999)), "overflow refused");
    check(bounded.status() == ProviderBridge::Status::stopped && bounded.pending() == 0,
          "overflow fails closed");
    check(ws_engine_generation(full.get()) == before + 1 && bounded.counts().loss_panics == 1,
          "one overflow panic");
    check(!bounded.submit(input(2, 2, WS_NOTE_ON, 60)), "stopped refuses input");
    check(ws_engine_generation(full.get()) == before + 1, "no repeated panic");
}

void losses_generation_and_barrier() {
    auto e = make();
    ProviderBridge bridge(e.get(), config());
    check(bridge.submit(input(2, 2, WS_NOTE_ON, 69, 0, 11, 7)), "note input");
    check(bridge.flush() == 1, "forward note");
    check(bridge.submit(input(0, 2, WS_NOTE_OFF, 0, 0, 11, 7)), "late off input");
    const auto gen = ws_engine_generation(e.get());
    check(bridge.flush() == 0, "late note off stops forwarding");
    check(bridge.status() == ProviderBridge::Status::stopped && bridge.counts().loss_panics == 1,
          "late off reconciles with panic");
    check(ws_engine_generation(e.get()) == gen + 1, "panic increments generation immediately");
    check(meter(e.get()).queued_events == 1, "queued old event remains until render");
    // Generation alone cannot prove that voices were reset. The test explicitly
    // drives the ACTIVE consumer through its reset boundary while quiescent.
    render(e.get());
    check(meter(e.get()).queued_events == 0, "render barrier consumes old queue");
    auto rebased = config();
    rebased.epoch_frame = 64;
    rebased.epoch_ns = 2000000000ull;
    check(bridge.resume_after_quiescent_barrier(rebased), "explicit rebase after barrier");
    check(bridge.submit({2000000000ull + 2000000ull, 3, WS_PARAMETER, 0, .8f, 1, 0}),
          "new epoch accepts input");
    check(bridge.flush() == 1, "new epoch forwards");

    auto external = make();
    ProviderBridge observer(external.get(), config());
    check(observer.submit(input(2, 2, WS_NOTE_ON, 60)), "pending before external change");
    check(ws_engine_panic(external.get()) == WS_OK, "external generation change");
    const auto changed = ws_engine_generation(external.get());
    check(observer.flush() == 0 && observer.pending() == 0, "external change drops epoch");
    check(observer.counts().generation_stops == 1 && observer.counts().loss_panics == 0,
          "external change does not issue another panic");
    check(ws_engine_generation(external.get()) == changed, "generation stable after observation");

    auto tiny = make(2);
    ProviderBridge pressured(tiny.get(), config());
    for (uint32_t i = 0; i < 3; ++i)
        check(pressured.submit(input(2, 2, WS_NOTE_ON, 60, 0, 1, i + 1)), "tiny queue input");
    check(pressured.flush() == 2, "two accepted before engine capacity");
    check(pressured.status() == ProviderBridge::Status::stopped &&
          pressured.counts().loss_panics == 1, "engine capacity panics");
}
}

int main(int argc, char **argv) {
    try {
        check(argc == 2, "expected Essentials module directory");
        modules = argv[1];
        ordering_coalescing_identity();
        late_horizon_and_bounds();
        losses_generation_and_barrier();
        std::cout << "provider bridge offline proof passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "provider bridge offline proof failed: " << e.what() << '\n';
        return 1;
    }
}
