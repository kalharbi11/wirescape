// SPDX-License-Identifier: GPL-3.0-only
// Compile the real helper with Qt's keyword macro in scope, before session.h includes it.
#include <nlohmann/json.hpp>
#define slots
#include "../document/telemetry_taps.h"
#undef slots
#include "../io/audio/callback_render.h"
#include "../io/audio/portaudio_service.h"
#include "../tests2/allocation_watch.h"
#include "devices.h"
#include "midi_service.h"
#include "session.h"
#include "null_sink_wait.h"
#include "runtime_patch.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace wirescape::sidecar;
void check(bool ok) {
    if (!ok) throw std::runtime_error("sidecar unit assertion failed");
}
void midi_hotplug_observation() {
    using namespace wirescape::midi;
    ObservedMidi backend(true, false);
    const Endpoint m8{"M8 MIDI 1", "M8:M8 MIDI 1"}, pad{"Pad", "pad"};
    check(backend.set_fake_endpoints({m8, pad}));
    std::vector<ws2_event> events;
    events.reserve(16);
    Sink sink{&events, [](void* context, uint32_t, const ws2_event& event, uint64_t) noexcept -> int {
        static_cast<std::vector<ws2_event>*>(context)->push_back(event);
        return WS2_OK;
    }};
    Service service(backend, sink);
    check(service.enable({m8.name, m8.system_name}) && service.enable({pad.name, pad.system_name}));
    check(service.subscribe({1, {}, true}));
    const std::vector<Endpoint> remembered{m8, pad};
    auto listing = [&] { return midi_devices({backend.observed(), remembered}, true); };
    service.poll();
    backend.arm_learning(true);
    check(backend.learning() == Json({{"armed", true}}));
    uint8_t first_cc[]{0xB0, 21, 74};
    check(backend.inject(m8, first_cc, 3, app_engine_clock_ns()));
    check(backend.learning()["deviceId"] == "M8 MIDI 1|M8:M8 MIDI 1");
    auto initial = listing();
    check(initial.size() == 2 && initial[0]["enabled"] == true);
    const uint8_t a[]{0x90, 60, 100}, b[]{0x90, 62, 100};
    check(backend.inject(m8, a, 3, app_engine_clock_ns()));
    check(backend.inject(pad, b, 3, app_engine_clock_ns()));
    service.poll();
    check(backend.held() == Json::array({60, 62}));
    events.clear();
    check(backend.set_fake_endpoints({pad})); service.poll();
    check(listing() != initial && listing().size() == 1);
    check(backend.learning() == Json({{"armed", true}})); // removed slot cannot be remapped
    check(backend.held() == Json::array({62}));
    check(events.size() == 1 && events[0].type == WS2_NOTE_CHOKE && events[0].source_id == 1);
    check(service.endpoint_states()[0].saved.system_name == m8.system_name);
    check(!backend.inject(m8, a, 3, app_engine_clock_ns()));
    check(backend.set_fake_endpoints({m8, pad})); service.poll();
    check(listing() == initial); // Same IDs and enabled choices return without a new selection.
    check(backend.inject(m8, a, 3, app_engine_clock_ns())); service.poll();
    check(backend.learning() == Json({{"armed", true}}));
    check(backend.held() == Json::array({60, 62}) && events.back().source_id == 1);
    service.stop();
    check(backend.held().empty());
}
void midi_learn_observation() {
    using namespace wirescape::midi;
    ObservedMidi fake(true, false);
    auto endpoints = fake.endpoints();
    check(endpoints.size() == 1);
    check(fake.open(endpoints[0], {}));
    uint8_t cc[]{0xB3, 21, 64};
    check(fake.inject(endpoints[0], cc, 3, app_engine_clock_ns()));
    check(fake.learning().is_null()); // unarmed callbacks publish nothing
    fake.arm_learning(true);
    check(fake.learning() == Json({{"armed", true}}));
    check(fake.inject(endpoints[0], cc, 3, app_engine_clock_ns()));
    const auto first = fake.learning();
    check(first["armed"] == true && first["channel"] == 4 && first["controller"] == 21);
    check(first["value"] == 64 && first["deviceId"] == "M8 MIDI 1|M8:M8 MIDI 1");
    check(first["seq"].is_string());
    uint64_t allocations = 0;
    {
        allocation_watch::ThreadScope scope;
        for (int i = 0; i < 1000; ++i) {
            cc[1] = uint8_t(i % 128);
            cc[2] = uint8_t((i * 3) % 128);
            check(fake.inject(endpoints[0], cc, 3, app_engine_clock_ns()));
        }
        allocations = scope.allocations();
    }
    check(allocations == 0);
    const auto newest = fake.learning();
    check(newest["seq"] != first["seq"] && newest["controller"] == cc[1] && newest["value"] == cc[2]);
    check(newest.size() == 6); // one observation, not a queue/history
    fake.expire_learning(std::chrono::steady_clock::now() + std::chrono::seconds(31));
    check(fake.learning()["armed"] == true); // captured: no no-capture timeout
    fake.arm_learning(true);
    check(fake.learning() == Json({{"armed", true}})); // rearm resets the observation
    fake.expire_learning(std::chrono::steady_clock::now() + std::chrono::seconds(31));
    check(fake.learning().is_null());
    fake.arm_learning(true);
    uint32_t ump_cc[]{0x20BF7F01}; // MIDI 1 UMP: channel 16, controller 127, value 1
    check(fake.inject_ump(endpoints[0], ump_cc, 1, app_engine_clock_ns()));
    check(fake.learning()["channel"] == 16 && fake.learning()["controller"] == 127);
    fake.close(endpoints[0]);
    check(fake.learning() == Json({{"armed", true}}));
    fake.arm_learning(false);
    check(fake.learning().is_null());
}

void sink_wait_deadline_regression() {
    using Clock = std::chrono::steady_clock;
    using Ns = std::chrono::nanoseconds;
    using Us = std::chrono::microseconds;
    const Clock::time_point zero{};
    const auto deadline = zero + Ns(5333333); // 256 frames / 48 kHz, rounded to nanoseconds
    check(sink_due_100ns(zero, zero) == 0);
    check(sink_due_100ns(zero, zero + Ns(1)) == -1);
    check(sink_due_100ns(zero, zero + Ns(100)) == -1);
    check(sink_due_100ns(zero, zero + Ns(101)) == -2);
    check(sink_due_100ns(zero, deadline) == -53334);

    // A timer signalled early must be rearmed for the same absolute deadline,
    // not let the next render start early or drift the periodic schedule.
    auto now = zero;
    unsigned arms = 0;
    allocation_watch::count.store(0);
    allocation_watch::enabled.store(true);
    const auto reached = sink_wait_until(
        deadline, [&] { return now; }, [&](int64_t due) {
            ++arms;
            if (arms == 1) {
                check(due == -53334);
                now = zero + Us(5000);
            } else {
                check(arms == 2 && due == -3334);
                now = deadline;
            }
            return SinkWaitResult::deadline;
        });
    check(reached == SinkWaitResult::deadline && arms == 2 && now == deadline);

    // A stop signal interrupts a long-block wait without waiting for the timer;
    // an unavailable kernel timer delegates to the existing CV fallback.
    arms = 0;
    const auto stopped = sink_wait_until(
        zero + Us(512000), [&] { return zero; }, [&](int64_t due) {
            check(due == -5120000);
            ++arms;
            return SinkWaitResult::stopped;
        });
    check(stopped == SinkWaitResult::stopped && arms == 1);
    const auto unavailable = sink_wait_until(
        deadline, [&] { return zero; }, [&](int64_t) { return SinkWaitResult::unavailable; });
    check(unavailable == SinkWaitResult::unavailable);
    arms = 0;
    const auto early_storm = sink_wait_until(
        deadline, [&] { return zero; }, [&](int64_t due) {
            check(due == -53334);
            ++arms;
            return SinkWaitResult::deadline;
        });
    check(early_storm == SinkWaitResult::unavailable && arms == 16);
    arms = 0;
    const auto overdue = sink_wait_until(
        zero, [&] { return zero + Us(1); }, [&](int64_t) {
            ++arms;
            return SinkWaitResult::unavailable;
        });
    check(overdue == SinkWaitResult::deadline && arms == 0);
    allocation_watch::enabled.store(false);
    check(allocation_watch::count.load() == 0);
}

int run_tests() {
    midi_learn_observation();
    RenderHistogram histogram;
    check(histogram.percentile(50) == 0);
    allocation_watch::count.store(0);
    allocation_watch::enabled.store(true);
    for (int percent = 0; percent < 100; ++percent) histogram.record(percent + 0.5, false);
    histogram.record(255.25, true);
    histogram.observe_faults(0);
    histogram.observe_faults(2);
    histogram.observe_faults(2); // a repeated sample is not another fault
    histogram.observe_faults(3);
    histogram.observe_faults(0); // new graph's counter restarted
    histogram.observe_faults(1);
    allocation_watch::enabled.store(false);
    check(histogram.faults == 4);
    check(allocation_watch::count.load() == 0);
    check(histogram.blocks == 101 && histogram.overruns == 1);
    check(histogram.bins[0] == 1 && histogram.bins[199] == 0 && histogram.bins[200] == 1);
    check(histogram.percentile(50) == 50 && histogram.percentile(99) == 99);
    check(histogram.percentile(100) == 200 && histogram.max_percent == 255.25);

    // Synthetic absolute deadlines prove equality, delayed wakes, long renders,
    // and catch-up blocks without relying on host scheduling or sleeping.
    RenderHistogram timing;
    using Clock = std::chrono::steady_clock;
    using Us = std::chrono::microseconds;
    const Clock::time_point zero{};
    allocation_watch::count.store(0);
    allocation_watch::enabled.store(true);
    timing.record_timing(100, zero, zero + Us(100), zero, zero + Us(100));
    timing.record_timing(1, zero + Us(201), zero + Us(202), zero + Us(100), zero + Us(200));
    timing.record_timing(101, zero + Us(200), zero + Us(301), zero + Us(200), zero + Us(300));
    timing.record_timing(1, zero + Us(500), zero + Us(501), zero + Us(300), zero + Us(400));
    allocation_watch::enabled.store(false);
    check(allocation_watch::count.load() == 0);
    check(timing.blocks == 4 && timing.overruns == 3);
    check(timing.overrun_render_count == 1 && timing.overrun_render_max_us == 101);
    check(timing.overrun_wake_count == 2 && timing.overrun_wake_max_us == 200);

    sink_wait_deadline_regression();
    midi_hotplug_observation();
    for (const auto &bad : {"{\"x\":1,\"x\":2}", "{\"x\":{\"y\":1,\"y\":2}}", "[]"}) {
        bool refused = false;
        try {
            strict_json(bad);
        } catch (...) {
            refused = true;
        }
        check(refused);
    }
    std::string boundary = "{\"x\":" + std::string(63, '[') + "0" + std::string(63, ']') + "}";
    check(strict_json(boundary).is_object());
    bool too_deep = false;
    try {
        strict_json("{\"x\":" + std::string(64, '[') + "0" + std::string(64, ']') + "}");
    } catch (...) {
        too_deep = true;
    }
    check(too_deep);
    std::ostringstream source;
    for (int i = 0; i < 257; ++i) source << "{\"v\":0,\"id\":\"q" << i << "\",\"type\":\"panic\"}\n";
    std::istringstream input(source.str());
    std::ostringstream output;
    Transport transport(input, output);
    transport.start();
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (transport.pending() < PendingLimit && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    check(transport.pending() == PendingLimit);
    // Reader has parsed the 257th command while the control executor is stalled.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for (int i = 0; i < 257; ++i) {
        Request request;
        check(transport.next(request, std::chrono::seconds(5)));
        check(request.value["id"] == "q" + std::to_string(i));
        check(request.error == (i == 256 ? "busy" : ""));
    }
    transport.finish();
    // The stream-backed test adapter must parse raw-pipe-sized chunks without
    // splitting or corrupting a newline-delimited request at 64 KiB boundaries.
    const std::string large_payload(65540, 'x');
    const std::string long_line = "{\"v\":0,\"id\":\"long\",\"type\":\"panic\",\"pad\":\"" + large_payload + "\"}\n";
    std::istringstream chunked_input(long_line + "{\"v\":0,\"id\":\"second\",\"type\":\"panic\"}\n" +
                                     std::string(LineLimit + 1, 'x') + "\n");
    std::ostringstream chunked_output;
    Transport chunked(chunked_input, chunked_output);
    chunked.start();
    Request first_chunk, second_chunk, excess_chunk;
    check(chunked.next(first_chunk, std::chrono::seconds(5)));
    check(first_chunk.error.empty() && first_chunk.value["id"] == "long");
    check(first_chunk.value["pad"].get<std::string>() == large_payload);
    chunked.complete(first_chunk);
    check(chunked.next(second_chunk, std::chrono::seconds(5)));
    check(second_chunk.error.empty() && second_chunk.value["id"] == "second");
    chunked.complete(second_chunk);
    check(chunked.next(excess_chunk, std::chrono::seconds(5)));
    check(excess_chunk.error == "limit");
    chunked.finish();

    // Coalescing must inspect the message before moving it into its slot.
    // MSVC evaluates JSON's by-value assignment argument before the left-hand
    // conditional expression, exposing a value() call on a moved-from null.
    std::istringstream coalesced_input("");
    std::ostringstream coalesced_output;
    Transport coalesced(coalesced_input, coalesced_output);
    coalesced.send({{"type", "status"}, {"probe", 0}}, true);
    coalesced.send({{"type", "status"}, {"probe", 1}}, true);
    coalesced.send({{"type", "telemetry"}, {"probe", 2}}, true);
    coalesced.start();
    coalesced.finish();
    std::istringstream coalesced_messages(coalesced_output.str());
    std::string message;
    check(bool(std::getline(coalesced_messages, message)));
    check(strict_json(message) == Json({{"type", "status"}, {"probe", 1}}));
    check(bool(std::getline(coalesced_messages, message)));
    check(strict_json(message) == Json({{"type", "telemetry"}, {"probe", 2}}));
    check(!std::getline(coalesced_messages, message));

    ObservedMidi fake(true, false);
    auto endpoints = fake.endpoints();
    check(endpoints.size() == 1);
    int messages = 0;
    wirescape::midi::Callback cb{
        &messages, [](void *context, const uint8_t *, size_t, uint64_t) { ++*static_cast<int *>(context); }, nullptr};
    check(fake.open(endpoints[0], cb));
    check(fake.activity().empty());
    uint8_t on[] = {0x90, 60, 100}, off[] = {0x90, 60, 0};
    check(fake.inject(endpoints[0], on, 3, 100));
    check(fake.held() == Json::array({60}));
    check(fake.inject(endpoints[0], off, 3, 110));
    check(fake.held().empty());
    check(messages == 2);
    fake.inject(endpoints[0], on, 3, app_engine_clock_ns());
    check(!fake.activity().empty());
    fake.clear_notes();
    check(fake.held().empty());
    uint32_t midi2_on[] = {0x40903c00, 0}, midi2_off[] = {0x40803c00, 0};
    check(fake.inject_ump(endpoints[0], midi2_on, 2, app_engine_clock_ns()));
    check(fake.held() == Json::array({60}));
    check(fake.inject_ump(endpoints[0], midi2_off, 2, app_engine_clock_ns()));
    check(fake.held().empty());
    fake.inject(endpoints[0], on, 3, 120);
    fake.close(endpoints[0]);
    check(fake.held().empty());
    ws_engine_config config{sizeof(config), 48000, 128, 64, 2048};
    wirescape::audio::EngineOwner catalogue(app_engine_create(&config), app_engine_destroy);
    std::string error;
    const char *patch =
        R"({"format":"wirescape.native.patch","version":2,"settings":{},"modules":[{"id":1,"module":"essentials.vco","version":2},{"id":7,"module":"essentials.output","version":2}],"connections":[{"kind":"signal","from":{"module":1,"port":"audio"},"to":{"module":7,"port":"audio"}}],"outputs":[{"module":7,"port":"audio","bus_channel":0},{"module":7,"port":"audio","bus_channel":1}]})";
    wirescape::audio::EngineOwner engine(wirescape::document::load_runtime_patch(patch, catalogue.get(), config, error),
                                         app_engine_destroy);
    check(bool(engine));
    float scratch[256]{}, output_block[256]{};
    std::atomic<uint64_t> faults{0};
    check(wirescape::audio::render_callback_block(engine.get(), scratch, 128, output_block, 128, app_engine_clock_ns(),
                                                  faults));
    ws_event parameter{};
    parameter.struct_size = sizeof(parameter);
    parameter.type = WS_PARAMETER;
    parameter.node_id = 1;
    // The native descriptor's stable legacy_index, independent of catalogue order.
    parameter.parameter_id = app_engine_native_descriptor(engine.get(), "essentials.vco")->params[0].legacy_index;
    parameter.generation = app_engine_generation(engine.get());
    allocation_watch::count.store(0);
    allocation_watch::enabled.store(true);
    bool okay = true;
    for (int i = 0; i < 1000; ++i) {
        parameter.value = float(220 + i % 100);
        okay = okay && app_engine_enqueue(engine.get(), &parameter) == WS_OK;
        okay = okay && wirescape::audio::render_callback_block(engine.get(), scratch, 128, output_block, 128,
                                                               app_engine_clock_ns(), faults);
    }
    allocation_watch::enabled.store(false);
    check(okay);
    check(allocation_watch::count.load() == 0);
    check(faults.load() == 0);

    // D5: compare ramped samples to a lockstep non-ramped reference. The
    // reference preserves the oscillator's actual audio, not synthesized silence.
    wirescape::audio::EngineOwner reference(
        wirescape::document::load_runtime_patch(patch, catalogue.get(), config, error), app_engine_destroy);
    wirescape::audio::EngineOwner faded(wirescape::document::load_runtime_patch(patch, catalogue.get(), config, error),
                                        app_engine_destroy);
    check(bool(reference) && bool(faded));
    float dry[128]{}, wet[128]{};
    float *dry_rows[] = {dry}, *wet_rows[] = {wet};
    auto render = [&](AppEngine *left, AppEngine *right) {
        check(app_engine_render_channels(left, wet_rows, 1, 128) == WS_OK);
        check(app_engine_render_channels(right, dry_rows, 1, 128) == WS_OK);
    };
    render(faded.get(), reference.get()); // both graphs initially at unity
    app_engine_begin_master_fade_out(faded.get());
    float expected_gain = 1.0f;
    const float step = 1.0f / 480.0f;
    int blocks = 0, nonzero_old = 0;
    allocation_watch::count.store(0);
    allocation_watch::enabled.store(true);
    while (!app_engine_master_fade_silent(faded.get()) && blocks < 8) {
        render(faded.get(), reference.get());
        ++blocks;
        for (int i = 0; i < 128; ++i) {
            expected_gain = std::max(0.0f, expected_gain - step);
            check(std::abs(wet[i] - dry[i] * expected_gain) < 0.0002f);
            if (std::abs(wet[i]) > 0.001f) ++nonzero_old;
        }
    }
    allocation_watch::enabled.store(false);
    check(app_engine_master_fade_silent(faded.get()) && nonzero_old > 0);
    check(blocks * 128 <= 480 + 256); // fade edge + a complete zero block
    check(allocation_watch::count.load() == 0);
    for (float sample : wet) check(sample == 0.0f);

    // New graph starts hard muted and rises only during its own render blocks.
    wirescape::audio::EngineOwner next(wirescape::document::load_runtime_patch(patch, catalogue.get(), config, error),
                                       app_engine_destroy);
    wirescape::audio::EngineOwner next_reference(
        wirescape::document::load_runtime_patch(patch, catalogue.get(), config, error), app_engine_destroy);
    check(bool(next) && bool(next_reference));
    app_engine_prepare_master_fade_in(next.get());
    expected_gain = 0.0f;
    blocks = 0;
    bool rose = false;
    while (blocks < 8) {
        render(next.get(), next_reference.get());
        ++blocks;
        for (int i = 0; i < 128; ++i) {
            expected_gain = std::min(1.0f, expected_gain + step);
            check(std::abs(wet[i] - dry[i] * expected_gain) < 0.0002f);
            if (std::abs(wet[i]) > 0.001f) rose = true;
        }
        if (expected_gain >= 1.0f) break;
    }
    check(rose && app_engine_master_fade_in_started(next.get()));
    check(blocks * 128 <= 480 + 128);
    // Null-sink deterministic clock bounds the complete mute/swap/unmute
    // envelope well below 150 ms at 48 kHz, independent of device timing.
    check((blocks + 6) * 128.0 / 48000.0 < 0.150);
    return 0;
}
int main() noexcept {
    try {
        return run_tests();
    } catch (const std::exception &error) {
        std::cerr << "sidecar unit: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "sidecar unit: unknown exception\n";
        return 1;
    }
}
