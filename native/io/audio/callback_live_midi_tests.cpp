// SPDX-License-Identifier: GPL-3.0-only
#include "../../tests2/allocation_watch.h"
#include "callback_render.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
using wirescape::audio::render_callback_block;
void check(bool ok, const char *expression, int line) {
    if (ok) return;
    std::fprintf(stderr, "%d: %s\n", line, expression);
    std::abort();
}
#define CHECK(x) check(bool(x), #x, __LINE__)
constexpr uint32_t block = 256, rate = 48000;
constexpr uint64_t epoch = 1000000000000000ULL;
using App = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
App graph() {
    ws_engine_config cfg{sizeof(cfg), rate, block, 8, 2048};
    App app(app_engine_create(&cfg), app_engine_destroy);
    CHECK(app);
    CHECK(app_engine_add_node(app.get(), 1, "midi.in") == WS_OK);
    CHECK(app_engine_add_node(app.get(), 2, "midi.to_cv") == WS_OK);
    CHECK(app_engine_add_node(app.get(), 3, "essentials.vco") == WS_OK);
    CHECK(app_engine_add_node(app.get(), 4, "essentials.adsr") == WS_OK);
    CHECK(app_engine_add_node(app.get(), 5, "essentials.vca") == WS_OK);
    CHECK(app_engine_add_node(app.get(), 6, "essentials.output") == WS_OK);
    CHECK(app_engine_connect(app.get(), 1, 0, 2, 0) == WS_OK);
    CHECK(app_engine_connect(app.get(), 2, 0, 3, 0) == WS_OK); // pitch
    CHECK(app_engine_connect(app.get(), 2, 1, 4, 0) == WS_OK); // gate
    CHECK(app_engine_connect(app.get(), 3, 0, 5, 0) == WS_OK);
    CHECK(app_engine_connect(app.get(), 4, 0, 5, 1) == WS_OK);
    CHECK(app_engine_connect(app.get(), 5, 0, 6, 0) == WS_OK);
    CHECK(app_engine_native(app.get()).output(6, 0, 1));
    CHECK(app_engine_prepare(app.get(), 6, 0) == WS_OK);
    CHECK(app_engine_initialize_parameter(app.get(), 2, 0, 1) == WS_OK);
    CHECK(app_engine_initialize_parameter(app.get(), 4, 0, .0001f) == WS_OK);
    CHECK(app_engine_initialize_parameter(app.get(), 4, 3, .005f) == WS_OK);
    return app;
}
void midi(AppEngine *app, bool on, uint64_t when) {
    ws2_event event{};
    event.struct_size = sizeof(event);
    event.type = WS2_MIDI;
    event.source_id = 1;
    event.note_id = -1;
    event.ump_words = 1;
    event.ump[0] = on ? 0x20903c64u : 0x20803c00u;
    // Same producer, direct port, timestamp mode and generation as midi::Host::emit.
    auto &engine = app_engine_native(app);
    CHECK(engine.enqueue(1, 1, WS2_DIRECT_PORT, event, when, wirescape::engine2::Timing::Timestamped,
                         engine.meter().generation) == WS2_OK);
}
void live_notes(uint32_t offset) {
    auto app = graph();
    std::array<float, 512> scratch{}, out{};
    static_assert(out.size() == size_t{block} * 2);
    std::atomic<uint64_t> faults{0};
    uint64_t frame = 0;
    auto render = [&] {
        bool ok;
        uint64_t allocations;
        {
            allocation_watch::Scope watch;
            const auto now = epoch + frame * 1000000000ULL / rate;
            ok = render_callback_block(app.get(), scratch.data(), block, out.data(), block, now, faults);
            allocations = watch.allocations();
        }
        CHECK(ok && allocations == 0 && faults.load() == 0);
        for (size_t i = 0; i < block; ++i) {
            CHECK(std::isfinite(out[2 * i]) && std::abs(out[2 * i]) <= 1);
            CHECK(out[2 * i] == out[2 * i + 1]);
        }
        frame += block;
    };
    render(); // prime the clock and signal graph, initially silent
    for (float value : out) CHECK(value == 0);
    midi(app.get(), true, epoch + (frame + offset) * 1000000000ULL / rate);
    uint64_t onset = UINT64_MAX;
    // D13 intentionally adds one max-block ingress lead. Under the old offline
    // call the epoch stamp maps far into the future, and this onset assertion fails.
    const uint64_t expected = frame + offset + block;
    for (unsigned n = 0; n < 3; ++n) {
        const auto start = frame;
        render();
        for (size_t i = 0; i < block; ++i)
            if (std::abs(out[2 * i]) > 1e-6f && onset == UINT64_MAX) onset = start + i;
    }
    CHECK(onset != UINT64_MAX);
    CHECK(onset + block >= expected && onset < expected + block);
    if (!offset) CHECK(onset < uint64_t{3} * block); // audible within the first two post-injection blocks
    midi(app.get(), false, epoch + frame * 1000000000ULL / rate);
    for (unsigned n = 0; n < 4; ++n) render(); // ingress lead + 5 ms release + one block
    for (float value : out) CHECK(value == 0);
    // Invalid output sizes follow the existing abort/silence path, before render.
    std::array<float, 514> oversized;
    static_assert(oversized.size() == (size_t{block} + 1) * 2);
    oversized.fill(1);
    CHECK(!render_callback_block(app.get(), scratch.data(), block, oversized.data(), block + 1, epoch, faults));
    for (float value : oversized) CHECK(value == 0);
    CHECK(!render_callback_block(app.get(), scratch.data(), block, nullptr, block, epoch, faults));
    CHECK(faults.load() == 2);
}
} // namespace
int main() {
    live_notes(0);
    live_notes(block / 2);
    std::puts("PASS live callback MIDI onset/release, mid-block timestamps, stereo and zero allocations");
}
