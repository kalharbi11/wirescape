// SPDX-License-Identifier: GPL-3.0-only
#include "midi_host.h"
#include "midi_service.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <thread>
#define CHECK(x) do{if(!(x))std::abort();}while(0)
struct Fake: wirescape::midi::Backend {
 int enumerations=0,opens=0;
 std::vector<wirescape::midi::Endpoint>endpoints()override{++enumerations;return {{"Controller","system-name"}};}
 bool open(const wirescape::midi::Endpoint&,wirescape::midi::Callback)override{++opens;return true;}
 void close(const wirescape::midi::Endpoint&)override{}
};
struct KeyboardBackend : wirescape::midi::Backend {
    std::atomic<bool> ready{false};
    wirescape::midi::Callback callback{};
    std::vector<wirescape::midi::Endpoint> endpoints() override { return {{"Keyboard", "notes"}}; }
    bool open(const wirescape::midi::Endpoint &, wirescape::midi::Callback cb) override {
        callback = cb;
        ready.store(true, std::memory_order_release);
        return true;
    }
    void close(const wirescape::midi::Endpoint &) override { ready.store(false, std::memory_order_release); }
};
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
    std::string error;
    CHECK(host.start(engine.get(), R"({"modules":[{"id":1,"module":"host.keyboard"}]})", error));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!fake->ready.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(fake->ready.load(std::memory_order_acquire));
    const uint8_t note[]{0x90, 60, 100};
    fake->callback.message(fake->callback.context, note, sizeof(note), 0);
    bool gate = false;
    float samples[128]{};
    for (int i = 0; i < 500 && !gate; ++i) {
        CHECK(app_engine_render(engine.get(), samples, 128) == WS_OK);
        for (float value : samples) gate |= value > .5f;
        if (!gate) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(gate);
    CHECK(host.counters().received == 1);
    CHECK(app_engine_panic(engine.get()) == WS_OK);
    CHECK(app_engine_render(engine.get(), samples, 128) == WS_OK);
    for (float value : samples) CHECK(value == 0);
    host.stop();
    host.stop();
}
int main(){
    global_keyboard_notes();
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
    CHECK(error.find("--confirm-midi") != std::string::npos);
    CHECK(ptr->enumerations == 0 && ptr->opens == 0);
    host.stop();
    host.stop();
    host.select({"Controller", "system-name"}, false);
    CHECK(host.enabled().empty());
    CHECK(host.start(nullptr, R"({"modules":[]})", error));
    std::puts("PASS host MIDI explicit authorization, saved-name restore and idempotent shutdown");
}
