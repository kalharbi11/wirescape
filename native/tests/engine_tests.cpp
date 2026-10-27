#include "wirescape/engine.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
static std::atomic<bool> watch_allocations{false};
static std::atomic<uint64_t> allocations{0};
extern "C" const ws_module_descriptor *WS_CALL ws_test_vco_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_adsr_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_vca_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_output_entry();
void *operator new(std::size_t size) {
    if (watch_allocations.load(std::memory_order_relaxed))
        ++allocations;
    if (void *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }
namespace {
int checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
using Engine = std::unique_ptr<ws_engine, decltype(&ws_engine_destroy)>;
std::string directory;
Engine make(double rate = 48000, uint32_t block = 512, uint32_t events = 1024, bool demo = true) {
    ws_engine_config c{sizeof(c), rate, block, 32, events};
    Engine e(ws_engine_create(&c), ws_engine_destroy);
    check(bool(e), "create");
    if (demo)
        check(ws_engine_build_essentials(e.get(), directory.c_str()) == WS_OK, "load Essentials");
    return e;
}
ws_event ev(ws_engine *e, uint64_t frame, uint32_t node, uint32_t type, float value = 0,
            uint32_t param = 0, uint32_t note = 1, uint32_t source = 1) {
    return {sizeof(ws_event),       frame, node, type, param, value, source, note,
            ws_engine_generation(e)};
}
void send(ws_engine *e, ws_event event) { check(ws_engine_enqueue(e, &event) == WS_OK, "enqueue"); }
std::vector<float> partition(uint32_t block) {
    auto e = make(48000, block);
    send(e.get(), ev(e.get(), 0, 3, WS_PARAMETER, .4f));
    send(e.get(), ev(e.get(), 13, 1, WS_NOTE_ON, 69));
    send(e.get(), ev(e.get(), 13, 2, WS_NOTE_ON, 69));
    send(e.get(), ev(e.get(), 1500, 1, WS_PARAMETER, 330));
    send(e.get(), ev(e.get(), 1500, 3, WS_PARAMETER, .8f));
    send(e.get(), ev(e.get(), 1777, 2, WS_NOTE_OFF));
    send(e.get(), ev(e.get(), 2345, 2, WS_NOTE_ON, 64, 0, 2));
    send(e.get(), ev(e.get(), 4001, 2, WS_NOTE_OFF, 0, 0, 1));
    send(e.get(), ev(e.get(), 5000, 2, WS_NOTE_OFF, 0, 0, 2));
    std::vector<float> result(8192);
    for (uint32_t i = 0; i < result.size();) {
        uint32_t n = std::min<uint32_t>(block, static_cast<uint32_t>(result.size()) - i);
        check(ws_engine_render(e.get(), result.data() + i, n) == WS_OK, "partition render");
        i += n;
    }
    return result;
}
void invariance() {
    auto reference = partition(32);
    for (uint32_t block : {64u, 128u, 256u, 512u}) {
        auto actual = partition(block);
        check(std::memcmp(reference.data(), actual.data(), reference.size() * sizeof(float)) == 0,
              "partition invariance");
    }
}
void envelope() {
    for (double rate : {44100., 48000., 96000.}) {
        auto e = make(rate);
        auto d = ws_engine_module(e.get(), 1);
        void *state = d->create(rate);
        check(state != nullptr, "ADSR create");
        ws_module_config config{sizeof(config), 512, rate, 1};
        check(d->prepare(state, &config) == 0, "ADSR prepare");
        ws_module_event on{WS_NOTE_ON, 0, 60, 1, 7};
        d->event(state, &on);
        auto process = [&](uint32_t frames) {
            std::vector<float> samples(frames);
            for (uint32_t pos = 0; pos < frames;) {
                auto n = std::min<uint32_t>(512, frames - pos);
                float *output[] = {samples.data() + pos};
                ws_process_context context{sizeof(context), n, pos, 1};
                d->process(state, nullptr, output, &context);
                pos += n;
            }
            return samples;
        };
        auto attack = process(static_cast<uint32_t>(std::llround(.01 * rate)));
        check(std::abs(attack.back() - 1) < 1e-6, "ADSR attack duration");
        auto decay = process(static_cast<uint32_t>(std::llround(.1 * rate)));
        check(std::abs(decay.back() - .7f) < 1e-6, "ADSR decay duration");
        ws_module_event stale{WS_NOTE_OFF, 0, 0, 2, 7};
        d->event(state, &stale);
        check(std::abs(process(1)[0] - .7f) < 1e-6, "ADSR source ownership");
        stale.source_id = 1;
        stale.note_id = 6;
        d->event(state, &stale);
        check(std::abs(process(1)[0] - .7f) < 1e-6, "ADSR note ownership");
        ws_module_event off{WS_NOTE_OFF, 0, 0, 1, 7};
        d->event(state, &off);
        auto release = process(static_cast<uint32_t>(std::llround(.2 * rate)));
        check(release.back() == 0, "ADSR release duration");
        check(process(32).back() == 0, "ADSR idle");
        d->destroy(state);
    }
}
void oscillator() {
    auto e = make();
    auto d = ws_engine_module(e.get(), 0);
    void *state = d->create(48000);
    ws_module_config c{sizeof(c), 512, 48000, 1};
    d->prepare(state, &c);
    ws_module_event freq{WS_PARAMETER, 0, 440, 0, 0};
    d->event(state, &freq);
    std::vector<float> samples(48000);
    for (uint32_t pos = 0; pos < samples.size();) {
        auto n = std::min<uint32_t>(512, static_cast<uint32_t>(samples.size()) - pos);
        float *output[] = {samples.data() + pos};
        ws_process_context context{sizeof(context), n, pos, 1};
        d->process(state, nullptr, output, &context);
        pos += n;
    }
    uint32_t crossings = 0;
    for (size_t i = 1; i < samples.size(); ++i)
        if (samples[i - 1] < 0 && samples[i] >= 0)
            ++crossings;
    check(crossings == 439 || crossings == 440, "VCO 440Hz crossings");
    for (size_t i = 0; i < samples.size(); i += 127)
        check(std::abs(samples[i] - std::sin(6.283185307179586 * 440 * i / 48000)) < 1e-5,
              "VCO sine accuracy");
    d->destroy(state);
}
void validation() {
    ws_engine_config bad{sizeof(bad), std::numeric_limits<double>::quiet_NaN(), 512, 32, 32};
    check(!ws_engine_create(&bad), "NaN sample rate");
    auto e = make();
    auto event = ev(e.get(), 0, 1, WS_PARAMETER, std::numeric_limits<float>::quiet_NaN());
    check(ws_engine_enqueue(e.get(), &event) == WS_INVALID, "NaN param");
    event.value = 0;
    check(ws_engine_enqueue(e.get(), &event) == WS_INVALID, "range param");
    event.value = 220;
    event.parameter_id = 99;
    check(ws_engine_enqueue(e.get(), &event) == WS_NOT_FOUND, "unknown parameter");
    event.parameter_id = 0;
    event.node_id = 99;
    check(ws_engine_enqueue(e.get(), &event) == WS_NOT_FOUND, "unknown node");
    send(e.get(), ev(e.get(), 100, 2, WS_NOTE_ON, 60));
    event = ev(e.get(), 99, 2, WS_NOTE_OFF);
    check(ws_engine_enqueue(e.get(), &event) == WS_INVALID, "unordered events");
    check(ws_engine_add_node(e.get(), 5, "essentials.vco") == WS_STATE, "prepared graph immutable");
    float output[513]{};
    check(ws_engine_render(e.get(), output, 513) == WS_INVALID, "oversized block");
    check(ws_engine_render(e.get(), output, 0) == WS_INVALID, "zero block");
    check(ws_engine_reset(e.get()) == WS_OK, "reset");
    check(ws_engine_enqueue(e.get(), &event) == WS_STATE, "stale generation");
}
void graph() {
    auto donor = make();
    auto e = make(48000, 512, 64, false);
    for (uint32_t i = 0; i < 4; ++i)
        check(ws_engine_register_module(e.get(), ws_engine_module(donor.get(), i)) == WS_OK,
              "register descriptor");
    check(ws_engine_add_node(e.get(), 1, "essentials.vca") == WS_OK, "add node");
    check(ws_engine_add_node(e.get(), 1, "essentials.vca") == WS_INVALID, "duplicate id");
    check(ws_engine_add_node(e.get(), 2, "essentials.vca") == WS_OK, "add second");
    check(ws_engine_add_node(e.get(), 3, "essentials.vco") == WS_OK, "add VCO");
    check(ws_engine_connect(e.get(), 99, 0, 1, 0) == WS_NOT_FOUND, "bad graph id");
    check(ws_engine_connect(e.get(), 3, 0, 1, 1) == WS_GRAPH, "bad port kind");
    check(ws_engine_connect(e.get(), 1, 0, 2, 0) == WS_OK, "edge1");
    check(ws_engine_connect(e.get(), 2, 0, 1, 0) == WS_OK, "edge2");
    check(ws_engine_prepare(e.get(), 2, 0) == WS_GRAPH, "cycle reject");
}
void queue_and_panic() {
    auto e = make(48000, 512, 2);
    send(e.get(), ev(e.get(), 0, 2, WS_NOTE_ON, 60));
    send(e.get(), ev(e.get(), 90000, 2, WS_NOTE_ON, 70));
    auto event = ev(e.get(), 90001, 2, WS_NOTE_OFF);
    check(ws_engine_enqueue(e.get(), &event) == WS_CAPACITY, "bounded queue");
    check(ws_engine_panic(e.get()) == WS_OK, "panic while full");
    float buffer[512];
    check(ws_engine_render(e.get(), buffer, 512) == WS_OK, "panic render");
    check(std::all_of(buffer, buffer + 512, [](float s) { return s == 0; }),
          "panic discards old note-ons");
    send(e.get(), ev(e.get(), 512, 2, WS_NOTE_ON, 60));
    check(ws_engine_render(e.get(), buffer, 512) == WS_OK, "postpanic render");
    check(std::any_of(buffer, buffer + 512, [](float s) { return s != 0; }), "new generation gate");
    check(ws_engine_deactivate(e.get()) == WS_OK, "deactivate");
    check(ws_engine_render(e.get(), buffer, 512) == WS_OK, "inactive render");
    check(std::all_of(buffer, buffer + 512, [](float s) { return s == 0; }), "inactive silence");
    check(ws_engine_activate(e.get()) == WS_OK, "activate");
    check(ws_engine_reset(e.get()) == WS_OK, "lifecycle reset");
    ws_meter meter{};
    meter.struct_size = sizeof(meter);
    check(ws_engine_get_meter(e.get(), &meter) == WS_OK && meter.frame_position == 0 &&
              meter.queued_events == 0,
          "reset meters");
}
void no_allocations() {
    auto e = make(48000, 512, 1024, false);
    const ws_module_descriptor *descriptors[] = {ws_test_vco_entry(), ws_test_adsr_entry(),
                                                 ws_test_vca_entry(), ws_test_output_entry()};
    for (uint32_t i = 0; i < 4; ++i) {
        check(ws_engine_register_module(e.get(), descriptors[i]) == WS_OK, "instrumented registry");
        check(ws_engine_add_node(e.get(), i + 1, descriptors[i]->id) == WS_OK, "instrumented node");
    }
    check(ws_engine_connect(e.get(), 1, 0, 3, 0) == WS_OK &&
              ws_engine_connect(e.get(), 2, 0, 3, 1) == WS_OK &&
              ws_engine_connect(e.get(), 3, 0, 4, 0) == WS_OK &&
              ws_engine_prepare(e.get(), 4, 0) == WS_OK,
          "instrumented graph");
    send(e.get(), ev(e.get(), 0, 2, WS_NOTE_ON, 60));
    float buffer[512];
    allocations.store(0);
    watch_allocations.store(true);
    void *sentinel = ::operator new(8);
    ::operator delete(sentinel);
    watch_allocations.store(false);
    check(allocations.load() == 1, "allocation instrumentation sentinel");
    allocations.store(0);
    watch_allocations.store(true);
    for (int i = 0; i < 1000; ++i)
        ws_engine_render(e.get(), buffer, 512);
    watch_allocations.store(false);
    check(allocations.load() == 0, "allocation-free engine and Essentials render");
}
void overload_budget() {
    auto e = make(48000, 512, 1024);
    for (uint32_t i = 0; i < 300; ++i)
        send(e.get(), ev(e.get(), 0, 1, WS_PARAMETER, 220));
    float buffer[512];
    auto generation = ws_engine_generation(e.get());
    check(ws_engine_render(e.get(), buffer, 512) == WS_CAPACITY, "event work budget");
    check(std::all_of(buffer, buffer + 512, [](float v) { return v == 0; }), "overload silence");
    ws_meter meter{}; meter.struct_size = sizeof(meter); ws_engine_get_meter(e.get(), &meter);
    check(meter.overload_blocks == 1 && meter.generation > generation && meter.frame_position == 512,
          "overload meter and recovery generation");
    check(ws_engine_render(e.get(), buffer, 512) == WS_OK, "overload recovery");
    e = make(48000, 512, 1024);
    for (uint32_t i = 0; i < 80; ++i)
        send(e.get(), ev(e.get(), i, 1, WS_PARAMETER, 220));
    check(ws_engine_render(e.get(), buffer, 512) == WS_CAPACITY, "graph subdivision budget");
}
void concurrency() {
    auto e = make(48000, 32, 128);
    std::atomic<bool> done{false}, stop{false}, producer_ok{true};
    std::atomic<uint32_t> accepted{0};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::thread producer([&] {
        for (uint32_t i = 0; i < 10000 && !stop.load(); ++i) {
            auto event = ev(e.get(), i * 32, 1, WS_PARAMETER, 220 + static_cast<float>(i % 200));
            int result;
            while ((result = ws_engine_enqueue(e.get(), &event)) == WS_CAPACITY && !stop.load()) {
                if (std::chrono::steady_clock::now() > deadline) { stop.store(true); break; }
                std::this_thread::yield();
            }
            if (stop.load()) break;
            if (result != WS_OK) { producer_ok.store(false); break; }
            ++accepted;
        }
        done.store(true);
    });
    float buffer[32];
    bool render_ok = true;
    while (!done.load() && !stop.load()) {
        if (ws_engine_render(e.get(), buffer, 32) != WS_OK) { render_ok = false; stop.store(true); }
        if (std::chrono::steady_clock::now() > deadline) stop.store(true);
    }
    stop.store(true);
    producer.join(); // No throwing assertions while producer remains joinable.
    check(render_ok && producer_ok.load(), "SPSC valid results");
    check(accepted == 10000, "SPSC producer progress before watchdog");
}
void quiescent_setup() {
    ws_parameter_descriptor parameters[64];
    for (uint32_t i = 0; i < 64; ++i) parameters[i] = {i, "Value", 0, 2, 1, ""};
    auto descriptor = *ws_test_vca_entry(); descriptor.id = "test.many_parameters";
    descriptor.parameters = parameters; descriptor.parameter_count = 64;
    ws_engine_config config{sizeof(config), 48000, 512, 64, 128};
    Engine e(ws_engine_create(&config), ws_engine_destroy);
    check(ws_engine_register_module(e.get(), &descriptor) == WS_OK, "setup descriptor");
    for (uint32_t node = 1; node <= 64; ++node)
        check(ws_engine_add_node(e.get(), node, descriptor.id) == WS_OK, "setup node");
    check(ws_engine_prepare(e.get(), 64, 0) == WS_OK, "setup prepare");
    for (uint32_t node = 1; node <= 64; ++node)
        for (uint32_t parameter = 0; parameter < 64; ++parameter)
            check(ws_engine_initialize_parameter(e.get(), node, parameter, .5f) == WS_OK, "4096 initial parameters bypass live queue");
    check(ws_engine_initialize_parameter(e.get(), 1, 0, 3) == WS_INVALID, "setup range");
    check(ws_engine_initialize_parameter(e.get(), 1, 999, 1) == WS_NOT_FOUND, "setup unknown parameter");
    ws_meter meter{}; meter.struct_size = sizeof(meter); ws_engine_get_meter(e.get(), &meter);
    check(meter.queued_events == 0 && meter.frame_position == 0, "setup consumes no frames or events");
    send(e.get(), ev(e.get(), 0, 1, WS_PARAMETER, 1));
    check(ws_engine_initialize_parameter(e.get(), 1, 0, 1) == WS_STATE, "queued traffic rejects setup");
    float buffer[512]; check(ws_engine_render(e.get(), buffer, 512) == WS_OK, "large setup renders");
    check(ws_engine_initialize_parameter(e.get(), 1, 0, 1) == WS_STATE, "rendered graph rejects setup");
    check(ws_engine_reset(e.get()) == WS_OK && ws_engine_initialize_parameter(e.get(), 1, 0, 1) == WS_OK, "quiescent reset permits setup");
    for (uint32_t count : {256u, 257u}) {
        auto voice = make(48000, 512, 1024);
        for (uint32_t i = 0; i < count; ++i) send(voice.get(), ev(voice.get(), 0, 1, WS_PARAMETER, 330));
        check(ws_engine_render(voice.get(), buffer, 512) == (count == 256 ? WS_OK : WS_CAPACITY), "exact dispatch boundary");
    }
    for (uint32_t count : {64u, 65u}) {
        auto voice = make(48000, 512, 1024);
        for (uint32_t i = 0; i < count; ++i) send(voice.get(), ev(voice.get(), i, 1, WS_PARAMETER, 330));
        check(ws_engine_render(voice.get(), buffer, 512) == (count == 64 ? WS_OK : WS_CAPACITY), "exact graph span boundary");
    }
}

} // namespace
int main(int argc, char **argv) {
    try {
        directory = argc > 1 ? argv[1] : ".";
        invariance();
        envelope();
        oscillator();
        validation();
        graph();
        queue_and_panic();
        no_allocations();
        overload_budget();
        quiescent_setup();
        concurrency();
        for (int i = 0; i < 20; ++i) {
            auto e = make();
        }
        std::cout << "PASS " << checks
                  << " checks: partition invariance, envelope timing, oscillator, validation, "
                     "graph, panic, lifecycle, allocation-free render, SPSC\n";
        return 0;
    } catch (const std::exception &e) {
        watch_allocations.store(false);
        std::cerr << "FAIL: " << e.what() << "\n";
        return 1;
    }
}
