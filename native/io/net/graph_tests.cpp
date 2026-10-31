// SPDX-License-Identifier: GPL-3.0-only
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include "../osc/codec.h"
#include "osc_service.h"
#include "runtime_patch.h"
#include <atomic>
#include <chrono>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
struct GraphSink {
    AppEngine *engine;
    std::atomic<int> result{-1};
    std::atomic<uint64_t> timestamp{0};
    static int deliver(void *context, uint32_t node, const ws2_event &event, uint64_t timestamp) noexcept {
        auto &self = *static_cast<GraphSink *>(context);
        const int status = app_engine_native(self.engine)
                               .enqueue(2, node, WS2_DIRECT_PORT, event, timestamp, wirescape::engine2::Timing::Timestamped,
                                        app_engine_generation(self.engine));
        self.timestamp.store(timestamp, std::memory_order_release);
        self.result.store(status, std::memory_order_release);
        return status;
    }
};
int main() {
    const std::string patch =
        R"({"format":"wirescape.native.patch","version":2,"settings":{},"modules":[{"id":1,"module":"osc.in","version":1,"parameters":{},"config":{"rows":[{"pattern":"/pico1/knob/*","smoothing_ms":0}]}},{"id":2,"module":"essentials.vco","version":1,"parameters":{}}],"connections":[],"outputs":[{"module":2,"port":"audio","bus_channel":0}]})";
    ws_engine_config config{sizeof(config), 48000, 128, 64, 2048};
    std::unique_ptr<AppEngine, decltype(&app_engine_destroy)> catalog(app_engine_create(&config), app_engine_destroy);
    std::string error;
    std::unique_ptr<AppEngine, decltype(&app_engine_destroy)> engine(
        wirescape::document::load_runtime_patch(patch, catalog.get(), config, error), app_engine_destroy);
    REQUIRE(bool(engine));
    GraphSink sink{engine.get()};
    wirescape::osc::Service service({&sink, GraphSink::deliver});
    REQUIRE(service.add({1, 0, false}) && service.start());
    wirescape::osc::Message message;
    message.address = "/pico1/knob/3";
    message.count = 1;
    message.arguments[0].type = 'f';
    message.arguments[0].real = .75f;
    uint8_t packet[1472]{};
    bool shortened = false;
    const auto bytes = wirescape::osc::encode(message, packet, sizeof(packet), shortened);
    const auto sender = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(service.bound_port());
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto sent_at = app_engine_clock_ns();
    REQUIRE(sendto(sender, reinterpret_cast<const char *>(packet), int(bytes), 0,
                   reinterpret_cast<sockaddr *>(&address), sizeof(address)) == int(bytes));
    for (uint32_t attempt = 0; attempt < 2000 && sink.result.load(std::memory_order_acquire) == -1; ++attempt)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    service.stop(); // producer stops and joins before render/engine teardown
#ifdef _WIN32
    closesocket(sender);
#else
    close(sender);
#endif
    REQUIRE(sink.result.load() == WS2_OK && service.counters().received == 1);
    const auto callback_time = app_engine_clock_ns();
    REQUIRE(sink.timestamp.load() >= sent_at && sink.timestamp.load() <= callback_time);
    REQUIRE(sink.timestamp.load() != 0);
    float output[128];
    float *channels[] = {output};
    int status;
    {
        allocation_watch::Scope scope;
        status = app_engine_render_live(engine.get(), channels, 1, 128, callback_time);
        REQUIRE(scope.allocations() == 0);
    }
    REQUIRE(status == WS_OK);
    // Timestamped ingress has one max-block of bounded scheduling lead.
    REQUIRE(app_engine_native(engine.get()).output_sample(1, 0, 0, 0) == 0);
    {
        allocation_watch::Scope scope;
        status = app_engine_render_live(engine.get(), channels, 1, 128, app_engine_clock_ns());
        REQUIRE(scope.allocations() == 0);
    }
    REQUIRE(status == WS_OK);
    REQUIRE(app_engine_native(engine.get()).output_sample(1, 0, 0, 0) == .75f);
    REQUIRE(app_engine_native(engine.get()).output_sample(1, 0, 0, 127) == .75f);
}

