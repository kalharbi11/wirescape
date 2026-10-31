// SPDX-License-Identifier: GPL-3.0-only
#include "../../io/midi/module_test_support.h"
#include "dsp.h"
using namespace wirescape;
int main() {
    auto tap = std::make_unique<monitor::Tap>();
    monitor::Configuration cfg;
    cfg.tap = tap.get();
    MidiArena arena;
    ws2_host host{sizeof(host), &arena, MidiArena::alloc};
    const auto *d = ws::Module<monitor::Monitor>::descriptor();
    void *instance = d->create(&host);
    REQUIRE(instance);
    ws2_config setup{sizeof(setup), 48000, 256, 16, 1, reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg)};
    REQUIRE(d->prepare(instance, &setup, &host) == WS2_OK);
    ws2_signal inputs[3]{
        {sizeof(ws2_signal), nullptr, 0, 0}, {sizeof(ws2_signal), nullptr, 0, 0}, {sizeof(ws2_signal), nullptr, 0, 0}};
    ws2_process ctx{};
    ctx.struct_size = sizeof(ctx);
    ctx.frames = 32;
    ctx.sample_rate = 48000;
    ctx.channels = 1;
    ctx.input_count = 3;
    ctx.inputs = inputs;
    std::vector<ws2_event> events(100, midi1(0xb0, 74, 32));
    ctx.events = {sizeof(ws2_event_list), uint32_t(events.size()), events.data()};
    {
        allocation_watch::Scope scope;
        REQUIRE(d->process(instance, &ctx) == WS2_OK);
        REQUIRE(scope.allocations() == 0);
    }
    monitor::Entry entry;
    uint32_t count = 0;
    while (tap->pop(entry)) {
        ++count;
        REQUIRE(entry.type == WS2_MIDI && entry.bytes == 4);
    }
    REQUIRE(count == 30 && tap->received == 100 && tap->rate_limited == 70);
    // Changing pitch-wheel values is one logical key, not 128 rate-limit buckets.
    tap->clear();
    d->reset(instance);
    events.clear();
    for (uint32_t i = 0; i < 100; ++i) events.push_back(midi1(0xe0, uint8_t(i), 64));
    ctx.events = {sizeof(ws2_event_list), uint32_t(events.size()), events.data()};
    REQUIRE(d->process(instance, &ctx) == WS2_OK);
    count = 0;
    while (tap->pop(entry)) ++count;
    REQUIRE(count == 30);
    // Ring overflow is visible, and clearing belongs only to the consumer.
    monitor::Entry e;
    for (uint32_t i = 0; i < 1024; ++i) REQUIRE(tap->push(e));
    REQUIRE(!tap->push(e) && tap->dropped >= 1);
    tap->clear();
    REQUIRE(!tap->pop(e));
    // Reset leaves monotonic SPSC indices intact and does not allocate.
    const auto write = tap->write.load();
    {
        allocation_watch::Scope scope;
        d->reset(instance);
        REQUIRE(scope.allocations() == 0);
    }
    REQUIRE(tap->write == write);
    float signal = .25f;
    const float *row = &signal;
    inputs[1] = {sizeof(ws2_signal), &row, 1, WS2_CONSTANT | WS2_CONNECTED};
    ctx.events.count = 0;
    ctx.frame_position = 48000;
    REQUIRE(d->process(instance, &ctx) == WS2_OK);
    REQUIRE(tap->pop(e) && e.type == (monitor::signal_type | 1));
    float value;
    std::memcpy(&value, e.payload, sizeof(value));
    REQUIRE(value == .25f);
    d->destroy(instance);
}
