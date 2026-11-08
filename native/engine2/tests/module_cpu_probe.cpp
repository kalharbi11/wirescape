// SPDX-License-Identifier: GPL-3.0-only
// Offline, device-free CPU probe for one built-in module: N instances, a saw into the first
// signal input, seeded noise into the second, parameters at their defaults unless --param sets
// them (--param id=value, repeatable) and --alternate id=a:b:K flips one between two values every
// K blocks (forces mode crossfades, band-layout changes and so on). Prints one line:
//   module_cpu_probe {"module":..., "render_ns":{"max":..,"p99":..}, "allocations":.., ...}
#include "../../tests2/allocation_watch.h"
#include "module_catalog.h"
#include "module_cpu_probe_output.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {
struct Arena {
    std::vector<unsigned char> bytes;
    size_t used = 0;
    static void *WS2_CALL alloc(void *context, size_t size, size_t alignment, uint32_t) {
        auto &a = *static_cast<Arena *>(context);
        const uintptr_t base = reinterpret_cast<uintptr_t>(a.bytes.data());
        const uintptr_t next = (base + a.used + alignment - 1) & ~(uintptr_t(alignment) - 1);
        if (next - base + size > a.bytes.size()) return nullptr;
        a.used = next - base + size;
        return a.bytes.data() + (next - base);
    }
};

struct Instance {
    const ws2_descriptor *d = nullptr;
    Arena arena;
    ws2_host host{sizeof(ws2_host), &arena, Arena::alloc};
    void *self = nullptr;
    std::vector<std::vector<float>> out_store;
    std::vector<std::vector<float *>> out_rows;
    std::vector<ws2_signal_out> outputs;
    std::vector<const float *> in_rows;
    std::vector<ws2_signal> inputs;
    std::vector<float> values;
    std::vector<const float *> value_rows;
    std::vector<ws2_signal> params;
    std::vector<ws2_event_list> event_inputs;
    ws2_process ctx{};
};

int fail(const char *message) {
    std::fprintf(stderr, "module_cpu_probe: %s\n", message);
    return 2;
}
} // namespace

int main(int argc, char **argv) {
    std::string id;
    uint32_t instances = 2, block = 128;
    double rate = 48000, seconds = 60;
    uint64_t seed = 42;
    std::vector<std::pair<std::string, float>> settings;
    std::string alternate_id, settings_text;
    float alternate_a = 0, alternate_b = 0;
    uint32_t alternate_every = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string key = argv[i], value = argv[i + 1];
        if (key == "--module") id = value;
        else if (key == "--instances") instances = uint32_t(std::stoul(value));
        else if (key == "--rate") rate = std::stod(value);
        else if (key == "--block") block = uint32_t(std::stoul(value));
        else if (key == "--seconds") seconds = std::stod(value);
        else if (key == "--seed") seed = std::stoull(value);
        else if (key == "--param") {
            const size_t eq = value.find('=');
            if (eq == std::string::npos || !eq) return fail("--param needs id=value");
            settings.emplace_back(value.substr(0, eq), std::stof(value.substr(eq + 1)));
            settings_text += (settings_text.empty() ? "" : ",") + value;
        } else if (key == "--alternate") {
            const size_t eq = value.find('='), c1 = value.find(':', eq), c2 = value.find(':', c1 + 1);
            if (eq == std::string::npos || !eq || c1 == std::string::npos || c2 == std::string::npos)
                return fail("--alternate needs id=a:b:blocks");
            alternate_id = value.substr(0, eq);
            alternate_a = std::stof(value.substr(eq + 1, c1 - eq - 1));
            alternate_b = std::stof(value.substr(c1 + 1, c2 - c1 - 1));
            alternate_every = uint32_t(std::stoul(value.substr(c2 + 1)));
            if (!alternate_every) return fail("--alternate needs blocks >= 1");
        } else return fail("unknown argument");
    }
    if (id.empty() || !instances || instances > 16 || !block || block > 4096 || !(rate >= 8000) || !(seconds > 0))
        return fail("usage: --module ID [--instances N] [--rate HZ] [--block FRAMES] [--seconds S] [--seed N]");
    const ws2_descriptor *d = nullptr;
    for (const ws2_descriptor *m : wirescape::app::builtin_modules())
        if (id == m->id) d = m;
    if (!d) return fail("module is not a built-in");
    auto param_index = [&](const std::string &name) -> int {
        for (uint32_t p = 0; p < d->param_count; ++p)
            if (name == d->params[p].id) return int(p);
        return -1;
    };
    for (const auto &[name, value] : settings)
        if (param_index(name) < 0) return fail("--param names an unknown parameter");
    const int alternate = alternate_id.empty() ? -1 : param_index(alternate_id);
    if (!alternate_id.empty() && alternate < 0) return fail("--alternate names an unknown parameter");

    constexpr uint32_t channels = 16;
    const ws2_config config{sizeof(ws2_config), rate, block, channels, seed, nullptr, 0};
    ws2_memory memory{sizeof(ws2_memory), 0, 0, 0};
    if (d->memory_requirements(&config, &memory) != WS2_OK) return fail("memory_requirements failed");
    std::vector<std::unique_ptr<Instance>> all;
    const size_t frames_total = size_t(seconds * rate);
    std::vector<float> saw(block), noise(block), zero(block, 0.f);
    for (uint32_t n = 0; n < instances; ++n) {
        auto x = std::make_unique<Instance>();
        x->d = d;
        x->arena.bytes.resize(memory.fast_bytes + memory.large_bytes + (1u << 20));
        x->self = d->create(&x->host);
        if (!x->self || d->prepare(x->self, &config, &x->host) != WS2_OK) return fail("create/prepare failed");
        x->out_store.assign(size_t(d->output_count) * channels, std::vector<float>(block));
        x->out_rows.resize(d->output_count);
        for (uint32_t o = 0; o < d->output_count; ++o) {
            for (uint32_t c = 0; c < channels; ++c)
                x->out_rows[o].push_back(x->out_store[size_t(o) * channels + c].data());
            x->outputs.push_back({sizeof(ws2_signal_out), x->out_rows[o].data(), 1, 0, channels});
        }
        x->in_rows.resize(d->input_count);
        for (uint32_t p = 0; p < d->input_count; ++p) {
            x->in_rows[p] = p == 0 ? saw.data() : p == 1 ? noise.data() : zero.data();
            x->inputs.push_back({sizeof(ws2_signal), &x->in_rows[p], 1, uint32_t(p < 2 ? WS2_CONNECTED : 0)});
            x->event_inputs.push_back({sizeof(ws2_event_list), 0, nullptr});
        }
        x->values.resize(d->param_count);
        x->value_rows.resize(d->param_count);
        for (uint32_t p = 0; p < d->param_count; ++p) {
            x->values[p] = d->params[p].initial;
            for (const auto &[name, value] : settings)
                if (name == d->params[p].id) x->values[p] = value;
            x->value_rows[p] = &x->values[p];
            x->params.push_back({sizeof(ws2_signal), &x->value_rows[p], 1, WS2_CONSTANT});
        }
        auto &c = x->ctx;
        c.struct_size = sizeof(c);
        c.frames = block;
        c.sample_rate = rate;
        c.channels = 1;
        c.input_count = d->input_count;
        c.output_count = d->output_count;
        c.param_count = d->param_count;
        c.inputs = x->inputs.data();
        c.outputs = x->outputs.data();
        c.params = x->params.data();
        c.event_inputs = x->event_inputs.data();
        c.events = {sizeof(ws2_event_list), 0, nullptr};
        all.push_back(std::move(x));
    }

    const size_t blocks = frames_total / block;
    std::vector<int64_t> ns(blocks);
    double phase = 0;
    module_cpu_probe::OutputEvidence output_evidence;
    uint32_t lcg = uint32_t(seed) * 2654435761u + 1u;
    size_t allocations = 0, faults = 0;
    for (size_t k = 0; k < blocks; ++k) {
        for (uint32_t i = 0; i < block; ++i) { // inputs are prepared outside the timed region
            phase += 110.0 / rate;
            if (phase >= 1) phase -= 1;
            saw[i] = float(phase - .5);
            lcg = lcg * 1664525u + 1013904223u;
            noise[i] = .3f * (float(lcg >> 8) / 8388608.f - 1.f);
        }
        if (alternate >= 0)
            for (auto &x : all) x->values[size_t(alternate)] = (k / alternate_every) % 2 ? alternate_b : alternate_a;
        const auto start = std::chrono::steady_clock::now();
        {
            allocation_watch::Scope watch;
            for (auto &x : all) {
                if (d->process(x->self, &x->ctx) != WS2_OK) ++faults;
                x->ctx.frame_position += block;
            }
            allocations += watch.allocations();
        }
        ns[k] = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
        for (auto &x : all)
            for (uint32_t o = 0; o < d->output_count; ++o)
                output_evidence.scan(x->outputs[o], block, channels);
    }
    for (auto &x : all) d->destroy(x->self);
    std::vector<int64_t> sorted = ns;
    std::sort(sorted.begin(), sorted.end());
    const int64_t worst = sorted.empty() ? 0 : sorted.back();
    const int64_t p99 = sorted.empty() ? 0 : sorted[std::min(sorted.size() - 1, size_t(double(sorted.size()) * .99))];
    std::printf("module_cpu_probe {\"module\":\"%s\",\"instances\":%u,\"sample_rate\":%.0f,\"block\":%u,\"seconds\":%g,"
                "\"blocks\":%zu,\"render_ns\":{\"max\":%lld,\"p99\":%lld},\"period_ns\":%.0f,\"allocations\":%zu,"
                "\"faults\":%zu,\"peak\":%.6g,\"params\":\"%s\",\"alternate\":\"%s\"}\n",
                id.c_str(), instances, rate, block, seconds, blocks, (long long)worst, (long long)p99,
                1e9 * block / rate, allocations, faults + output_evidence.faults, output_evidence.peak, settings_text.c_str(),
                alternate_id.empty() ? ""
                                     : (alternate_id + "=" + std::to_string(alternate_a) + ":" +
                                        std::to_string(alternate_b) + ":" + std::to_string(alternate_every))
                                           .c_str());
    return 0;
}
