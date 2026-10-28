// SPDX-License-Identifier: GPL-3.0-only
#include "midi_service.h"
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include <map>
using namespace wirescape::midi;
struct Fake : Backend {
    std::vector<Endpoint> list{{"A", "sys:A"}, {"B", "sys:B"}};
    std::map<std::string, Callback> callbacks;
    uint32_t opens = 0, closes = 0;
    std::vector<Endpoint> endpoints() override { return list; }
    bool open(const Endpoint& e, Callback cb) override { if (callbacks.count(e.system_name)) return false; callbacks[e.system_name] = cb; ++opens; return true; }
    void close(const Endpoint& e) override { callbacks.erase(e.system_name); ++closes; }
    void bytes(const char* system, std::initializer_list<uint8_t> data, uint64_t ns) {
        const auto cb = callbacks.at(system); { allocation_watch::Scope scope; cb.message(cb.context, data.begin(), data.size(), ns); REQUIRE(scope.allocations() == 0); }
    }
    void ump(const char* system, std::initializer_list<uint32_t> data, uint64_t ns) { const auto cb = callbacks.at(system); cb.ump(cb.context, data.begin(), data.size(), ns); }
};
struct Output {
    struct Item { uint32_t node; ws2_event event; uint64_t ns; };
    std::vector<Item> items;
    Output() { items.reserve(20000); }
    static int send(void* context, uint32_t node, const ws2_event& event, uint64_t ns) noexcept { static_cast<Output*>(context)->items.push_back({node, event, ns}); return WS2_OK; }
    Sink sink() { return {this, send}; }
};
static void fanout_identity_disconnect() {
    Fake backend; Output out; Service service(backend, out.sink());
    REQUIRE(service.enable({"A", "sys:A"})); REQUIRE(service.enable({"B", "sys:B"}));
    REQUIRE(service.subscribe({10, {"A", "sys:A"}, false})); REQUIRE(service.subscribe({11, {}, true})); service.poll();
    REQUIRE(backend.opens == 2);
    backend.bytes("sys:A", {0x90, 60, 127}, 6); backend.bytes("sys:B", {0x91, 62, 64}, 9); service.poll();
    REQUIRE(out.items.size() == 3); REQUIRE(out.items[0].node == 10 && out.items[1].node == 11 && out.items[2].node == 11);
    REQUIRE(out.items[0].ns == 6 && out.items[0].event.ump[0] == 0x20903c7fu);
    REQUIRE(out.items[0].event.source_id != out.items[2].event.source_id); REQUIRE(backend.opens == 2);
    std::reverse(backend.list.begin(), backend.list.end()); service.poll(); REQUIRE(backend.opens == 2);
    backend.list.erase(std::remove_if(backend.list.begin(), backend.list.end(), [](const Endpoint& e) { return e.name == "A"; }), backend.list.end());
    out.items.clear(); service.poll(); REQUIRE(backend.closes == 1); REQUIRE(out.items.size() == 2);
    for (auto& item : out.items) REQUIRE(item.event.type == WS2_NOTE_CHOKE && item.event.source_id == 1);
    REQUIRE(!service.endpoint_states()[0].online && service.endpoint_states()[1].online);
    backend.list.push_back({"A renamed", "sys:A"}); service.poll(); REQUIRE(backend.opens == 3); REQUIRE(service.endpoint_states()[0].online);
    REQUIRE(service.endpoint_states()[0].saved.name == "A"); // saved label, never reassigned by enumeration index
}
static void protocol_bounds_and_reserve() {
    Fake backend; Output out; Service service(backend, out.sink()); REQUIRE(service.enable({"A", "sys:A"})); REQUIRE(service.subscribe({1, {}, true})); service.poll();
    backend.bytes("sys:A", {0x70, 60, 1}, 0); backend.bytes("sys:A", {0x90, 200, 1}, 0); backend.bytes("sys:A", {0xf0, 1, 2, 3, 0xf7}, 0);
    REQUIRE(service.counters().malformed == 2 && service.counters().oversize == 1);
    backend.bytes("sys:A", {0xb0, 101, 0}, 0); backend.bytes("sys:A", {0xb0, 100, 0}, 0); backend.bytes("sys:A", {0xb0, 6, 12}, 0); service.poll();
    REQUIRE(service.counters().ignored_rpn == 1); out.items.clear();
    backend.bytes("sys:A", {0x90, 60, 0}, 1); backend.ump("sys:A", {0x40903c00u, 0u}, 2); service.poll();
    REQUIRE(out.items.size() == 2 && out.items[0].event.ump_words == 1 && out.items[1].event.ump_words == 2);
    REQUIRE((out.items[0].event.ump[0] >> 28) == 2 && (out.items[1].event.ump[0] >> 28) == 4);
    out.items.clear();
    for (uint32_t i = 0; i < 5000; ++i) backend.bytes("sys:A", {0x90, 60, 100}, i);
    backend.bytes("sys:A", {0x80, 60, 0}, 5001); service.poll();
    REQUIRE(out.items.size() == 3841 && ((out.items.back().event.ump[0] >> 20) & 15) == 8);
    REQUIRE(service.counters().ring_full > 0);
    out.items.clear();
    for (uint32_t i = 0; i < 5000; ++i) backend.bytes("sys:A", {0x80, 60, 0}, i);
    service.poll(); REQUIRE(!out.items.empty() && out.items[0].event.type == WS2_NOTE_CHOKE);
}
static void missing_and_shutdown() {
    Fake backend; Output out; Service service(backend, out.sink()); REQUIRE(service.enable({"Missing", "gone"})); REQUIRE(service.subscribe({1, {}, true})); service.poll();
    REQUIRE(backend.opens == 0 && !service.endpoint_states()[0].online);
    REQUIRE(service.enable({"A", "sys:A"})); service.poll(); REQUIRE(backend.opens == 1);
    backend.bytes("sys:A", {0x90, 60, 127}, 1); service.stop();
    REQUIRE(backend.callbacks.empty() && !out.items.empty() && out.items.back().event.type == WS2_NOTE_CHOKE);
    REQUIRE(!real_backend(false));
}
int main() { fanout_identity_disconnect(); protocol_bounds_and_reserve(); missing_and_shutdown(); std::puts("MIDI fake endpoint, identity, protocol, reserve and shutdown passed"); }
