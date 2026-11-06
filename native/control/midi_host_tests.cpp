// SPDX-License-Identifier: GPL-3.0-only
#include "../tests2/allocation_watch.h"
#include "midi_host.h"
#include "midi_service.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <thread>
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            std::fprintf(stderr, "%d: %s\n", __LINE__, #x);                                                            \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (0)
struct Fake: wirescape::midi::Backend {
 int enumerations=0,opens=0;
 std::vector<wirescape::midi::Endpoint>endpoints()override{++enumerations;return {{"Controller","system-name"}};}
 bool open(const wirescape::midi::Endpoint&,wirescape::midi::Callback)override{++opens;return true;}
 void close(const wirescape::midi::Endpoint&)override{}
};
struct KeyboardBackend : wirescape::midi::Backend {
    std::atomic<bool> present{true}, ready{false};
    std::atomic<unsigned> opens{0}, closes{0};
    wirescape::midi::Callback callbacks[2]{};
    std::vector<wirescape::midi::Endpoint> endpoints() override {
        return present.load(std::memory_order_acquire)
                   ? std::vector<wirescape::midi::Endpoint>{{"Keyboard", "notes"}, {"Pad", "pad-notes"}}
                   : std::vector<wirescape::midi::Endpoint>{};
    }
    bool open(const wirescape::midi::Endpoint &endpoint, wirescape::midi::Callback cb) override {
        callbacks[endpoint.name == "Keyboard" ? 0 : 1] = cb;
        ++opens;
        ready.store(true, std::memory_order_release);
        return true;
    }
    void close(const wirescape::midi::Endpoint &) override {
        ready.store(false, std::memory_order_release);
        ++closes;
    }
};
static bool wait_gate(AppEngine *engine, bool wanted) {
    float samples[128]{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        // Render like the live callback: real service chokes carry steady-clock stamps.
        float *rows[]{samples};
        CHECK(app_engine_render_live(engine, rows, 1, 128, app_engine_clock_ns()) == WS_OK);
        bool gate = false;
        for (float value : samples) gate |= value > .5f;
        if (gate == wanted) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}
static void global_keyboard_notes() {
    ws_engine_config config{sizeof(config), 48000, 128, 16, 2048};
    std::unique_ptr<AppEngine, decltype(&app_engine_destroy)> engine(app_engine_create(&config), app_engine_destroy);
    CHECK(engine);
    CHECK(app_engine_add_node(engine.get(), 1, "host.keyboard") == WS_OK);
    CHECK(app_engine_prepare(engine.get(), 1, 1) == WS_OK); // Observe the keyboard gate without any device.
    auto backend = std::make_unique<KeyboardBackend>();
    auto *fake = backend.get();
    wirescape::midi::Host host(true, std::move(backend));
    host.select({"Keyboard", "notes"}, true);
    host.select({"Pad", "pad-notes"}, true);
    std::string error;
    CHECK(host.start(engine.get(), R"({"modules":[{"id":1,"module":"host.keyboard"}]})", error));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!fake->ready.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(fake->ready.load(std::memory_order_acquire));
    const auto both_open = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (fake->opens.load(std::memory_order_acquire) < 2 && std::chrono::steady_clock::now() < both_open)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(fake->opens.load(std::memory_order_acquire) == 2);
    const auto midi1 = [&](uint8_t status, uint8_t key, uint8_t velocity) {
        const uint8_t bytes[]{status, key, velocity};
        fake->callbacks[0].message(fake->callbacks[0].context, bytes, sizeof(bytes), app_engine_clock_ns());
    };
    midi1(0x90, 60, 100);
    CHECK(wait_gate(engine.get(), true));
    midi1(0x80, 60, 0);
    CHECK(wait_gate(engine.get(), false));
    midi1(0x90, 61, 100);
    CHECK(wait_gate(engine.get(), true));
    midi1(0x90, 61, 0);
    CHECK(wait_gate(engine.get(), false));
    const auto ump = [&](uint32_t first, uint32_t second) {
        const uint32_t words[]{first, second};
        fake->callbacks[0].ump(fake->callbacks[0].context, words, 2, app_engine_clock_ns());
    };
    ump(0x40903c00, 0xffff0000);
    CHECK(wait_gate(engine.get(), true));
    ump(0x40803c00, 0xffff0000);
    CHECK(wait_gate(engine.get(), false));
    ump(0x40903c00, 0);
    CHECK(wait_gate(engine.get(), true)); // MIDI 2 zero velocity remains Note On.
    ump(0x40803c00, 0);
    CHECK(wait_gate(engine.get(), false));
    const uint8_t pad_on[]{0x90, 62, 100}, pad_off[]{0x80, 62, 0};
    fake->callbacks[1].message(fake->callbacks[1].context, pad_on, sizeof(pad_on), app_engine_clock_ns());
    CHECK(wait_gate(engine.get(), true));
    fake->callbacks[1].message(fake->callbacks[1].context, pad_off, sizeof(pad_off), app_engine_clock_ns());
    CHECK(wait_gate(engine.get(), false)); // Global keyboard receives notes from every enabled source.
    CHECK(host.counters().received == 10);
    // Device loss chokes held notes; the worker later reopens the same saved source.
    midi1(0x90, 64, 100);
    CHECK(wait_gate(engine.get(), true));
    fake->present.store(false, std::memory_order_release);
    const auto lost = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (fake->closes.load(std::memory_order_acquire) < 2 && std::chrono::steady_clock::now() < lost)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(fake->closes.load(std::memory_order_acquire) == 2);
    CHECK(wait_gate(engine.get(), false));
    fake->present.store(true, std::memory_order_release);
    const auto reopened = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (fake->opens.load(std::memory_order_acquire) < 4 && std::chrono::steady_clock::now() < reopened)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(fake->opens.load(std::memory_order_acquire) == 4);
    midi1(0x90, 65, 100);
    CHECK(wait_gate(engine.get(), true));
    midi1(0x80, 65, 0);
    CHECK(wait_gate(engine.get(), false));
    midi1(0x90, 67, 100);
    CHECK(wait_gate(engine.get(), true));
    CHECK(app_engine_panic(engine.get()) == WS_OK);
    float samples[128]{};
    CHECK(app_engine_render(engine.get(), samples, 128) == WS_OK);
    for (float value : samples) CHECK(value == 0);
    host.stop();
    host.stop();
}
// Real Host + Service + AppEngine, fake endpoints and callback. No Qt or sidecar.
static void live_subscriptions() {
    using App = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
    struct Sources {
        bool keyboard, midi;
    };
    auto graph = [](Sources sources) {
        ws_engine_config config{sizeof(config), 48000, 256, 16, 2048};
        App e(app_engine_create(&config), app_engine_destroy);
        CHECK(e);
        CHECK(app_engine_add_node(e.get(), 1, "host.keyboard") == WS_OK);
        CHECK(app_engine_add_node(e.get(), 4, "midi.to_cv") == WS_OK);
        if (sources.keyboard) CHECK(app_engine_add_node(e.get(), 2, "host.keyboard") == WS_OK);
        if (sources.midi) {
            CHECK(app_engine_add_node(e.get(), 3, "midi.in") == WS_OK);
            CHECK(app_engine_connect(e.get(), 3, 0, 4, 0) == WS_OK);
        }
        CHECK(app_engine_native(e.get()).output(4, 1, 1));
        if (sources.keyboard) CHECK(app_engine_native(e.get()).output(2, 1, 2));
        CHECK(app_engine_prepare(e.get(), 1, 1) == WS_OK);
        return e;
    };
    const std::string initial = R"({"modules":[{"id":1,"module":"host.keyboard"}]})";
    const std::string added =
        R"({"modules":[{"id":1,"module":"host.keyboard"},{"id":2,"module":"host.keyboard"},{"id":3,"module":"midi.in","config":{"endpoint":"Keyboard","system_name":"notes"}}]})";
    auto engine = graph({false, false});
    auto backend = std::make_unique<KeyboardBackend>();
    auto *fake = backend.get();
    wirescape::midi::Host host(true, std::move(backend));
    host.select({"Keyboard", "notes"}, true);
    std::string error;
    CHECK(host.start(engine.get(), initial, error));
    std::atomic<bool> running{true}, pause{false}, paused{false};
    std::atomic<unsigned> blocks{0};
    std::atomic<size_t> allocations{0};
    std::atomic<float> gates[3]{};
    std::thread render([&] {
        float samples[3][256]{};
        float *rows[]{samples[0], samples[1], samples[2]};
        while (running.load()) {
            if (pause.load()) {
                paused = true;
                std::this_thread::yield();
                continue;
            }
            paused = false;
            {
                allocation_watch::ThreadScope watch;
                CHECK(app_engine_render_live(engine.get(), rows, 3, 256, app_engine_clock_ns()) == WS_OK);
                allocations += watch.allocations();
            }
            for (unsigned i = 0; i < 3; ++i) gates[i] = samples[i][255];
            ++blocks;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    auto wait = [](auto condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!condition() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        CHECK(condition());
    };
    wait([&] { return fake->ready.load(std::memory_order_acquire); });
    auto note = [&](uint8_t status) {
        const uint8_t bytes[]{status, 60, 100};
        fake->callbacks[0].message(fake->callbacks[0].context, bytes, sizeof(bytes), app_engine_clock_ns());
    };
    note(0x90);
    wait([&] { return gates[0] > .5f; });
    const auto generation = app_engine_generation(engine.get());
    auto accept = [&](AppEngine *candidate, const std::string &document) {
        bool accepted = false;
        wait([&] {
            if (!accepted) accepted = host.replan(candidate, document, 16000000, error);
            return accepted;
        }); // A busy dispatch is a bounded refusal; retry without dropping the candidate.
    };
    auto candidate = graph({true, true});
    accept(candidate.get(), added);
    candidate.reset();
    const auto before = blocks.load();
    wait([&] { return blocks > before + 4; });
    CHECK(gates[0] > .5f && gates[1] == 0 && gates[2] == 0); // Held original note survives addition.
    note(0x80);
    wait([&] { return gates[0] == 0; }); // Retained subscription still receives releases.
    note(0x90);
    wait([&] { return gates[0] > .5f && gates[1] > .5f && gates[2] > .5f; });
    // Refused edit must preserve all accepted subscriptions and graph state.
    candidate = graph({false, false});
    CHECK(!host.replan(candidate.get(), "invalid", 16000000, error));
    CHECK(!host.replan(candidate.get(), initial, 0, error));
    note(0x80);
    wait([&] { return gates[0] == 0 && gates[1] == 0 && gates[2] == 0; });
    note(0x90);
    wait([&] { return gates[1] > .5f && gates[2] > .5f; });
    // Deterministic acknowledgement timeout: render cannot park. Then retry the same candidate.
    pause = true;
    wait([&] { return paused.load(); });
    CHECK(!host.replan(candidate.get(), initial, 1000000, error));
    CHECK(app_engine_generation(engine.get()) == generation);
    pause = false;
    const auto resumed = blocks.load();
    wait([&] { return blocks > resumed + 2; });
    accept(candidate.get(), initial);
    candidate.reset();
    const auto removed = blocks.load();
    wait([&] { return blocks > removed + 4; });
    CHECK(gates[1] == 0);  // Removed MIDI source chokes the surviving CV destination.
    CHECK(gates[0] > .5f); // Removing other sources does not choke the global keyboard.
    note(0x80);
    wait([&] { return gates[0] == 0; });
    note(0x90);
    wait([&] { return gates[0] > .5f; });
    const auto after = blocks.load();
    wait([&] { return blocks > after + 4; });
    CHECK(gates[1] == 0 && gates[2] == 0); // Removed routes do not see new input.
    CHECK(app_engine_generation(engine.get()) == generation);
    CHECK(fake->opens == 1 && fake->closes == 0); // No device close/reopen or producer replacement.
    CHECK(host.counters().send_fail == 0 && allocations == 0);
    host.stop();
    running = false;
    render.join();
}
int main(){
    global_keyboard_notes();
    live_subscriptions();
    wirescape::midi::Host disabled;
    CHECK(!disabled.authorized());
    CHECK(disabled.endpoints().empty());
    auto fake = std::make_unique<Fake>();
    auto *ptr = fake.get();
    wirescape::midi::Host host(false, std::move(fake));
    host.restore(R"({"devices":[{"kind":"midi","name":"Controller","system_name":"system-name","enabled":true}]})");
    bool rejected = false;
    try {
        host.restore(R"({"devices":[{"kind":"midi","name":123,"enabled":true}]})");
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected);
    CHECK(host.enabled().size() == 1);
    CHECK(host.enabled().size() == 1);
    CHECK(host.enabled()[0].system_name == "system-name");
    std::string error;
    CHECK(!host.start(nullptr, R"({"modules":[]})", error));
    CHECK(error.find("Enable MIDI") != std::string::npos);
    CHECK(ptr->enumerations == 0 && ptr->opens == 0);
    host.stop();
    host.stop();
    host.select({"Controller", "system-name"}, false);
    CHECK(host.enabled().empty());
    CHECK(host.start(nullptr, R"({"modules":[]})", error));
    std::puts("PASS host MIDI explicit authorization, saved-name restore and idempotent shutdown");
}
