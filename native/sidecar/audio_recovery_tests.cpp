// SPDX-License-Identifier: GPL-3.0-only
// Link-time fake implements the existing audio service boundary; never loads PortAudio.
#include "session.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace fake {
std::vector<wirescape::audio::Device> outputs;
std::vector<std::string> attempts;
bool active = true, refuse = false, enumeration_throws = false, render = false;
uint32_t rejected_rate = 0;
std::string rate_refused_id;
int stopped = 0, exchanges = 0;
AppEngine *engine = nullptr;
}
// Legacy-shaped fake output: the id is the bare name, as before host-API identity.
wirescape::audio::Device output(const std::string &name, int index, bool is_default = false) {
    return {name, name, "", index, is_default};
}
namespace wirescape::audio {
std::vector<Device> devices(const std::string &) {
    if (fake::enumeration_throws) throw std::runtime_error("fake enumeration failure");
    return fake::outputs;
}
struct Stream::Impl {
    EngineOwner engine{nullptr, app_engine_destroy};
    std::function<void()> producers;
    // With fake::render, a paced thread plays the device callback (block-boundary adopt).
    EngineSlot slot;
    std::atomic<bool> quit{false};
    std::thread callback;
};
Stream::Stream(EngineOwner &owner, const StreamSelection &selection, double rate, uint32_t block,
               std::function<void()> producers)
    : impl_(std::make_unique<Impl>()) {
    fake::attempts.push_back(selection.name);
    if (resolve_device(fake::outputs, selection.name) < 0 || fake::refuse)
        throw std::runtime_error("fake unavailable/ambiguous output");
    if (uint32_t(rate) == fake::rejected_rate || selection.name == fake::rate_refused_id) throw RateUnsupported();
    impl_->engine = std::move(owner);
    impl_->producers = std::move(producers);
    fake::engine = impl_->engine.get();
    fake::active = true;
    if (fake::render)
        impl_->callback = std::thread([impl = impl_.get(), live = impl_->engine.get(), block, rate]() mutable {
            std::vector<float> left(block), right(block);
            float *rows[] = {left.data(), right.data()};
            const auto period = std::chrono::duration<double>(block / rate);
            while (!impl->quit.load()) {
                live = impl->slot.adopt(live);
                app_engine_render_live(live, rows, 2, block, app_engine_clock_ns());
                std::this_thread::sleep_for(period);
            }
        });
}
Stream::~Stream() { stop(); }
EngineOwner Stream::stop() {
    impl_->quit.store(true);
    if (impl_->callback.joinable()) impl_->callback.join();
    if (impl_->engine) {
        if (impl_->producers) impl_->producers();
        ++fake::stopped;
    }
    return std::move(impl_->engine);
}
bool Stream::exchange(EngineOwner &next, std::chrono::steady_clock::time_point deadline) {
    if (!impl_->engine || !impl_->callback.joinable() || !impl_->slot.offer(next.get(), deadline)) return false;
    std::swap(impl_->engine, next);
    fake::engine = impl_->engine.get();
    ++fake::exchanges;
    return true;
}
bool Stream::running() const { return bool(impl_->engine) && fake::active; }
double Stream::cpu_percent() const { return 0; }
uint64_t Stream::dropouts() const { return 0; }
} // namespace wirescape::audio
using namespace wirescape::sidecar;
using namespace std::chrono_literals;
void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
Json audio(const Session &s) { return s.status()["status"]["audio"]; }
int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("wirescape-audio-recovery-" + std::to_string(app_engine_clock_ns()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::filesystem::remove_all(path); }
    } cleanup{root};
    try {
        std::filesystem::create_directories(root);
        Options options;
        options.audio = true; // fake linked service, never the physical backend
        options.settings = (root / "settings.json").string();
        std::ofstream(options.settings) << R"({"audioDeviceId":"Remembered","opaque":17})";
        fake::outputs = {output("Other default", 0, true)};
        const auto base = std::chrono::steady_clock::time_point{};
        {
            Session s(options);
            s.start(base);
            check(audio(s)["state"] == "reconnecting", "missing remembered output must retry");
            check(audio(s)["deviceId"].is_null(), "missing output must not be fabricated");
            check(fake::attempts == std::vector<std::string>{"Remembered"}, "must not open default");
            for (int i = 0; i < 100; ++i) s.poll(base); // no busy-spin opens
            check(fake::attempts.size() == 1, "repeated polls before deadline must not reopen");
            s.poll(base + 100ms);
            check(fake::attempts.size() == 2, "first backoff expires");
            s.poll(base + 299ms);
            check(fake::attempts.size() == 2, "failed retries increase backoff");
            fake::outputs.push_back(output("Remembered", 9, false));
            check(s.poll(base + 300ms), "recovery must notify devices");
            check(audio(s)["state"] == "running" && audio(s)["deviceId"] == "Remembered", "identity recovered");
            const auto token = s.status()["session"];
            auto *const graph = fake::engine;
            fake::enumeration_throws = true;
            s.poll(base + 310ms);
            check(audio(s)["state"] == "running" && fake::stopped == 0,
                  "enumeration exception must preserve healthy stream");
            fake::enumeration_throws = false;
            fake::outputs.push_back(output("Remembered", 10, false));
            s.poll(base + 320ms);
            check(audio(s)["state"] == "running" && fake::stopped == 0,
                  "duplicate identity must preserve healthy stream");
            fake::outputs.pop_back();
            fake::active = false;
            check(audio(s)["state"] == "device-lost", "inactive stream status cannot claim running");
            check(s.poll(base + 400ms), "loss must notify devices");
            check(audio(s)["state"] == "device-lost" && fake::stopped == 1, "loss must retire once");
            fake::outputs = {output("Other default", 0, true)};
            const auto count = fake::attempts.size();
            s.poll(base + 499ms);
            check(audio(s)["state"] == "reconnecting", "loss is followed by reconnecting");
            check(fake::attempts.size() == count, "loss retry must wait");
            s.poll(base + 500ms); // permanently missing for a while
            fake::outputs.push_back(output("Remembered", 2, false)); // changed backend index
            s.poll(base + 700ms);
            check(audio(s)["deviceId"] == "Remembered", "re-add must use identity, not stale index");
            check(fake::engine == graph && s.status()["session"] == token && s.status()["gen"] == "0",
                  "recovery must preserve graph, generation and process session");
            for (const auto &name : fake::attempts) check(name == "Remembered", "no silent fallback");
            std::ifstream saved(options.settings);
            Json settings;
            saved >> settings;
            check(settings["audioDeviceId"] == "Remembered" && settings["opaque"] == 17, "preferences preserved");
            fake::outputs = {output("Other default", 0, true)};
            check(s.poll(base + 800ms) && audio(s)["state"] == "device-lost",
                  "enumerated removal must retire even before backend activity reports loss");
            s.shutdown();
            check(fake::stopped == 2, "clean shutdown retires recovered stream");
        }
        // An available device can refuse opening; permanent failure remains alive
        // and caps retry spacing at five seconds without flooding open attempts.
        fake::refuse = true;
        fake::attempts.clear();
        {
            Session s(options);
            s.start(base);
            auto now = base + 100ms;
            for (int delay : {200, 400, 800, 1600, 3200, 5000}) {
                const auto before = fake::attempts.size();
                s.poll(now);
                check(fake::attempts.size() == before + 1, "retry deadline must attempt once");
                s.poll(now + std::chrono::milliseconds(delay - 1));
                check(fake::attempts.size() == before + 1, "bounded exponential delay");
                now += std::chrono::milliseconds(delay);
            }
            check(audio(s)["state"] == "reconnecting", "permanent refusal keeps reconnecting");
            auto reply = s.reply({{{"v", 0}, {"id", "still-alive"}, {"type", "panic"}}, {}, {}});
            check(reply["ok"] == true && reply["gen"] == "0", "commands remain usable without output");
        }
        // Fresh startup without any preference may choose the actual default once.
        fake::refuse = false;
        options.settings = (root / "fresh.json").string();
        {
            Session s(options);
            s.start();
            check(audio(s)["deviceId"] == "Other default", "initial default selection remains supported");
        }
        // The link-time fake is the sidecar's protocol-level audio seam; no OS
        // audio endpoint is enumerated or opened.
        options.settings = (root / "stream-contract.json").string();
        fake::attempts.clear();
        fake::stopped = 0;
        fake::outputs = {
            {"USB Stereo", "USB Stereo", "FakeAudio", 3, true, "FakeAudio", 2, {44100, 48000, 96000}, true, ""},
            {"Mono DAC", "Mono DAC", "FakeAudio", 4, false, "FakeAudio", 1, {}, false, "mono-output"},
            {"No Output", "No Output", "FakeAudio", 5, false, "FakeAudio", 0, {}, false, "no-output"}};
        {
            Session s(options);
            s.start();
            check(audio(s)["state"] == "running" && audio(s)["deviceId"] == "USB Stereo",
                  "fake stereo output must be playing");
            auto call = [&](const char *id, const char *method, Json payload = Json::object()) {
                Json cmd = {{"v", 0}, {"id", id}, {"type", method}};
                cmd.update(payload);
                return s.reply({cmd, {}, {}});
            };
            auto hello = s.hello();
            check(hello["audioDevices"].size() == 3 &&
                      hello["capabilities"] == Json::array({"midiLearn", "setSampleRate", "setBufferFrames"}),
                  "hello must expose the supported device catalogue and methods");
            auto listed = call("list", "listAudioDevices")["result"]["devices"];
            check(listed.size() == 3 && listed[0]["backend"] == "FakeAudio" && listed[0]["outputChannels"] == 2 &&
                      listed[0]["supported"] == true,
                  "backend/channels must be reported honestly");
            check(listed[1]["supported"] == false && listed[1]["unsupportedReason"] == "mono-output" &&
                      listed[2]["unsupportedReason"] == "no-output",
                  "mono and no-output routes must be listed but disabled");
            const auto stopped = fake::stopped;
            const auto playing = fake::engine;
            auto missing = call("missing", "selectAudioDevice", {{"deviceId", "outdated-id"}});
            auto mono = call("mono", "selectAudioDevice", {{"deviceId", "Mono DAC"}});
            fake::outputs.push_back(fake::outputs.front()); // volatile index is never a disambiguator
            s.poll();
            auto duplicate = call("duplicate", "selectAudioDevice", {{"deviceId", "USB Stereo"}});
            check(duplicate["code"] == "duplicate" && fake::stopped == stopped,
                  "duplicate IDs must be refused before stopping the old stream");
            fake::outputs.pop_back();
            s.poll();
            check(missing["ok"] == false && missing["code"] == "not-found" && mono["ok"] == false &&
                      mono["code"] == "refused" && mono["message"] == "mono-output" && fake::stopped == stopped &&
                      fake::engine == playing && audio(s)["deviceId"] == "USB Stereo",
                  "stale/mono refusal must not interrupt the active route");
            auto invalid_rate = call("bad", "setSampleRate", {{"hz", 32000}});
            auto invalid_frames = call("bad-block", "setBufferFrames", {{"frames", 33}});
            check(invalid_rate["code"] == "invalid" && invalid_frames["code"] == "invalid" && fake::stopped == stopped,
                  "out-of-set settings must not stop the stream");
            auto rate = call("rate", "setSampleRate", {{"hz", 44100}});
            check(rate["ok"] == true && rate["gen"] == "1" && audio(s)["state"] == "running" &&
                      audio(s)["sampleRate"] == 44100 && audio(s)["bufferFrames"] == 256,
                  "rate ack requires a running new stream");
            auto frames = call("frames", "setBufferFrames", {{"frames", 128}});
            check(frames["ok"] == true && frames["gen"] == "2" && audio(s)["sampleRate"] == 44100 &&
                      audio(s)["bufferFrames"] == 128,
                  "buffer restart must keep selected rate");
            std::ifstream saved(options.settings);
            Json prefs;
            saved >> prefs;
            check(prefs["audio"]["sampleRate"] == 44100 && prefs["audio"]["bufferFrames"] == 128,
                  "persist only after successful restart");
            fake::rejected_rate = 96000;
            const auto gen = s.status()["gen"];
            auto unavailable = call("fail", "setSampleRate", {{"hz", 96000}});
            check(unavailable["ok"] == false && unavailable["code"] == "refused" &&
                      unavailable["message"] == "rate-unsupported" && audio(s)["reason"] == "rate-unsupported" &&
                      s.status()["gen"] == gen && audio(s)["sampleRate"] == 44100 && audio(s)["bufferFrames"] == 128 &&
                      audio(s)["state"] == "running" && audio(s)["deviceId"] == "USB Stereo",
                  "failure must roll back to the playing route");
            fake::rejected_rate = 0;
            std::ifstream saved_after(options.settings);
            Json after;
            saved_after >> after;
            check(after["audio"] == prefs["audio"], "rejected stream config must not persist");
            fake::outputs.push_back({"Rate DAC", "Rate DAC", "FakeAudio", 6, false});
            s.poll();
            fake::rate_refused_id = "Rate DAC";
            auto route_rate = call("route-rate", "selectAudioDevice", {{"deviceId", "Rate DAC"}});
            check(route_rate["code"] == "refused" && route_rate["message"] == "rate-unsupported" &&
                      audio(s)["deviceId"] == "USB Stereo" && audio(s)["state"] == "running" &&
                      audio(s)["reason"] == "rate-unsupported",
                  "rate-refused selection restores the old route");
            const auto opened = fake::attempts.size();
            s.poll(std::chrono::steady_clock::now() + 10s);
            check(fake::attempts.size() == opened, "rate refusal must never retry the rejected route");
            fake::rate_refused_id.clear();
            fake::active = false;
            const auto lost = std::chrono::steady_clock::now() + 20s;
            s.poll(lost);
            check(audio(s)["state"] == "device-lost" && !audio(s).contains("reason"),
                  "real device loss supersedes a previous refused setting");
            s.poll(lost + 100ms);
            check(audio(s)["state"] == "running", "real device loss still reconnects the remembered route");
            s.shutdown();
        }
        // An unsupported startup rate is a stable refusal, recoverable by an explicit rate choice.
        options.settings = (root / "startup-rate.json").string();
        fake::rejected_rate = options.rate;
        fake::attempts.clear();
        {
            Session s(options);
            s.start();
            check(audio(s)["state"] == "no-device" && audio(s)["reason"] == "rate-unsupported",
                  "startup rate mismatch is not reconnecting");
            const auto opened = fake::attempts.size();
            s.poll(std::chrono::steady_clock::now() + 10s);
            check(fake::attempts.size() == opened, "startup rate refusal never spins retries");
            auto fixed = s.reply({{{"v", 0}, {"id", "fix-rate"}, {"type", "setSampleRate"}, {"hz", 44100}}, {}, {}});
            check(fixed["ok"] == true && audio(s)["state"] == "running" && !audio(s).contains("reason"),
                  "explicit supported rate recovers without changing identity");
        }
        fake::rejected_rate = 0;
        // A failure after stream start must stop the candidate before its telemetry taps die.
        fake::outputs = {output("USB Stereo", 3, true)};
        fake::render = true;
        options.settings = (root / "post-start-rollback.json").string();
        std::ofstream(options.settings) << R"({"audio":"malformed","opaque":17})";
        {
            Session s(options);
            s.start();
            Json patch = {{"format", "wirescape.native.patch"},
                          {"version", 2},
                          {"modules", Json::array({{{"id", 1}, {"module", "essentials.output"}},
                                                   {{"id", 2}, {"module", "essentials.lfo"}}})},
                          {"connections", Json::array()}};
            auto loaded = s.reply({{{"v", 0}, {"id", "tap-graph"}, {"type", "loadGraph"}, {"patch", patch}}, {}, {}});
            check(loaded["ok"] == true, "prepare a graph with live telemetry taps");
            const auto gen = s.status()["gen"];
            auto failed = s.reply({{{"v", 0}, {"id", "post-start"}, {"type", "setSampleRate"}, {"hz", 44100}}, {}, {}});
            check(failed["code"] == "unavailable" && s.status()["gen"] == gen && audio(s)["state"] == "running" &&
                      audio(s)["sampleRate"] == 48000,
                  "post-start preferences failure rolls back the live graph and configuration");
            auto edit = s.reply({{{"v", 0},
                                  {"id", "old-graph"},
                                  {"type", "setParam"},
                                  {"moduleId", 2},
                                  {"paramId", "rate"},
                                  {"value", 4}},
                                 {},
                                 {}});
            check(edit["ok"] == true, "the previous document remains editable after rollback");
            check(s.telemetry()["telemetry"]["phase"].contains("2"), "previous telemetry storage survives rollback");
        }
        fake::render = false;
        // Windows lists one endpoint per host API and repeats names inside one host API.
        check(wirescape::audio::device_ids({{"Headphones", "MME"},
                                            {"Headphones", "Windows WASAPI"},
                                            {"Headphones ()", "Windows WDM-KS"},
                                            {"Headphones ()", "Windows WDM-KS"},
                                            {"Headphones ()", "Windows WDM-KS"}}) ==
                  std::vector<std::string>{"MME|Headphones", "Windows WASAPI|Headphones",
                                           "Windows WDM-KS|Headphones ()", "Windows WDM-KS|Headphones ()#2",
                                           "Windows WDM-KS|Headphones ()#3"},
              "ids are unique per host API with stable ordinals");
        fake::outputs = {{"MME|Headphones", "Headphones", "MME", 3, true},
                         {"Windows WASAPI|Headphones", "Headphones", "Windows WASAPI", 14, false},
                         {"Windows WDM-KS|Headphones ()", "Headphones ()", "Windows WDM-KS", 27, false},
                         {"Windows WDM-KS|Headphones ()#2", "Headphones ()", "Windows WDM-KS", 30, false}};
        options.settings = (root / "legacy.json").string();
        std::ofstream(options.settings) << R"({"audioDeviceId":"Headphones"})";
        fake::attempts.clear();
        {
            Session s(options);
            s.start();
            check(audio(s)["state"] == "running" && audio(s)["deviceId"] == "MME|Headphones",
                  "an ambiguous legacy name falls back to the default instead of reconnecting forever");
            for (const char *id : {"Windows WASAPI|Headphones", "Windows WDM-KS|Headphones ()#2"}) {
                auto reply =
                    s.reply({{{"v", 0}, {"id", "pick"}, {"type", "selectAudioDevice"}, {"deviceId", id}}, {}, {}});
                check(reply["ok"] == true && audio(s)["deviceId"] == id, "every repeated name is selectable by id");
            }
            for (const auto &attempt : fake::attempts) check(attempt.find('|') != std::string::npos, "opens use ids");
        }
        std::ofstream(options.settings) << R"j({"audioDeviceId":"Headphones ()"})j";
        fake::outputs.pop_back();
        {
            Session s(options);
            s.start();
            check(audio(s)["deviceId"] == "Windows WDM-KS|Headphones ()", "a unique legacy name upgrades to its id");
            s.shutdown();
            std::ifstream saved(options.settings);
            Json settings;
            saved >> settings;
            check(settings["audioDeviceId"] == "Windows WDM-KS|Headphones ()", "the upgraded id is persisted");
        }
        // An offer nobody adopts is withdrawn: the render side keeps its engine.
        {
            wirescape::audio::EngineSlot slot;
            auto *const current = reinterpret_cast<AppEngine *>(0x10);
            auto *const next = reinterpret_cast<AppEngine *>(0x20);
            check(!slot.offer(next, std::chrono::steady_clock::now()), "an unadopted offer times out");
            check(slot.adopt(current) == current, "a withdrawn offer is never adopted later");
        }
        // loadGraph on a running device swaps in place: no reopen, no stop, every gen lands.
        fake::render = true;
        fake::attempts.clear();
        fake::stopped = fake::exchanges = 0;
        options.settings = (root / "inplace.json").string();
        {
            Session s(options);
            s.start();
            check(audio(s)["state"] == "running" && fake::attempts.size() == 1, "one open at startup");
            for (int i = 1; i <= 6; ++i) {
                Json patch = {{"format", "wirescape.native.patch"},
                              {"version", 2},
                              {"modules", Json::array({{{"id", 1}, {"module", "essentials.output"}}})},
                              {"connections", Json::array()}};
                if (i % 2) patch["modules"].push_back({{"id", 2}, {"module", "essentials.vco"}});
                auto reply = s.reply({{{"v", 0}, {"id", "load"}, {"type", "loadGraph"}, {"patch", patch}}, {}, {}});
                check(reply["ok"] == true && reply["gen"] == std::to_string(i), "every in-place loadGraph lands");
            }
            check(fake::attempts.size() == 1 && fake::stopped == 0 && fake::exchanges == 6,
                  "patch switches never reopen or stop the device");
            check(audio(s)["state"] == "running", "the stream keeps running across switches");
            s.shutdown();
        }
        fake::render = false;
        std::cout << "audio recovery fake-backend tests passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
