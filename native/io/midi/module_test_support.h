// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../../sdk/examples/test_support.h"
#include "../../tests2/allocation_watch.h"
#include <vector>
#include <algorithm>
struct MidiArena {
    std::vector<uint8_t> storage = std::vector<uint8_t>(1024 * 1024); size_t used = 0;
    static void* WS2_CALL alloc(void* c, size_t n, size_t align, uint32_t) {
        auto& a = *static_cast<MidiArena*>(c); uintptr_t base = reinterpret_cast<uintptr_t>(a.storage.data());
        uintptr_t next = (base + a.used + align - 1) & ~(uintptr_t(align) - 1); if (next - base + n > a.storage.size()) return nullptr;
        a.used = next - base + n; return reinterpret_cast<void*>(next);
    }
};
template<class T> struct MidiRig {
    MidiArena arena; ws2_host host{sizeof(host), &arena, MidiArena::alloc};
    const ws2_descriptor* d = ws::Module<T>::descriptor(); void* instance = nullptr;
    struct Buffers { float output[8][16][256]{}; };
    std::unique_ptr<Buffers> buffer = std::make_unique<Buffers>();
    float values[32]{}; const float* parameter_rows[32]{}; ws2_signal params[32]{};
    ws2_signal input{sizeof(ws2_signal), nullptr, 0, 0};
    float* output_rows[8][16]{}; ws2_signal_out outputs[8]{}; ws2_event_out event_outputs[8]{};
    ws2_event_list input_events{}; ws2_process ctx{};
    std::vector<ws2_event> emitted; size_t event_limit = 20000;
    static int WS2_CALL push(void* context, uint32_t port, const ws2_event* event) {
        auto& r = *static_cast<MidiRig*>(context); if (r.emitted.size() >= r.event_limit) return WS2_CAPACITY;
        auto copy = *event; copy.port = port; r.emitted.push_back(copy); return WS2_OK;
    }
    explicit MidiRig(const uint8_t* config = nullptr, uint32_t bytes = 0, double rate = 48000) {
        emitted.reserve(20000); instance = d->create(&host); REQUIRE(instance);
        ws2_config cfg{sizeof(cfg), rate, 256, 16, 42, config, bytes}; REQUIRE(d->prepare(instance, &cfg, &host) == WS2_OK);
        for (uint32_t i = 0; i < d->param_count; ++i) { values[i] = d->params[i].initial; parameter_rows[i] = &values[i]; params[i] = {sizeof(ws2_signal), &parameter_rows[i], 1, WS2_CONSTANT}; }
        for (uint32_t p = 0; p < d->output_count; ++p) { for (uint32_t c = 0; c < 16; ++c) output_rows[p][c] = buffer->output[p][c]; outputs[p] = {sizeof(ws2_signal_out), output_rows[p], 1, 0, 16}; event_outputs[p] = {sizeof(ws2_event_out), this, p, push}; }
        ctx.struct_size = sizeof(ctx); ctx.sample_rate = rate; ctx.channels = 1; ctx.input_count = d->input_count; ctx.output_count = d->output_count; ctx.param_count = d->param_count;
        ctx.inputs = &input; ctx.params = params; ctx.outputs = outputs; ctx.event_outputs = event_outputs; ctx.event_inputs = &input_events;
    }
    ~MidiRig() { d->destroy(instance); }
    void run(std::vector<ws2_event> events = {}, uint32_t frames = 32) {
        std::stable_sort(events.begin(), events.end(), [](const ws2_event& a, const ws2_event& b) { return a.offset < b.offset; });
        input_events = {sizeof(ws2_event_list), uint32_t(events.size()), events.data()}; ctx.events = input_events; ctx.frames = frames;
        int result; { allocation_watch::Scope scope; result = d->process(instance, &ctx); REQUIRE(scope.allocations() == 0); }
        input_events = {}; ctx.events = {};
        REQUIRE(result == WS2_OK); ctx.frame_position += frames;
        for (uint32_t p = 0; p < d->output_count; ++p) if (d->outputs[p].kind == WS2_SIGNAL) for (uint32_t c = 0; c < outputs[p].channels; ++c) for (uint32_t i = 0; i < frames; ++i) REQUIRE(std::isfinite(buffer->output[p][c][i]));
    }
    float sample(uint32_t p, uint32_t channel = 0, uint32_t frame = 31) const { return buffer->output[p][channel][frame]; }
};
inline ws2_event midi1(uint8_t status, uint8_t a, uint8_t b, uint32_t offset = 0, uint32_t source = 1) {
    ws2_event e{}; e.struct_size = sizeof(e); e.type = WS2_MIDI; e.channel = -1; e.note_id = -1; e.source_id = source; e.offset = offset; e.ump_words = 1;
    e.ump[0] = (status < 0xf0 ? 0x20000000u : 0x10000000u) | uint32_t(status) << 16 | uint32_t(a) << 8 | b; return e;
}
inline ws2_event midi2(uint8_t status, uint8_t key, uint32_t value, uint32_t offset = 0, uint32_t source = 1) {
    auto e = midi1(status, key, 0, offset, source); e.ump[0] = 0x40000000u | uint32_t(status) << 16 | uint32_t(key) << 8; e.ump[1] = value; e.ump_words = 2; return e;
}
inline ws2_event source_choke(uint32_t source = UINT32_MAX, uint32_t offset = 0) { ws2_event e{}; e.struct_size = sizeof(e); e.type = WS2_NOTE_CHOKE; e.source_id = source; e.channel = -1; e.note_id = -1; e.offset = offset; return e; }
