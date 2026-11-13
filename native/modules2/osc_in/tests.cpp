// SPDX-License-Identifier: GPL-3.0-only
#include "../../io/midi/module_test_support.h"
#include "dsp.h"
using namespace wirescape;
struct Rig {
    MidiArena arena;
    ws2_host host{sizeof(host), &arena, MidiArena::alloc};
    const ws2_descriptor *d = ws::Module<osc::OscIn>::descriptor();
    void *state;
    struct Buffers {
        float values[9][16][256]{};
    };
    std::unique_ptr<Buffers> data = std::make_unique<Buffers>();
    float *rows[9][16]{};
    ws2_signal_out outputs[9]{};
    ws2_event_out events[9]{};
    ws2_process ctx{};
    uint32_t emitted = 0;
    static int WS2_CALL push(void *p, uint32_t, const ws2_event *) {
        ++static_cast<Rig *>(p)->emitted;
        return WS2_OK;
    }
    explicit Rig(const osc::Configuration &cfg, double rate = 48000) {
        state = d->create(&host);
        REQUIRE(state);
        ws2_config c{sizeof(c), rate, 256, 16, 1, reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg)};
        REQUIRE(d->prepare(state, &c, &host) == WS2_OK);
        for (uint32_t p = 0; p < 9; ++p) {
            for (uint32_t ch = 0; ch < 16; ++ch) rows[p][ch] = data->values[p][ch];
            outputs[p] = {sizeof(ws2_signal_out), rows[p], 1, 0, 16};
            events[p] = {sizeof(ws2_event_out), this, p, push};
        }
        ctx.struct_size = sizeof(ctx);
        ctx.sample_rate = rate;
        ctx.channels = 1;
        ctx.output_count = 9;
        ctx.outputs = outputs;
        ctx.event_outputs = events;
    }
    ~Rig() { d->destroy(state); }
    void run(const std::vector<ws2_event> &input = {}, uint32_t frames = 32) {
        ctx.frames = frames;
        ctx.events = {sizeof(ws2_event_list), uint32_t(input.size()), input.data()};
        int result;
        {
            allocation_watch::Scope scope;
            result = d->process(state, &ctx);
            REQUIRE(scope.allocations() == 0);
        }
        REQUIRE(result == WS2_OK);
        ctx.frame_position += frames;
        for (uint32_t p = 0; p < 8; ++p)
            for (uint32_t c = 0; c < outputs[p].channels; ++c)
                for (uint32_t i = 0; i < frames; ++i) REQUIRE(std::isfinite(data->values[p][c][i]));
    }
};
static std::vector<uint8_t> packet(const char *address, float value, bool argument = true) {
    osc::Message m;
    m.address = address;
    m.count = argument ? 1 : 0;
    m.arguments[0].type = 'f';
    m.arguments[0].real = value;
    std::vector<uint8_t> p(1472);
    bool truncated = false;
    p.resize(osc::encode(m, p.data(), p.size(), truncated));
    return p;
}
static ws2_event message(const std::vector<uint8_t> &bytes, uint32_t offset = 0) {
    ws2_event e{};
    e.struct_size = sizeof(e);
    e.type = WS2_MESSAGE;
    e.message_kind = WS2_MSG_OSC;
    e.source_id = 1;
    e.channel = e.note_id = -1;
    e.data = bytes.data();
    e.bytes = uint32_t(bytes.size());
    e.offset = offset;
    return e;
}
int main() {
    osc::Statistics stats;
    osc::Configuration cfg;
    cfg.statistics = &stats;
    std::strcpy(cfg.rows[0].pattern, "/pico1/knob/*");
    cfg.rows[0].input_max = 10;
    cfg.rows[0].smoothing_ms = 0;
    cfg.rows[0].timeout_ms = 1;
    cfg.rows[0].stale = 1;
    std::strcpy(cfg.rows[1].pattern, "/fire");
    cfg.rows[1].kind = 2;
    auto ten = packet("/pico1/knob/10", 10), three = packet("/pico1/knob/3", 3), fire = packet("/fire", 0, false);
    Rig r(cfg);
    r.run({message(ten), message(three), message(fire)});
    REQUIRE(r.outputs[0].channels == 2 && r.data->values[0][0][0] == .3f && r.data->values[0][1][0] == 1);
    REQUIRE(r.emitted == 3 && r.data->values[1][0][31] == 1);
    // Valid first bundle child + invalid second child is one rejected packet:
    // neither CV nor the raw Messages event output may change.
    auto invalid = three;
    invalid[0] = 'x';
    std::vector<uint8_t> malformed(16);
    std::memcpy(malformed.data(), "#bundle\0", 8);
    for (const auto *child : {&ten, &invalid}) {
        const size_t at = malformed.size();
        malformed.resize(at + 4 + child->size());
        osc::put32(malformed.data() + at, uint32_t(child->size()));
        std::memcpy(malformed.data() + at + 4, child->data(), child->size());
    }
    osc::Statistics rejectedStats;
    auto rejectedConfig = cfg;
    rejectedConfig.statistics = &rejectedStats;
    Rig rejected(rejectedConfig);
    rejected.run({message(malformed)});
    REQUIRE(rejectedStats.received == 0 && rejectedStats.malformed == 1);
    REQUIRE(rejected.emitted == 0 && rejected.data->values[0][0][0] == 0);
    r.run();
    REQUIRE(r.data->values[0][0][15] == .3f && r.data->values[0][0][16] == 0);
    REQUIRE(r.data->values[1][0][15] == 1 && r.data->values[1][0][16] == 0);
    std::vector<ws2_event> burst(100, message(fire));
    r.run(burst);
    REQUIRE(stats.dropped == 36);
    auto low = packet("/pico1/knob/3", 2), high = packet("/pico1/knob/3", 8);
    Rig latest(cfg);
    latest.run({message(low, 1), message(high, 5)});
    REQUIRE(latest.data->values[0][0][4] == 0 && latest.data->values[0][0][5] == .8f);
    for (double rate : {44100., 48000., 96000.}) {
        osc::Configuration stable;
        for (auto &row : stable.rows) row.smoothing_ms = 5;
        auto control = packet("/pico1/knob/1", .75f);
        Rig whole(stable, rate);
        whole.run({message(control)}, 256);
        std::vector<float> reference(whole.data->values[0][0], whole.data->values[0][0] + 256);
        for (uint32_t block : {1u, 17u, 64u}) {
            Rig split(stable, rate);
            std::vector<float> output;
            for (uint32_t at = 0; at < 256;) {
                const auto n = std::min(block, 256u - at);
                split.run(at ? std::vector<ws2_event>{} : std::vector<ws2_event>{message(control)}, n);
                output.insert(output.end(), split.data->values[0][0], split.data->values[0][0] + n);
                at += n;
            }
            REQUIRE(output == reference);
        }
    }
    {
        // #612: a disabled row ignores matching messages (CV holds output_min,
        // gate/trigger stay low); enabled rows and the Messages output still work.
        osc::Statistics offStats;
        osc::Configuration off;
        off.statistics = &offStats;
        off.rows[0].enabled = 0;
        off.rows[0].output_min = -.25f;
        off.rows[0].smoothing_ms = 0;
        off.rows[1].smoothing_ms = 0;
        std::strcpy(off.rows[2].pattern, "/fire");
        off.rows[2].kind = 2;
        off.rows[2].enabled = 0;
        auto knob1 = packet("/pico1/knob/1", .5f), knob2 = packet("/pico1/knob/2", .5f);
        auto unmatched = packet("/somewhere/else", 1);
        Rig disabled(off);
        disabled.run({message(knob1), message(knob2), message(fire), message(unmatched)});
        REQUIRE(disabled.data->values[0][0][0] == -.25f && disabled.data->values[0][0][31] == -.25f);
        REQUIRE(disabled.data->values[1][0][0] == .5f);
        REQUIRE(disabled.data->values[2][0][0] == 0 && disabled.data->values[2][0][31] == 0);
        // Disabled-only addresses (knob1, /fire) emit nothing on `messages`; the enabled
        // row and unmatched traffic (address discovery via Monitor) still pass.
        REQUIRE(disabled.emitted == 2 && offStats.received == 4 && offStats.dropped == 0);
        osc::Configuration bad;
        bad.rows[3].enabled = 2;
        REQUIRE(!osc::configuration(reinterpret_cast<const uint8_t *>(&bad), sizeof(bad), bad));
        osc::Configuration defaults;
        for (const auto &row : defaults.rows) REQUIRE(row.enabled == 1);
    }
    Bytes bytes;
    ws2_writer writer{sizeof(writer), &bytes, sink};
    REQUIRE(r.d->save_state(r.state, &writer) == WS2_OK);
    bytes[0] = 99;
    Source source_context{&bytes, 0};
    ws2_reader reader{sizeof(reader), &source_context, source, uint32_t(bytes.size())};
    REQUIRE(r.d->load_state(r.state, &reader) == WS2_UNSUPPORTED);
    bytes.clear();
    source_context.pos = 0;
    reader.bytes = 0;
    REQUIRE(r.d->load_state(r.state, &reader) == WS2_INVALID);
}
