// SPDX-License-Identifier: GPL-3.0-only
#include "session.h"
#include "runtime_patch.h"
#include "wirescape_sidecar_categories.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
#define WIRESCAPE_SIDECAR_SANITIZED 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
#define WIRESCAPE_SIDECAR_SANITIZED 1
#endif
namespace wirescape::sidecar {
namespace {
// D5 budgets: 100 ms mute, 150 ms gap (stretched only when one block period is longer).
// Sanitizer builds run several times slower, so they get a liveness allowance instead;
// release builds (Linux, Pi, Windows) keep the D5 budget as a hard refusal.
#ifdef WIRESCAPE_SIDECAR_SANITIZED
constexpr std::chrono::milliseconds kMuteBudget{1000}, kGapBudget{1000};
#else
constexpr std::chrono::milliseconds kMuteBudget{100}, kGapBudget{150};
#endif
struct Refusal : std::runtime_error {
    std::string code;
    Refusal(std::string c, const char *m) : std::runtime_error(m), code(std::move(c)) {}
};
void need(bool condition, const char *code = "invalid", const char *message = "Invalid command") {
    if (!condition) throw Refusal(code, message);
}
uint32_t integer(const Json &v, uint32_t low = 1, uint32_t high = UINT32_MAX) {
    need(v.is_number_integer());
    auto n = v.get<int64_t>();
    need(n >= low && uint64_t(n) <= high);
    return uint32_t(n);
}
double number(const Json &v, double low, double high) {
    need(v.is_number());
    double n = v.get<double>();
    need(std::isfinite(n) && n >= low && n <= high);
    return n;
}
Json identity(const Json &c) {
    return {{"kind", c.at("kind")},
            {"from", {{"module", c.at("from").at("module")}, {"port", c.at("from").at("port")}}},
            {"to", {{"module", c.at("to").at("module")}, {"port", c.at("to").at("port")}}}};
}
Json &node(Json &patch, uint32_t id) {
    for (auto &m : patch.at("modules"))
        if (m.at("id") == id) return m;
    throw Refusal("not-found", "No such module");
}
const ws2_param *parameter(const ws2_descriptor *d, const std::string &id) {
    for (uint32_t i = 0; i < d->param_count; ++i)
        if (id == d->params[i].id) return &d->params[i];
    throw Refusal("not-found", "No such parameter");
}
const ws2_port *port(const ws2_descriptor *d, const std::string &id, bool output) {
    auto *p = output ? d->outputs : d->inputs;
    auto n = output ? d->output_count : d->input_count;
    for (uint32_t i = 0; i < n; ++i)
        if (id == p[i].id) return &p[i];
    throw Refusal("not-found", "No such port");
}
Json catalogue(AppEngine *engine) {
    auto result = Json::array();
    const char *hints[] = {"any",  "audio",   "pitch",    "cv_bi",   "cv_uni",
                           "gate", "trigger", "ev_notes", "ev_midi", "ev_message"};
    for (uint32_t i = 0; i < app_engine_module_count(engine); ++i) {
        auto *meta = app_engine_module(engine, i);
        auto *d = app_engine_native_descriptor(engine, meta->id);
        if (!d) continue;
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
            {{"id", d->id}, {"name", d->name}, {"category", wirescape_sidecar_category(d->id)}, {"ports", ports}, {"parameters", params}});
    }
    return {{"modules", result}};
}
} // namespace
Options parse_options(int argc, char **argv) {
    Options o;
    o.directory = std::filesystem::absolute(argv[0]).parent_path().string();
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--confirm-audio") o.audio = true;
        else if (arg == "--confirm-midi") o.midi = true;
        else if (arg == "--null-audio") o.null_audio = true;
        else if (arg == "--bench-summary") o.bench_summary = true;
        else if (arg == "--device" || arg == "--settings" || arg == "--rate" || arg == "--block") {
            need(++i < argc);
            std::string v = argv[i];
            need(!v.empty());
            if (arg == "--device") o.device = v;
            else if (arg == "--settings") o.settings = v;
            else {
                need(v.find_first_not_of("0123456789") == std::string::npos);
                auto n = std::stoull(v);
                need(n > 0 && n <= (arg == "--block" ? 4096 : UINT32_MAX));
                if (arg == "--block") o.block = uint32_t(n);
                else o.rate = uint32_t(n);
            }
        } else throw Refusal("invalid", "Unknown startup flag");
    }
    need(!o.bench_summary || o.null_audio, "invalid", "--bench-summary requires --null-audio");
    if (o.null_audio) o.audio = o.midi = false;
    return o;
}
Session::Session(Options options)
    : options_(std::move(options)), config_{sizeof(config_), double(options_.rate), options_.block, 64, 2048},
      catalogue_(app_engine_create(&config_), app_engine_destroy), engine_(nullptr, app_engine_destroy) {
    need(bool(catalogue_), "unavailable", "Cannot create catalogue");
    std::random_device random;
    token_ = std::to_string(random()) + "-" + std::to_string(app_engine_clock_ns());
    try {
        std::ifstream in(options_.settings, std::ios::binary);
        if (in) {
            std::string bytes(LineLimit + 1, '\0');
            in.read(bytes.data(), std::streamsize(bytes.size()));
            bytes.resize(size_t(in.gcount()));
            preferences_ = strict_json(bytes);
        }
    } catch (...) {
        preferences_ = Json::object();
    }
    auto observed = std::make_unique<ObservedMidi>(options_.null_audio, options_.midi);
    observer_ = observed.get();
    observer_->endpoints();
    midi_ = std::make_unique<midi::Host>(options_.midi || options_.null_audio, std::move(observed));
    if ((options_.midi || options_.null_audio) && preferences_.contains("devices")) midi_->restore(preferences_.dump());
    patch_ = {{"format", "wirescape.native.patch"},
              {"version", 2},
              {"modules", Json::array({{{"id", 1}, {"module", "essentials.output"}}})},
              {"connections", Json::array()}};
    patch_ = execution(patch_);
    std::string error;
    engine_.reset(document::load_runtime_patch(patch_.dump(), catalogue_.get(), config_, error));
    need(bool(engine_), "unavailable", "Cannot create startup graph");
    active_ = engine_.get();
    refresh_devices();
    device_ = options_.device.empty() ? preferences_.value("audioDeviceId", std::string{}) : options_.device;
    if (options_.audio) audio_state_ = "starting";
}
void Session::start(std::chrono::steady_clock::time_point now) {
    try {
        start_audio(now);
        start_midi();
        if (stream_) persist();
    } catch (...) {
        stop_audio();
        throw;
    }
}
Session::~Session() {
    try {
        shutdown();
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
}
void Session::shutdown() {
    midi_->stop();
    stop_audio();
    if (options_.bench_summary && !bench_reported_) {
        bench_reported_ = true;
        std::cerr << "bench blocks=" << bench_.blocks << " p50=" << bench_.percentile(50)
                  << " p99=" << bench_.percentile(99) << " max=" << bench_.max_percent
                  << " overruns=" << bench_.overruns << '\n';
    }
}
void Session::start_audio(std::chrono::steady_clock::time_point now) {
    stop_sink_.store(false);
    if (options_.audio) {
        try {
            // Resolve the default only before any identity has been selected.
            // A remembered/missing output must never be replaced by the default.
            if (device_.empty())
                for (const auto &d : audio::devices(options_.directory))
                    if (d.is_default) device_ = d.name;
            need(!device_.empty(), "unavailable", "No selected audio output");
            stream_ = std::make_unique<audio::Stream>(engine_, audio::StreamSelection{options_.directory, device_},
                                                      options_.rate, options_.block, [this] { midi_->stop(); });
            audio_state_ = "running";
            audio_backoff_ = std::chrono::milliseconds(100);
        } catch (const audio::ShutdownFailure &) {
            throw;
        } catch (const std::exception &) {
            audio_state_ = device_.empty() ? "no-device" : "reconnecting";
            audio_retry_ = now + audio_backoff_;
            audio_backoff_ = std::min(audio_backoff_ * 2, std::chrono::milliseconds(5000));
            if (!engine_) throw;
        }
    } else {
        active_ = engine_.get();
        sink_ = std::thread(
            [this, left = std::vector<float>(options_.block), right = std::vector<float>(options_.block)]() mutable {
                float *rows[] = {left.data(), right.data()};
                auto next = std::chrono::steady_clock::now();
                sink_rendering_.store(true, std::memory_order_release);
                while (!stop_sink_.load()) {
                    auto before = std::chrono::steady_clock::now();
                    if (app_engine_render_live(active_, rows, 2, options_.block, app_engine_clock_ns()) != WS_OK) break;
                    auto after = std::chrono::steady_clock::now();
                    double percent = std::chrono::duration<double>(after - before).count() *
                                     options_.rate / options_.block * 100;
                    cpu_.store(percent);
                    next += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                        std::chrono::duration<double>(double(options_.block) / options_.rate));
                    if (options_.bench_summary) bench_.record(percent, after > next);
                    std::unique_lock<std::mutex> lock(sink_mutex_);
                    sink_wake_.wait_until(lock, next, [this] { return stop_sink_.load(); });
                }
                sink_rendering_.store(false, std::memory_order_release);
            });
    }
}
void Session::stop_audio() {
    stop_sink_.store(true);
    sink_wake_.notify_all();
    if (sink_.joinable()) sink_.join();
    sink_rendering_.store(false, std::memory_order_release);
    if (stream_) {
        dropouts_.fetch_add(stream_->dropouts());
        engine_ = stream_->stop();
        stream_.reset();
    }
    cpu_.store(0);
}
void Session::start_midi() {
    std::string error;
    need(midi_->start(active_, patch_.dump(), error), "unavailable", "Cannot start MIDI subscriptions");
}
bool Session::refresh_devices() {
    bool enumerated = true;
    try {
        audio_devices_ = audio_devices(options_.directory, options_.null_audio);
    } catch (...) {
        enumerated = false; // Keep the last successful list; failure is not disappearance.
    }
    midi_devices_ = midi_devices({observer_->observed(), midi_->enabled()}, options_.midi || options_.null_audio);
    return enumerated;
}
void Session::persist() {
    auto preferences = preferences_;
    preferences["audioDeviceId"] = device_;
    preferences["devices"] = Json::array();
    for (const auto &e : midi_->enabled())
        preferences["devices"].push_back(
            {{"kind", "midi"}, {"name", e.name}, {"system_name", e.system_name}, {"enabled", true}});
    auto path = std::filesystem::path(options_.settings), temp = path;
    temp += ".tmp";
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << preferences.dump();
        out.close();
        need(bool(out), "unavailable", "Cannot save device preferences");
    }
#ifdef _WIN32
    need(
        MoveFileExW(temp.wstring().c_str(), path.wstring().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
        "unavailable", "Cannot save device preferences");
#else
    std::filesystem::rename(temp, path);
#endif
    preferences_ = std::move(preferences);
}
Json Session::execution(Json patch) {
    need(patch.is_object() && patch.at("format") == "wirescape.native.patch" &&
         patch.at("version").is_number_integer() && patch.at("version") == 2);
    need(patch.at("modules").is_array() && patch.at("connections").is_array());
    need(patch["modules"].size() <= 64 && patch["connections"].size() <= 128, "limit", "Graph capacity exceeded");
    if (!patch.contains("settings")) patch["settings"] = Json::object();
    std::set<uint32_t> ids;
    auto descriptor = [&](uint32_t id) {
        auto &m = node(patch, id);
        auto *d = app_engine_native_descriptor(catalogue_.get(), m.at("module").get<std::string>().c_str());
        need(d, "unknown-module", "Module not installed");
        return d;
    };
    uint32_t output = UINT32_MAX;
    for (auto &m : patch["modules"]) {
        auto id = integer(m.at("id"));
        need(ids.insert(id).second, "duplicate", "Duplicate module ID");
        auto *d = descriptor(id);
        need(!m.value("inactive", false), "unavailable", "Module is inactive");
        if (!m.contains("version")) m["version"] = d->version;
        if (std::string(d->id) == "essentials.output") output = std::min(output, id);
        auto values = m.value("parameters", Json::object());
        need(values.is_object());
        for (auto it = values.begin(); it != values.end(); ++it) {
            auto *p = parameter(d, it.key());
            need(it.value().is_number());
            double value = it.value().get<double>();
            need(std::isfinite(value) && value >= p->minimum && value <= p->maximum, "refused",
                 "Parameter outside range");
        }
    }
    std::set<std::string> edges;
    for (const auto &c : patch["connections"]) {
        need(c.is_object());
        auto from = integer(c.at("from").at("module")), to = integer(c.at("to").at("module"));
        auto *fd = descriptor(from);
        auto *td = descriptor(to);
        auto *f = port(fd, c.at("from").at("port").get<std::string>(), true);
        auto kind = c.at("kind").get<std::string>();
        need(edges.insert(identity(c).dump()).second, "duplicate", "Duplicate connection");
        if (kind == "modulation") {
            auto *p = parameter(td, c.at("to").at("port").get<std::string>());
            need(f->kind == WS2_SIGNAL, "kind-mismatch", "Modulation requires a signal source");
            need(p->flags & WS2_MODULATABLE, "refused", "Parameter cannot be modulated");
            number(c.at("amount"), -1, 1);
        } else {
            need(kind == "event" || kind == "signal");
            auto *t = port(td, c.at("to").at("port").get<std::string>(), false);
            auto expected = kind == "event" ? WS2_EVENT : WS2_SIGNAL;
            need(f->kind == expected && t->kind == expected, "kind-mismatch", "Cable requires matching port kinds");
        }
    }
    if (!patch.contains("outputs")) {
        need(output != UINT32_MAX, "refused", "Patch needs an Output module");
        patch["outputs"] = Json::array({{{"module", output}, {"port", "audio"}, {"bus_channel", 0}},
                                        {{"module", output}, {"port", "audio"}, {"bus_channel", 1}}});
    }
    need(patch["outputs"].is_array() && !patch["outputs"].empty(), "refused", "Invalid output routes");
    for (const auto &o : patch["outputs"]) {
        auto *d = descriptor(integer(o.at("module")));
        auto *p = port(d, o.at("port").get<std::string>(), true);
        need(p->kind == WS2_SIGNAL, "kind-mismatch", "Output requires a signal port");
        integer(o.at("bus_channel"), 0, 15);
    }
    need(patch.dump().size() <= LineLimit, "limit", "Patch exceeds byte limit");
    return patch;
}
void Session::commit(Json patch, bool new_namespace) {
    patch = execution(std::move(patch));
    std::string error;
    const auto patch_bytes = patch.dump();
    audio::EngineOwner candidate(document::load_runtime_patch(patch_bytes, catalogue_.get(), config_, error),
                                 app_engine_destroy);
    need(bool(candidate), "refused", "Graph configuration or state refused");
    // Stage all potentially throwing document/ID work before accepting a live graph.
    std::set<uint32_t> ids = new_namespace ? std::set<uint32_t>{} : used_ids_;
    for (const auto &m : patch["modules"]) ids.insert(integer(m["id"]));
    const bool rendering = (stream_ && stream_->running()) || sink_rendering_.load(std::memory_order_acquire);
    if (!new_namespace && rendering) {
        const uint64_t timeout_ns = 3ULL * options_.block * 1000000000ULL / options_.rate;
        // A busy MIDI dispatch is transient; a graph refusal is not. Never stop for busy.
        for (unsigned attempt = 0; attempt < 4; ++attempt) {
            if (midi_->replan(candidate.get(), patch_bytes, timeout_ns, error)) {
                patch_.swap(patch);
                used_ids_.swap(ids);
                ++generation_;
                std::cerr << "commit path=live gen=" << generation_ << '\n';
                return;
            }
            if (error != "MIDI worker busy") break;
            if (attempt == 3) throw Refusal("busy", "MIDI worker busy; graph unchanged");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    // D5 directly for loadGraph, stopped/lost streams and non-busy replan refusal.
    // The candidate was fully prepared above, so validation refusal is untouched.
    const auto begun = std::chrono::steady_clock::now();
    // D5 budgets (see kMuteBudget/kGapBudget); they stretch only when a block period is longer.
    const auto period = std::chrono::microseconds(int64_t(1e6 * double(options_.block) / double(options_.rate)));
    if ((stream_ && stream_->running()) || sink_rendering_.load(std::memory_order_acquire)) {
        app_engine_begin_master_fade_out(active_);
        const auto deadline = begun + std::max<std::chrono::microseconds>(kMuteBudget, 2 * period);
        while (!app_engine_master_fade_silent(active_)) {
            // D6 may lose the stream during a refused replan or the D5 fade.
            // A callback that no longer renders cannot acknowledge a mute.
            if ((stream_ && !stream_->running()) || (!stream_ && !sink_rendering_.load(std::memory_order_acquire)))
                break;
            if (std::chrono::steady_clock::now() >= deadline) {
                app_engine_resume_master_fade(active_);
                throw Refusal("unavailable", "Audio mute did not complete");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    app_engine_prepare_master_fade_in(candidate.get());
    try {
        midi_->stop();
        app_engine_panic(active_);
        stop_audio();
    } catch (...) {
        app_engine_resume_master_fade(active_);
        throw;
    }
    auto previous = std::move(engine_);
    auto old_patch = patch_;
    engine_ = std::move(candidate);
    active_ = engine_.get();
    patch_.swap(patch); // No allocation after the quiescent graph replacement.
    try {
        start_audio();
        start_midi();
        if (stream_ || sink_.joinable()) {
            const auto deadline = begun + std::max<std::chrono::microseconds>(kGapBudget, 3 * period);
            while (!app_engine_master_fade_in_started(active_)) {
                if (std::chrono::steady_clock::now() >= deadline)
                    throw Refusal("unavailable", "Audio swap exceeded gap budget");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    } catch (...) {
        midi_->stop();
        stop_audio();
        engine_ = std::move(previous);
        active_ = engine_.get();
        patch_ = old_patch;
        app_engine_prepare_master_fade_in(active_);
        start_audio();
        start_midi();
        throw;
    }
    ++generation_;
    keyboard_.fill(false);
    used_ids_.swap(ids);
    std::cerr << "commit path=d5 gen=" << generation_ << '\n';
}
Json Session::apply(const Json &command) {
    auto type = command.at("type").get<std::string>();
    if (type == "listAudioDevices") return {{"devices", audio_devices_}};
    if (type == "listMidiDevices") return {{"devices", midi_devices_}};
    if (type == "setOsc" || type == "setSampleRate" || type == "setBufferFrames")
        throw Refusal("unavailable", "Capability not available");
    if (type == "selectAudioDevice") {
        need(command.at("deviceId").is_string());
        auto id = command.at("deviceId").get<std::string>();
        auto count = std::count_if(audio_devices_.begin(), audio_devices_.end(),
                                   [&](const Json &d) { return d.at("id") == id; });
        need(count > 0, "not-found", "No such audio output");
        need(count == 1, "duplicate", "Ambiguous audio output name");
        need(options_.audio || options_.null_audio, "unavailable", "Audio device access is not authorized");
        auto old_device = device_;
        midi_->stop();
        stop_audio();
        device_ = id;
        try {
            start_audio();
            need(!options_.audio || bool(stream_), "unavailable", "Cannot open audio output");
            start_midi();
            persist();
        } catch (...) {
            midi_->stop();
            stop_audio();
            device_ = old_device;
            start_audio();
            start_midi();
            throw;
        }
        refresh_devices();
        return nullptr;
    }
    if (type == "setMidiEnabled") {
        need(command.at("deviceId").is_string() && command.at("on").is_boolean());
        auto id = command.at("deviceId").get<std::string>();
        auto found =
            std::find_if(midi_devices_.begin(), midi_devices_.end(), [&](const Json &d) { return d.at("id") == id; });
        need(found != midi_devices_.end(), "not-found", "No such MIDI input");
        need(options_.midi || options_.null_audio, "unavailable", "MIDI device access is not authorized");
        auto old = midi_->enabled();
        midi_->stop();
        midi_->select({found->at("name").get<std::string>(), found->at("system_name").get<std::string>()},
                      command.at("on").get<bool>());
        try {
            need(midi_->enabled().size() <= 16, "limit", "Too many MIDI inputs");
            start_midi();
            persist();
        } catch (...) {
            midi_->stop();
            for (auto &e : midi_->enabled()) (void)e;
            auto selected = midi_->enabled();
            for (auto &e : selected) midi_->select(e, false);
            for (auto &e : old) midi_->select(e, true);
            start_midi();
            throw;
        }
        refresh_devices();
        return nullptr;
    }
    if (type == "panic") {
        app_engine_panic(active_);
        keyboard_.fill(false);
        observer_->clear_notes();
        return nullptr;
    }
    if (type == "noteOn" || type == "noteOff") {
        auto key = integer(command.at("note"), 0, 127);
        float velocity = type == "noteOn" ? float(number(command.at("velocity"), 0, 1)) : 0;
        ws2_event event{};
        event.struct_size = sizeof(event);
        event.type = type == "noteOn" ? WS2_NOTE_ON : WS2_NOTE_OFF;
        event.note_id = -1;
        event.key = int32_t(key);
        event.value = velocity;
        for (const auto &m : patch_["modules"])
            if (m.at("module") == "host.keyboard") {
                auto result = app_engine_native(active_).enqueue(0, integer(m.at("id")), WS2_DIRECT_PORT, event, 0,
                                                                 engine2::Timing::Immediate);
                if (result != WS2_OK) {
                    if (type == "noteOff") app_engine_panic(active_);
                    throw Refusal("busy", "Event queue full");
                }
            }
        keyboard_[key] = type == "noteOn";
        return nullptr;
    }
    if (type == "setParam") {
        auto id = integer(command.at("moduleId"));
        auto &m = node(patch_, id);
        auto *d = app_engine_native_descriptor(catalogue_.get(), m.at("module").get<std::string>().c_str());
        auto param = command.at("paramId").get<std::string>();
        auto *p = parameter(d, param);
        need(command.at("value").is_number());
        double plain = command.at("value").get<double>();
        need(std::isfinite(plain));
        need(plain >= p->minimum && plain <= p->maximum, "refused", "Parameter outside range");
        float value = float(plain);
        ws2_event native{};
        native.struct_size = sizeof(native);
        native.type = WS2_PARAM;
        native.index = uint32_t(p - d->params);
        native.value = value;
        auto result = app_engine_native(active_).enqueue(0, id, WS2_DIRECT_PORT, native, 0, engine2::Timing::Immediate);
        need(result == WS2_OK, "busy", "Event queue full");
        m["parameters"][param] = value;
        return nullptr;
    }
    Json candidate = patch_;
    if (type == "loadGraph") {
        commit(command.at("patch"), true);
        return nullptr;
    }
    if (type == "addModule") {
        auto id = integer(command.at("moduleId"));
        need(!used_ids_.count(id), "duplicate", "Module ID was already used");
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
        auto &outputs = candidate["outputs"];
        outputs.erase(std::remove_if(outputs.begin(), outputs.end(), [&](const Json &o) { return o["module"] == id; }),
                      outputs.end());
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
    } else if (type == "ccMap.set") {
        auto &m = node(candidate, integer(command.at("moduleId")));
        need(m.at("module") == "midi.cc_to_cv", "not-found", "No such CC mapping module");
        auto row = integer(command.at("row"), 0, UINT32_MAX);
        need(row < 8, "limit", "CC row capacity exceeded");
        auto &cfg = m["config"];
        if (cfg.is_null()) cfg = Json::object();
        auto &rows = cfg["rows"];
        if (rows.is_null()) rows = Json::array();
        need(rows.is_array() && row <= rows.size(), "limit", "CC rows cannot have holes");
        auto mapping = command.at("mapping");
        need(mapping.is_object());
        integer(mapping.at("channel"), 0, 16);
        integer(mapping.at("controller"), 0, 127);
        for (const char *key : {"fourteen_bit", "bipolar"})
            if (mapping.contains(key)) need(mapping[key].is_boolean());
        if (mapping.contains("smoothing_ms")) number(mapping["smoothing_ms"], 0, 2000);
        if (row == rows.size()) rows.push_back(mapping);
        else rows[row] = mapping;
    } else throw Refusal("invalid", "Unknown command type");
    commit(std::move(candidate), false);
    return nullptr;
}
Json Session::base() const { return {{"v", 0}, {"session", token_}, {"gen", std::to_string(generation_)}}; }
Json Session::reply(const Request &request) {
    auto result = base();
    result["re"] = nullptr;
    result["ok"] = false;
    try {
        if (!request.value.is_null() && request.value.contains("id") && request.value["id"].is_string()) {
            auto id = request.value["id"].get<std::string>();
            bool valid = !id.empty() && id.size() <= 64 &&
                         std::all_of(id.begin(), id.end(), [](unsigned char c) { return c >= 32 && c <= 126; });
            if (valid) result["re"] = id;
        }
        if (!request.error.empty()) throw Refusal(request.error, "Input refused");
        need(!result["re"].is_null());
        need(request.value.at("v").is_number_integer() && request.value.at("v") == 0 &&
             request.value.at("type").is_string());
        auto data = apply(request.value);
        result["ok"] = true;
        if (!data.is_null()) result["result"] = std::move(data);
    } catch (const audio::ShutdownFailure &e) {
        std::cerr << e.what() << '\n';
        std::cerr.flush();
        std::_Exit(2);
    } catch (const Refusal &e) {
        result["code"] = e.code;
        result["message"] = e.what();
    } catch (const Json::exception &) {
        result["code"] = "invalid";
        result["message"] = "Invalid command field";
    } catch (const std::exception &) {
        result["code"] = "unavailable";
        result["message"] = "Engine operation unavailable";
    }
    result["gen"] = std::to_string(generation_);
    return result;
}
Json Session::audio_status() const {
    auto selected = stream_ && stream_->running();
    Json result = {{"state", options_.audio && stream_ && !selected ? "device-lost" : audio_state_},
                   {"deviceId", selected ? Json(device_) : Json(nullptr)},
                   {"deviceName", selected ? Json(device_) : Json(nullptr)},
                   {"sampleRate", options_.rate},
                   {"bufferFrames", options_.block},
                   {"latencyMs", 2. * options_.block / options_.rate * 1000},
                   {"cpuPercent", selected ? stream_->cpu_percent() : 0},
                   {"dropouts", dropouts_.load() + (stream_ ? stream_->dropouts() : 0)}};
    return result;
}
Json Session::midi_status() const { return {{"devices", midi_devices_}, {"activity", observer_->activity()}}; }
Json Session::hello() const {
    return {{"v", 0},
            {"session", token_},
            {"type", "hello"},
            {"protocol", 0},
            {"engine", {{"version", "0.1.0"}, {"sha", WIRESCAPE_ENGINE_SHA}}},
            {"capabilities", Json::array()},
            {"catalogue", catalogue(catalogue_.get())},
            {"audio", audio_status()},
            {"midi", midi_status()}};
}
Json Session::status() const {
    auto value = base();
    value["type"] = "status";
    value["status"] = {{"audio", audio_status()}, {"midi", midi_status()}};
    return value;
}
Json Session::telemetry() {
    auto held = observer_->held();
    for (size_t key = 0; key < 128; ++key)
        if (keyboard_[key] && std::find(held.begin(), held.end(), Json(key)) == held.end()) held.push_back(key);
    auto meters = Json::object();
    const auto &routes = patch_["outputs"];
    if (routes.size() == 2 && routes[0]["module"] == routes[1]["module"] && routes[0]["port"] == "audio" &&
        routes[1]["port"] == "audio" &&
        ((routes[0]["bus_channel"] == 0 && routes[1]["bus_channel"] == 1) ||
         (routes[0]["bus_channel"] == 1 && routes[1]["bus_channel"] == 0))) {
        auto id = integer(routes[0]["module"]);
        if (node(patch_, id)["module"] == "essentials.output") {
            ws_meter meter{};
            app_engine_get_meter(active_, &meter);
            if (meter.rendered_blocks) meters[std::to_string(id)] = Json::array({meter.peak, meter.peak});
        }
    }
    Json telemetry = {{"tick", ++tick_},
                      {"time", std::chrono::duration<double>(std::chrono::steady_clock::now() - started_).count()},
                      {"meters", meters},
                      {"heldNotes", held}};
    for (const char *key : {"effective", "phase", "envelope", "scope", "eventActivity"})
        telemetry[key] = Json::object();
    auto value = base();
    value["type"] = "telemetry";
    value["telemetry"] = telemetry;
    return value;
}
Json Session::devices() {
    auto value = base();
    value["type"] = "devices";
    value["audioDevices"] = audio_devices_;
    value["midiDevices"] = midi_devices_;
    return value;
}
bool Session::poll_audio(std::chrono::steady_clock::time_point now, bool enumerated) {
    if (!options_.audio) return false;
    auto previous = audio_state_;
    const auto matches = std::count_if(audio_devices_.begin(), audio_devices_.end(),
                                       [&](const Json &d) { return d.at("id") == device_; });
    if (stream_ && (!stream_->running() || (enumerated && matches == 0))) {
        midi_->stop();
        stop_audio();
        audio_state_ = "device-lost";
        audio_backoff_ = std::chrono::milliseconds(100);
        audio_retry_ = now + audio_backoff_;
    } else if (!stream_) {
        if (audio_state_ == "device-lost") audio_state_ = "reconnecting";
        if (now < audio_retry_) return previous != audio_state_;
        start_audio(now);
        if (stream_) start_midi();
    }
    return previous != audio_state_;
}
bool Session::poll(std::chrono::steady_clock::time_point now) {
    auto old_audio = audio_devices_, old_midi = midi_devices_;
    if (midi_->enabled().empty()) observer_->endpoints();
    const auto enumerated = refresh_devices();
    auto changed = poll_audio(now, enumerated);
    return changed || old_audio != audio_devices_ || old_midi != midi_devices_;
}
} // namespace wirescape::sidecar
