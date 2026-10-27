#include "wirescape/engine.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
using Library = HMODULE;
static Library open_library(const char *p) {
    return LoadLibraryW(std::filesystem::u8path(p).c_str());
}
static void close_library(Library p) { FreeLibrary(p); }
static ws_module_entry_fn entry(Library p) {
    return reinterpret_cast<ws_module_entry_fn>(GetProcAddress(p, "ws_module_entry"));
}
#else
#include <dlfcn.h>
using Library = void *;
static Library open_library(const char *p) { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
static void close_library(Library p) { dlclose(p); }
static ws_module_entry_fn entry(Library p) {
    return reinterpret_cast<ws_module_entry_fn>(dlsym(p, "ws_module_entry"));
}
#endif
namespace {
static_assert(std::atomic<uint64_t>::is_always_lock_free,
              "Wirescape requires lock-free 64-bit atomics (64-bit Windows/Linux ARM64)");
static_assert(std::atomic<float>::is_always_lock_free,
              "Wirescape requires lock-free float atomics");
struct LibraryGuard {
    Library handle;
    ~LibraryGuard() {
        if (handle)
            close_library(handle);
    }
};
struct Node {
    uint32_t id;
    const ws_module_descriptor *descriptor;
    void *state;
    std::vector<int> sources, source_ports;
    std::vector<std::vector<float>> buffers;
    std::vector<const float *> input_pointers;
    std::vector<float *> output_pointers;
};
bool valid_descriptor(const ws_module_descriptor *d) {
    if (!d || d->struct_size != sizeof(*d) || d->abi_version != WS_ABI_VERSION || !d->id ||
        !d->id[0] || !d->name || d->input_count > 8 || !d->output_count || d->output_count > 8 ||
        d->parameter_count > 64 || !d->outputs || !d->create || !d->destroy || !d->prepare ||
        !d->reset || !d->event || !d->process)
        return false;
    if ((d->input_count && !d->inputs) || (d->parameter_count && !d->parameters))
        return false;
    for (uint32_t i = 0; i < d->input_count; ++i) {
        auto &p = d->inputs[i];
        if (!p.id || !p.id[0] || !p.name || !p.name[0] ||
            (p.kind != WS_AUDIO && p.kind != WS_CONTROL))
            return false;
        for (uint32_t j = 0; j < i; ++j)
            if (std::strcmp(d->inputs[j].id, p.id) == 0)
                return false;
    }
    for (uint32_t i = 0; i < d->output_count; ++i) {
        auto &p = d->outputs[i];
        if (!p.id || !p.id[0] || !p.name || !p.name[0] ||
            (p.kind != WS_AUDIO && p.kind != WS_CONTROL))
            return false;
        for (uint32_t j = 0; j < i; ++j)
            if (std::strcmp(d->outputs[j].id, p.id) == 0)
                return false;
    }
    for (uint32_t i = 0; i < d->parameter_count; ++i) {
        auto &p = d->parameters[i];
        if (!p.name || !p.name[0] || !p.unit || !std::isfinite(p.minimum) ||
            !std::isfinite(p.maximum) || !std::isfinite(p.initial) || p.minimum > p.initial ||
            p.initial > p.maximum)
            return false;
        for (uint32_t j = 0; j < i; ++j)
            if (d->parameters[j].id == p.id)
                return false;
    }
    return true;
}
} // namespace
struct ws_engine {
    ws_engine_config config;
    std::vector<Library> libraries;
    std::vector<const ws_module_descriptor *> modules;
    std::vector<Node> nodes;
    std::vector<uint32_t> order;
    std::vector<ws_event> events;
    std::atomic<uint64_t> head{0}, tail{0}, position{0}, blocks{0}, late{0}, generation{1},
        overflow{0}, overload{0};
    std::atomic<float> peak{0};
    std::atomic<uint32_t> overload_reason{0};
    std::atomic<bool> prepared{false}, active{false};
    std::atomic_flag rendering = ATOMIC_FLAG_INIT;
    uint64_t last_event_frame = 0, producer_generation = 1, applied_generation = 1;
    uint32_t output_node = 0, output_port = 0;
    char error[256]{};
    explicit ws_engine(ws_engine_config c) : config(c), events(c.max_events) {
        nodes.reserve(c.max_nodes);
        modules.reserve(c.max_nodes);
        libraries.reserve(c.max_nodes);
    }
    ~ws_engine() {
        for (auto &n : nodes)
            n.descriptor->destroy(n.state);
        for (auto l : libraries)
            close_library(l);
    }
    int fail(int code, const char *message) {
        std::snprintf(error, sizeof(error), "%s", message);
        return code;
    }
    int find(uint32_t id) const {
        for (size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].id == id)
                return static_cast<int>(i);
        return -1;
    }
};
extern "C" {
uint32_t WS_CALL ws_engine_abi_version() { return WS_ABI_VERSION; }
ws_engine *WS_CALL ws_engine_create(const ws_engine_config *c) {
    if (!c || c->struct_size != sizeof(*c) || !std::isfinite(c->sample_rate) ||
        c->sample_rate < 8000 || c->sample_rate > 384000 || !c->max_block_frames ||
        c->max_block_frames > 65536 || !c->max_nodes || c->max_nodes > 256 || !c->max_events ||
        c->max_events > 65536)
        return nullptr;
    try {
        return new ws_engine(*c);
    } catch (...) {
        return nullptr;
    }
}
void WS_CALL ws_engine_destroy(ws_engine *e) { delete e; }
const char *WS_CALL ws_engine_error(const ws_engine *e) { return e ? e->error : "Invalid engine"; }
int WS_CALL ws_engine_register_module(ws_engine *e, const ws_module_descriptor *d) {
    if (!e)
        return WS_INVALID;
    if (e->prepared.load())
        return e->fail(WS_STATE, "Graph is already prepared");
    if (!valid_descriptor(d))
        return e->fail(WS_INVALID, "Invalid module descriptor or ABI");
    for (auto m : e->modules)
        if (std::strcmp(m->id, d->id) == 0)
            return e->fail(WS_INVALID, "Duplicate module ID");
    if (e->modules.size() >= e->config.max_nodes)
        return e->fail(WS_CAPACITY, "Module capacity exceeded");
    try {
        e->modules.push_back(d);
        return WS_OK;
    } catch (...) {
        return e->fail(WS_CAPACITY, "Cannot store module");
    }
}
int WS_CALL ws_engine_load_module(ws_engine *e, const char *path) {
    if (!e || !path)
        return WS_INVALID;
    if (e->prepared.load())
        return WS_STATE;
    try {
        LibraryGuard library{open_library(path)};
        if (!library.handle)
            return e->fail(WS_LIBRARY, "Cannot open module library");
        auto f = entry(library.handle);
        if (!f)
            return e->fail(WS_LIBRARY, "Module entry point missing");
        auto result = ws_engine_register_module(e, f());
        if (result != WS_OK)
            return result;
        try {
            e->libraries.push_back(library.handle);
        } catch (...) {
            e->modules.pop_back();
            throw;
        }
        library.handle = nullptr;
        return WS_OK;
    } catch (...) {
        return e->fail(WS_LIBRARY, "Module load failed");
    }
}
uint32_t WS_CALL ws_engine_module_count(const ws_engine *e) {
    return e ? static_cast<uint32_t>(e->modules.size()) : 0;
}
const ws_module_descriptor *WS_CALL ws_engine_module(const ws_engine *e, uint32_t i) {
    return e && i < e->modules.size() ? e->modules[i] : nullptr;
}
int WS_CALL ws_engine_add_node(ws_engine *e, uint32_t id, const char *module) {
    if (!e || !module || !id)
        return WS_INVALID;
    if (e->prepared.load())
        return WS_STATE;
    if (e->find(id) >= 0)
        return e->fail(WS_INVALID, "Duplicate node ID");
    if (e->nodes.size() >= e->config.max_nodes)
        return WS_CAPACITY;
    const ws_module_descriptor *d = nullptr;
    for (auto m : e->modules)
        if (std::strcmp(m->id, module) == 0)
            d = m;
    if (!d)
        return WS_NOT_FOUND;
    void *state = nullptr;
    try {
        Node n{};
        n.id = id;
        n.descriptor = d;
        n.sources.assign(d->input_count, -1);
        n.source_ports.assign(d->input_count, 0);
        n.input_pointers.assign(d->input_count, nullptr);
        n.output_pointers.resize(d->output_count);
        n.buffers.resize(d->output_count);
        for (auto &b : n.buffers)
            b.resize(e->config.max_block_frames);
        state = d->create(e->config.sample_rate);
        if (!state)
            return WS_CAPACITY;
        n.state = state;
        e->nodes.push_back(std::move(n));
        return WS_OK;
    } catch (...) {
        if (state)
            d->destroy(state);
        return e->fail(WS_CAPACITY, "Cannot allocate node buffers");
    }
}
int WS_CALL ws_engine_connect(ws_engine *e, uint32_t src, uint32_t sp, uint32_t dst, uint32_t dp) {
    if (!e)
        return WS_INVALID;
    if (e->prepared.load())
        return WS_STATE;
    int si = e->find(src), di = e->find(dst);
    if (si < 0 || di < 0)
        return WS_NOT_FOUND;
    auto &s = e->nodes[si];
    auto &d = e->nodes[di];
    if (sp >= s.descriptor->output_count || dp >= d.descriptor->input_count)
        return WS_INVALID;
    if (s.descriptor->outputs[sp].kind != d.descriptor->inputs[dp].kind)
        return e->fail(WS_GRAPH, "Port kinds do not match");
    if (d.sources[dp] >= 0)
        return e->fail(WS_GRAPH, "Input already connected");
    d.sources[dp] = si;
    d.source_ports[dp] = static_cast<int>(sp);
    return WS_OK;
}
int WS_CALL ws_engine_prepare(ws_engine *e, uint32_t output, uint32_t port) {
    if (!e)
        return WS_INVALID;
    if (e->prepared.load())
        return WS_STATE;
    int oi = e->find(output);
    if (oi < 0)
        return WS_NOT_FOUND;
    if (port >= e->nodes[oi].descriptor->output_count ||
        e->nodes[oi].descriptor->outputs[port].kind != WS_AUDIO)
        return WS_INVALID;
    try {
        std::vector<uint32_t> degree(e->nodes.size(), 0), order;
        order.reserve(e->nodes.size());
        for (size_t i = 0; i < e->nodes.size(); ++i)
            for (int s : e->nodes[i].sources)
                if (s >= 0)
                    ++degree[i];
        for (uint32_t i = 0; i < degree.size(); ++i)
            if (!degree[i])
                order.push_back(i);
        for (size_t cursor = 0; cursor < order.size(); ++cursor)
            for (uint32_t i = 0; i < degree.size(); ++i)
                for (int s : e->nodes[i].sources)
                    if (s == static_cast<int>(order[cursor]) && !--degree[i])
                        order.push_back(i);
        if (order.size() != e->nodes.size())
            return e->fail(WS_GRAPH, "Graph contains a cycle");
        ws_module_config config{sizeof(ws_module_config), e->config.max_block_frames,
                                e->config.sample_rate, e->generation.load()};
        for (auto &n : e->nodes)
            if (n.descriptor->prepare(n.state, &config) != 0)
                return e->fail(WS_STATE, "Module prepare failed");
        e->order = std::move(order);
        e->output_node = static_cast<uint32_t>(oi);
        e->output_port = port;
        e->prepared.store(true, std::memory_order_release);
        e->active.store(true);
        return WS_OK;
    } catch (...) {
        return e->fail(WS_CAPACITY, "Cannot prepare graph");
    }
}
uint64_t WS_CALL ws_engine_generation(const ws_engine *e) {
    return e ? e->generation.load(std::memory_order_acquire) : 0;
}
int WS_CALL ws_engine_enqueue(ws_engine *e, const ws_event *event) {
    if (!e || !event || event->struct_size != sizeof(*event) || !std::isfinite(event->value))
        return WS_INVALID;
    if (!e->prepared.load(std::memory_order_acquire))
        return WS_STATE;
    auto gen = e->generation.load(std::memory_order_acquire);
    if (event->generation != gen)
        return WS_STATE;
    if (e->producer_generation != gen) {
        e->last_event_frame = 0;
        e->producer_generation = gen;
    }
    if (event->frame < e->last_event_frame)
        return WS_INVALID;
    int ni = e->find(event->node_id);
    if (ni < 0)
        return WS_NOT_FOUND;
    auto d = e->nodes[ni].descriptor;
    if (event->type == WS_PARAMETER) {
        bool found = false;
        for (uint32_t i = 0; i < d->parameter_count; ++i) {
            auto &p = d->parameters[i];
            if (event->parameter_id == p.id) {
                found = true;
                if (event->value < p.minimum || event->value > p.maximum)
                    return WS_INVALID;
            }
        }
        if (!found)
            return WS_NOT_FOUND;
    } else if (event->type == WS_NOTE_ON) {
        if (event->value < 0 || event->value > 127)
            return WS_INVALID;
    } else if (event->type == WS_GATE) {
        if (event->value < 0 || event->value > 1)
            return WS_INVALID;
    } else if (event->type != WS_NOTE_OFF)
        return WS_INVALID;
    auto head = e->head.load(std::memory_order_relaxed);
    if (head - e->tail.load(std::memory_order_acquire) >= e->events.size()) {
        e->overflow.fetch_add(1, std::memory_order_relaxed);
        return WS_CAPACITY;
    }
    e->events[head % e->events.size()] = *event;
    e->last_event_frame = event->frame;
    e->head.store(head + 1, std::memory_order_release);
    return WS_OK;
}
int WS_CALL ws_engine_initialize_parameter(ws_engine *e, uint32_t node, uint32_t parameter, float value) {
    if (!e || !std::isfinite(value)) return WS_INVALID;
    if (!e->prepared.load() || e->position.load() || e->blocks.load() ||
        e->head.load() != e->tail.load()) return WS_STATE;
    if (e->rendering.test_and_set(std::memory_order_acquire)) return WS_BUSY;
    int result = WS_NOT_FOUND;
    const int index = e->find(node);
    if (index >= 0) {
        auto &n = e->nodes[index];
        for (uint32_t i = 0; i < n.descriptor->parameter_count; ++i) {
            const auto &p = n.descriptor->parameters[i];
            if (p.id != parameter) continue;
            result = WS_INVALID;
            if (value >= p.minimum && value <= p.maximum) {
                ws_module_event event{WS_PARAMETER, parameter, value, 0, 0};
                n.descriptor->event(n.state, &event);
                result = WS_OK;
            }
            break;
        }
    }
    e->rendering.clear(std::memory_order_release);
    return result;
}
int WS_CALL ws_engine_render(ws_engine *e, float *output, uint32_t frames) {
    if (!e || !output || !frames || frames > e->config.max_block_frames)
        return WS_INVALID;
    if (!e->prepared.load(std::memory_order_acquire))
        return WS_STATE;
    if (e->rendering.test_and_set(std::memory_order_acquire))
        return WS_BUSY;
    if (!e->active.load()) {
        std::fill_n(output, frames, 0.f);
        e->rendering.clear(std::memory_order_release);
        return WS_OK;
    }
    const auto gen = e->generation.load(std::memory_order_acquire);
    const auto head = e->head.load(std::memory_order_acquire);
    auto tail = e->tail.load(std::memory_order_relaxed);
    if (gen != e->applied_generation) {
        for (auto &n : e->nodes)
            n.descriptor->reset(n.state);
        e->applied_generation = gen;
    }
    const uint64_t start = e->position.load(std::memory_order_relaxed), end = start + frames;
    uint32_t offset = 0;
    uint32_t examined = 0, spans = 0;
    auto overload = [&](uint32_t reason) {
        std::fill_n(output, frames, 0.f);
        e->tail.store(head, std::memory_order_release);
        e->generation.fetch_add(1, std::memory_order_acq_rel);
        e->overload.fetch_add(1, std::memory_order_relaxed);
        e->overload_reason.store(reason, std::memory_order_relaxed);
        e->position.store(end, std::memory_order_release);
        e->peak.store(0); e->blocks.fetch_add(1);
        e->rendering.clear(std::memory_order_release);
        return WS_CAPACITY;
    };
    float peak = 0;
    while (offset < frames) {
        uint64_t now = start + offset;
        while (tail < head) {
            const auto &ev = e->events[tail % e->events.size()];
            if (ev.generation < gen) {
                if (++examined > 256) return overload(1);
                ++tail;
                continue;
            }
            if (ev.generation > gen || ev.frame > now)
                break;
            if (++examined > 256) return overload(1);
            if (ev.frame < now)
                e->late.fetch_add(1, std::memory_order_relaxed);
            int ni = e->find(ev.node_id);
            ws_module_event me{ev.type, ev.parameter_id, ev.value, ev.source_id, ev.note_id};
            e->nodes[ni].descriptor->event(e->nodes[ni].state, &me);
            ++tail;
        }
        uint32_t count = frames - offset;
        if (tail < head) {
            auto frame = e->events[tail % e->events.size()].frame;
            if (frame < end && frame > now)
                count = static_cast<uint32_t>(frame - now);
        }
        if (++spans > 64) return overload(2);
        for (auto index : e->order) {
            auto &n = e->nodes[index];
            for (size_t i = 0; i < n.sources.size(); ++i)
                n.input_pointers[i] =
                    n.sources[i] < 0
                        ? nullptr
                        : e->nodes[n.sources[i]].buffers[n.source_ports[i]].data() + offset;
            for (size_t i = 0; i < n.buffers.size(); ++i)
                n.output_pointers[i] = n.buffers[i].data() + offset;
            ws_process_context context{sizeof(ws_process_context), count, now, gen};
            n.descriptor->process(n.state, n.input_pointers.data(), n.output_pointers.data(),
                                  &context);
        }
        const float *source = e->nodes[e->output_node].buffers[e->output_port].data() + offset;
        for (uint32_t i = 0; i < count; ++i) {
            float value = std::isfinite(source[i]) ? source[i] : 0;
            output[offset + i] = value;
            peak = std::max(peak, std::abs(value));
        }
        offset += count;
    }
    e->tail.store(tail, std::memory_order_release);
    e->position.store(end, std::memory_order_release);
    e->peak.store(peak, std::memory_order_relaxed);
    e->blocks.fetch_add(1, std::memory_order_relaxed);
    e->rendering.clear(std::memory_order_release);
    return WS_OK;
}
int WS_CALL ws_engine_reset(ws_engine *e) {
    if (!e || !e->prepared.load())
        return WS_STATE;
    if (e->rendering.test_and_set(std::memory_order_acquire))
        return WS_BUSY;
    for (auto &n : e->nodes)
        n.descriptor->reset(n.state);
    e->tail.store(e->head.load());
    e->position.store(0);
    e->blocks.store(0);
    e->late.store(0);
    e->overflow.store(0);
    e->overload.store(0);
    e->overload_reason.store(0);
    e->peak.store(0);
    e->applied_generation = e->generation.fetch_add(1, std::memory_order_acq_rel) + 1;
    e->rendering.clear(std::memory_order_release);
    return WS_OK;
}
int WS_CALL ws_engine_activate(ws_engine *e) {
    if (!e || !e->prepared.load())
        return WS_STATE;
    e->active.store(true);
    return WS_OK;
}
int WS_CALL ws_engine_deactivate(ws_engine *e) {
    if (!e || !e->prepared.load())
        return WS_STATE;
    e->active.store(false);
    return WS_OK;
}
int WS_CALL ws_engine_panic(ws_engine *e) {
    if (!e || !e->prepared.load())
        return WS_STATE;
    e->generation.fetch_add(1, std::memory_order_acq_rel);
    return WS_OK;
}
int WS_CALL ws_engine_get_meter(const ws_engine *e, ws_meter *m) {
    if (!e || !m || m->struct_size != sizeof(*m))
        return WS_INVALID;
    m->frame_position = e->position.load();
    m->rendered_blocks = e->blocks.load();
    m->late_events = e->late.load();
    m->peak = e->peak.load();
    auto tail = e->tail.load();
    auto head = e->head.load();
    m->queued_events = static_cast<uint32_t>(head >= tail ? head - tail : 0);
    m->generation = e->generation.load();
    m->overflow_events = e->overflow.load();
    m->overload_blocks = e->overload.load();
    m->overload_reason = e->overload_reason.load();
    return WS_OK;
}
int WS_CALL ws_engine_build_essentials(ws_engine *e, const char *directory) {
    if (!e || !directory)
        return WS_INVALID;
    try {
#ifdef _WIN32
        const char *names[] = {"ws_vco.dll", "ws_adsr.dll", "ws_vca.dll", "ws_output.dll"};
#elif defined(__APPLE__)
        const char *names[] = {"libws_vco.dylib", "libws_adsr.dylib", "libws_vca.dylib",
                               "libws_output.dylib"};
#else
        const char *names[] = {"libws_vco.so", "libws_adsr.so", "libws_vca.so", "libws_output.so"};
#endif
        const char *ids[] = {"essentials.vco", "essentials.adsr", "essentials.vca",
                             "essentials.output"};
        for (int i = 0; i < 4; ++i) {
            auto path = (std::filesystem::u8path(directory) / names[i]).u8string();
            int r = ws_engine_load_module(e, path.c_str());
            if (r)
                return r;
            r = ws_engine_add_node(e, i + 1, ids[i]);
            if (r)
                return r;
        }
        int r = ws_engine_connect(e, 1, 0, 3, 0);
        if (r)
            return r;
        r = ws_engine_connect(e, 2, 0, 3, 1);
        if (r)
            return r;
        r = ws_engine_connect(e, 3, 0, 4, 0);
        if (r)
            return r;
        return ws_engine_prepare(e, 4, 0);
    } catch (...) {
        return e->fail(WS_LIBRARY, "Cannot build Essentials graph");
    }
}
}
