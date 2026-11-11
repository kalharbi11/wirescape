// SPDX-License-Identifier: GPL-3.0-only
// Browser demo engine: the real AppEngine catalogue and runtime patch loader
// behind a tiny C API for an AudioWorklet. Commands and renders run on the same
// (worklet) thread, so there is no cross-thread state. This is a demo build:
// graph commits allocate on that thread and crossfade instead of the sidecar's
// D5/live-replan paths. MIDI, OSC, devices and files do not exist here.
#include "web_engine.h"
#include "../document/telemetry_taps.h"
#include "app_engine.h"
#include "runtime_patch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

namespace {
using Json = nlohmann::json;
using Owner = std::unique_ptr<AppEngine, void (*)(AppEngine *)>;
constexpr uint32_t kMaxFrames = 1024, kFadeFrames = 480;

struct Refusal {
    std::string code, message;
};
void need(bool ok, const char *code = "invalid", const char *message = "Invalid command") {
    if (!ok) throw Refusal{code, message};
}
uint32_t integer(const Json &v, uint64_t lo = 0, uint64_t hi = UINT32_MAX) {
    need(v.is_number_integer() || v.is_number_unsigned());
    auto n = v.get<int64_t>();
    need(n >= int64_t(lo) && uint64_t(n) <= hi);
    return uint32_t(n);
}
double number(const Json &v, double lo, double hi) {
    need(v.is_number());
    auto n = v.get<double>();
    need(std::isfinite(n) && n >= lo && n <= hi, "refused", "Value outside range");
    return n;
}
const ws2_param *parameter(const ws2_descriptor *d, const std::string &id) {
    for (uint32_t i = 0; i < d->param_count; ++i)
        if (id == d->params[i].id) return &d->params[i];
    throw Refusal{"not-found", "No such parameter"};
}
const ws2_port *port(const ws2_descriptor *d, const std::string &id, bool output) {
    auto *p = output ? d->outputs : d->inputs;
    auto n = output ? d->output_count : d->input_count;
    for (uint32_t i = 0; i < n; ++i)
        if (id == p[i].id) return &p[i];
    throw Refusal{"not-found", "No such port"};
}
Json &node(Json &patch, uint32_t id) {
    for (auto &m : patch["modules"])
        if (m.at("id") == id) return m;
    throw Refusal{"not-found", "No such module"};
}
Json identity(const Json &c) {
    return Json::array({c.at("from").at("module"), c.at("from").at("port"), c.at("to").at("module"),
                        c.at("to").at("port"), c.value("kind", "signal")});
}
// Browser AudioWorklet has neither MIDI devices nor an OSC UDP ingress worker.
// Keep the live catalogue and loadGraph/addModule validation on the same gate.
bool web_module(const char *id) {
    return std::strncmp(id, "midi.", 5) != 0 && std::strncmp(id, "osc.", 4) != 0;
}

struct Web {
    double rate = 48000;
    ws_engine_config config{};
    Owner catalogue{nullptr, app_engine_destroy}, live{nullptr, app_engine_destroy}, old{nullptr, app_engine_destroy};
    Json patch;
    uint64_t generation = 0;
    uint32_t fade = 0, frames = 0;
    float left[kMaxFrames]{}, right[kMaxFrames]{}, scratch_l[kMaxFrames]{}, scratch_r[kMaxFrames]{};
    float peak = 0;
    std::set<uint32_t> held;
    std::string reply;
    wirescape::document::telemetry::Sampler telemetry;

    const ws2_descriptor *descriptor(const Json &m) {
        auto type = m.at("module").get<std::string>();
        auto *d = app_engine_native_descriptor(catalogue.get(), type.c_str());
        need(d && web_module(d->id), "unknown-module", "Module not available in the web demo");
        return d;
    }
    // Mirrors the sidecar's execution normalisation (native/sidecar/session.cpp).
    Json execution(Json p) {
        need(p.is_object() && p.value("format", "") == "wirescape.native.patch" && p.value("version", 0) == 2);
        need(p.at("modules").is_array() && p.at("connections").is_array());
        need(p["modules"].size() <= 64 && p["connections"].size() <= 128, "limit", "Graph capacity exceeded");
        if (!p.contains("settings")) p["settings"] = Json::object();
        std::set<uint32_t> ids;
        uint32_t output = UINT32_MAX;
        for (auto &m : p["modules"]) {
            auto id = integer(m.at("id"));
            need(ids.insert(id).second, "duplicate", "Duplicate module ID");
            auto *d = descriptor(m);
            need(!m.value("inactive", false), "unavailable", "Module is inactive");
            if (!m.contains("version")) m["version"] = d->version;
            if (std::string(d->id) == "essentials.output") output = std::min(output, id);
            auto values = m.value("parameters", Json::object());
            need(values.is_object());
            for (auto it = values.begin(); it != values.end(); ++it) {
                auto *q = parameter(d, it.key());
                number(it.value(), q->minimum, q->maximum);
            }
        }
        std::set<std::string> edges;
        for (const auto &c : p["connections"]) {
            need(c.is_object());
            auto *fd = descriptor(node(p, integer(c.at("from").at("module"))));
            auto *td = descriptor(node(p, integer(c.at("to").at("module"))));
            auto *f = port(fd, c.at("from").at("port").get<std::string>(), true);
            auto kind = c.at("kind").get<std::string>();
            need(edges.insert(identity(c).dump()).second, "duplicate", "Duplicate connection");
            if (kind == "modulation") {
                auto *q = parameter(td, c.at("to").at("port").get<std::string>());
                need(f->kind == WS2_SIGNAL, "kind-mismatch", "Modulation requires a signal source");
                need(q->flags & WS2_MODULATABLE, "refused", "Parameter cannot be modulated");
                number(c.at("amount"), -1, 1);
            } else {
                need(kind == "event" || kind == "signal");
                auto *t = port(td, c.at("to").at("port").get<std::string>(), false);
                auto expected = kind == "event" ? WS2_EVENT : WS2_SIGNAL;
                need(f->kind == expected && t->kind == expected, "kind-mismatch", "Cable requires matching port kinds");
            }
        }
        if (!p.contains("outputs")) {
            need(output != UINT32_MAX, "refused", "Patch needs an Output module");
            p["outputs"] = Json::array({{{"module", output}, {"port", "audio"}, {"bus_channel", 0}},
                                        {{"module", output}, {"port", "audio"}, {"bus_channel", 1}}});
        }
        return p;
    }
    void commit(Json candidate) {
        candidate = execution(std::move(candidate));
        std::string error;
        wirescape::document::telemetry::Sampler next_telemetry(
            wirescape::document::telemetry::plan(candidate, catalogue.get(), /*phase_enabled=*/true, /*groove_enabled=*/true));
        using wirescape::document::load_runtime_patch;
        Owner next(load_runtime_patch(candidate.dump(), catalogue.get(), config, error, &next_telemetry.taps()),
                   app_engine_destroy);
        need(bool(next), "refused", "Graph configuration refused");
        if (live) {
            app_engine_panic(live.get());
            old = std::move(live);
            fade = kFadeFrames;
        }
        live = std::move(next);
        telemetry.swap(next_telemetry); // Reset cumulative event cursors on every commit.
        patch = std::move(candidate);
        held.clear();
        ++generation;
    }
    void keyboard(bool on, uint32_t key, float velocity) {
        if (!live) return;
        ws2_event e{};
        e.struct_size = sizeof(e);
        e.type = on ? WS2_NOTE_ON : WS2_NOTE_OFF;
        e.note_id = -1;
        e.key = int32_t(key);
        e.value = velocity;
        for (const auto &m : patch["modules"])
            if (m.at("module") == "host.keyboard")
                need(app_engine_native(live.get())
                             .enqueue(0, integer(m.at("id")), WS2_DIRECT_PORT, e, 0,
                                      wirescape::engine2::Timing::Immediate) == WS2_OK,
                     "busy", "Event queue full");
        if (on) held.insert(key);
        else held.erase(key);
    }
    void apply(const Json &command) {
        auto type = command.at("type").get<std::string>();
        if (type == "loadGraph") return commit(command.at("patch"));
        if (type == "panic") {
            if (live) app_engine_panic(live.get());
            held.clear();
            return;
        }
        if (type == "noteOn" || type == "noteOff") {
            auto key = integer(command.at("note"), 0, 127);
            float velocity = type == "noteOn" ? float(number(command.at("velocity"), 0, 1)) : 0.f;
            return keyboard(type == "noteOn", key, velocity);
        }
        need(bool(live), "unavailable", "No graph loaded");
        if (type == "setParam") {
            auto id = integer(command.at("moduleId"));
            auto &m = node(patch, id);
            auto *d = descriptor(m);
            auto name = command.at("paramId").get<std::string>();
            auto *p = parameter(d, name);
            float value = float(number(command.at("value"), p->minimum, p->maximum));
            ws2_event e{};
            e.struct_size = sizeof(e);
            e.type = WS2_PARAM;
            e.index = uint32_t(p - d->params);
            e.value = value;
            need(app_engine_native(live.get())
                         .enqueue(0, id, WS2_DIRECT_PORT, e, 0, wirescape::engine2::Timing::Immediate) == WS2_OK,
                 "busy", "Event queue full");
            m["parameters"][name] = value;
            return;
        }
        Json candidate = patch;
        candidate.erase("outputs");
        if (type == "addModule") {
            auto id = integer(command.at("moduleId"));
            Json module = {{"id", id}, {"module", command.at("moduleType")}};
            if (command.contains("params")) module["parameters"] = command.at("params");
            candidate["modules"].push_back(module);
        } else if (type == "removeModule") {
            auto id = integer(command.at("moduleId"));
            node(candidate, id);
            auto &modules = candidate["modules"];
            modules.erase(std::remove_if(modules.begin(), modules.end(), [&](const Json &m) { return m["id"] == id; }),
                          modules.end());
            auto &cables = candidate["connections"];
            cables.erase(
                std::remove_if(cables.begin(), cables.end(),
                               [&](const Json &c) { return c["from"]["module"] == id || c["to"]["module"] == id; }),
                cables.end());
            if (patch.contains("outputs")) {
                Json outputs = Json::array();
                for (const auto &o : patch["outputs"])
                    if (o["module"] != id) outputs.push_back(o);
                if (!outputs.empty()) candidate["outputs"] = outputs;
            }
        } else if (type == "connect" || type == "disconnect" || type == "setModAmount") {
            auto connection = command.at("connection");
            need(connection.is_object());
            auto &cables = candidate["connections"];
            auto found = std::find_if(cables.begin(), cables.end(),
                                      [&](const Json &c) { return identity(c) == identity(connection); });
            if (type == "connect") {
                need(found == cables.end(), "duplicate", "Connection exists");
                cables.push_back(connection);
            } else {
                need(found != cables.end(), "not-found", "No such connection");
                if (type == "disconnect") cables.erase(found);
                else {
                    need(found->at("kind") == "modulation");
                    (*found)["amount"] = number(command.at("amount"), -1, 1);
                }
            }
        } else throw Refusal{"invalid", "Not available in the web demo"};
        commit(std::move(candidate));
    }
    void render(uint32_t frames) {
        float *rows[] = {left, right};
        std::fill(left, left + frames, 0.f);
        std::fill(right, right + frames, 0.f);
        if (live && app_engine_render_channels(live.get(), rows, 2, frames) != WS_OK) {
            std::fill(left, left + frames, 0.f);
            std::fill(right, right + frames, 0.f);
        }
        if (fade && old) {
            float *prior[] = {scratch_l, scratch_r};
            if (app_engine_render_channels(old.get(), prior, 2, frames) != WS_OK) {
                std::fill(scratch_l, scratch_l + frames, 0.f);
                std::fill(scratch_r, scratch_r + frames, 0.f);
            }
            for (uint32_t i = 0; i < frames; ++i) {
                float g = fade ? float(fade) / float(kFadeFrames) : 0.f;
                left[i] = left[i] * (1.f - g) + scratch_l[i] * g;
                right[i] = right[i] * (1.f - g) + scratch_r[i] * g;
                if (fade) --fade;
            }
        }
        if (!fade) old.reset();
        float p = 0;
        for (uint32_t i = 0; i < frames; ++i) {
            if (!std::isfinite(left[i])) left[i] = 0;
            if (!std::isfinite(right[i])) right[i] = 0;
            p = std::max(p, std::max(std::fabs(left[i]), std::fabs(right[i])));
        }
        peak = p;
        this->frames = frames;
    }
};
Web *web = nullptr;

Json catalogue_json(AppEngine *engine) {
    auto result = Json::array();
    const char *hints[] = {"any",  "audio",   "pitch",    "cv_bi",   "cv_uni",
                           "gate", "trigger", "ev_notes", "ev_midi", "ev_message"};
    for (uint32_t i = 0; i < app_engine_module_count(engine); ++i) {
        auto *meta = app_engine_module(engine, i);
        auto *d = app_engine_native_descriptor(engine, meta->id);
        if (!d || !web_module(d->id)) continue;
        Json ports = Json::array(), params = Json::array();
        for (bool output : {false, true}) {
            auto *p = output ? d->outputs : d->inputs;
            auto n = output ? d->output_count : d->input_count;
            for (uint32_t j = 0; j < n; ++j)
                ports.push_back({{"id", p[j].id},
                                 {"name", p[j].name},
                                 {"direction", output ? "output" : "input"},
                                 {"kind", p[j].kind == WS2_EVENT ? "event" : "signal"},
                                 {"hint", hints[std::min(p[j].hint, 9u)]},
                                 {"poly", p[j].poly == WS2_SUM     ? "sum"
                                          : p[j].poly == WS2_FIRST ? "first"
                                                                   : "poly"},
                                 {"stereo_pair", p[j].stereo_pair ? Json(p[j].stereo_pair) : Json(nullptr)}});
        }
        for (uint32_t j = 0; j < d->param_count; ++j) {
            auto &p = d->params[j];
            Json value = {{"id", p.id},
                          {"name", p.name},
                          {"minimum", p.minimum},
                          {"maximum", p.maximum},
                          {"default", p.initial},
                          {"unit", p.unit ? p.unit : ""},
                          {"taper", p.taper == WS2_LOG   ? "log"
                                    : p.taper == WS2_EXP ? "exp"
                                                         : "linear"},
                          {"step", p.step},
                          {"decimals", p.decimals},
                          {"modulatable", bool(p.flags & WS2_MODULATABLE)}};
            if (p.option_count) {
                value["choices"] = Json::array();
                for (uint32_t k = 0; k < p.option_count; ++k) value["choices"].push_back(p.options[k]);
            }
            params.push_back(value);
        }
        result.push_back(
            {{"id", d->id}, {"name", d->name}, {"category", "Native"}, {"ports", ports}, {"parameters", params}});
    }
    return {{"modules", result}};
}
} // namespace

extern "C" {
int wsw_init(double sample_rate) {
    if (web) return 1;
    if (!(sample_rate >= 8000 && sample_rate <= 192000)) return 0;
    auto *w = new Web();
    w->rate = sample_rate;
    w->config = {sizeof(ws_engine_config), sample_rate, 128, 64, 2048};
    w->catalogue.reset(app_engine_create(&w->config));
    if (!w->catalogue) {
        delete w;
        return 0;
    }
    web = w;
    return 1;
}
const char *wsw_catalogue() {
    if (!web) return "";
    web->reply = catalogue_json(web->catalogue.get()).dump();
    return web->reply.c_str();
}
const char *wsw_command(const char *json) {
    if (!web) return "{\"ok\":false,\"code\":\"unavailable\",\"message\":\"Engine not started\"}";
    Json reply;
    try {
        auto command = Json::parse(json ? json : "");
        web->apply(command);
        reply = {{"ok", true}};
    } catch (const Refusal &r) {
        reply = {{"ok", false}, {"code", r.code}, {"message", r.message}};
    } catch (const std::exception &) {
        reply = {{"ok", false}, {"code", "invalid"}, {"message", "Malformed command"}};
    }
    reply["gen"] = std::to_string(web->generation);
    web->reply = reply.dump();
    return web->reply.c_str();
}
const char *wsw_telemetry() {
    if (!web) return "{}";
    Json meters = Json::object(), scope = Json::object(), outputs = Json::object();
    if (web->live && web->patch.contains("outputs") && !web->patch["outputs"].empty()) {
        auto id = std::to_string(integer(web->patch["outputs"][0]["module"]));
        meters[id] = Json::array({web->peak, web->peak});
        // A 64-point view of the last block for the Output module's scope.
        Json points = Json::array();
        uint32_t n = std::max<uint32_t>(web->frames, 1);
        for (uint32_t i = 0; i < 64; ++i) points.push_back(web->left[std::min(n - 1, i * n / 64)]);
        scope[id] = points;
    }
    if (web->live) {
        // Last sample of each cabled output, keyed moduleId:portId, for the "carrying signal" cable look.
        auto &engine = app_engine_native(web->live.get());
        uint32_t last = web->frames ? web->frames - 1 : 0;
        for (const auto &c : web->patch["connections"]) {
            auto module = integer(c.at("from").at("module"));
            auto name = c.at("from").at("port").get<std::string>();
            auto key = std::to_string(module) + ":" + name;
            if (outputs.contains(key)) continue;
            auto *d = engine.descriptor(module);
            if (!d) continue;
            for (uint32_t i = 0; i < d->output_count; ++i)
                if (name == d->outputs[i].id && d->outputs[i].kind == WS2_SIGNAL) {
                    float v = engine.output_sample(module, i, 0, last);
                    outputs[key] = std::isfinite(v) ? std::max(-1.f, std::min(1.f, v)) : 0.f;
                }
        }
    }
    auto snapshot = web->telemetry.sample(web->live.get());
    Json result = {{"meters", meters}, {"scope", scope}, {"outputs", outputs}, {"heldNotes", Json(web->held)}};
    result.update(snapshot); // Complete render-backed maps; never reuse the previous graph.
    web->reply = result.dump();
    return web->reply.c_str();
}
float *wsw_render(uint32_t frames) {
    if (!web || frames > kMaxFrames) return nullptr;
    web->render(frames);
    return web->left;
}
float *wsw_right() { return web ? web->right : nullptr; }
float wsw_peak() { return web ? web->peak : 0.f; }
char *wsw_alloc(uint32_t bytes) { return static_cast<char *>(::operator new(bytes, std::nothrow)); }
void wsw_free(char *p) { ::operator delete(p); }
}
