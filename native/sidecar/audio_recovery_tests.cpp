// SPDX-License-Identifier: GPL-3.0-only
// Link-time fake implements the existing audio service boundary; never loads PortAudio.
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "session.h"
#include "../io/osc/codec.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace fake {
std::vector<wirescape::audio::Device> outputs;
std::vector<std::string> attempts;
std::string refuse_id; // fake a single device-specific PortAudio open failure
std::string open_error; // injected malformed/multibyte host diagnostic
std::vector<std::string> installed_asio;
bool active = true, refuse = false, enumeration_throws = false, render = false;
uint32_t rejected_rate = 0;
// Fake ASIO buffer negotiation: when accept_block is set, any other block fails to
// open; a first attempt then reports `preferred` (0 = driver offers no size).
uint32_t accept_block = 0;
int64_t preferred = 0;
std::vector<uint32_t> blocks;
std::string rate_refused_id;
int stopped = 0, exchanges = 0;
std::vector<wirescape::audio::DeviceQuery> queries;
std::atomic<uint64_t> completed_blocks{0};
AppEngine *engine = nullptr;
}
// Legacy-shaped fake output: the id is the bare name, as before host-API identity.
wirescape::audio::Device output(const std::string &name, int index, bool is_default = false) {
    return {name, name, "", index, is_default};
}
namespace wirescape::audio {
std::vector<std::string> installed_asio_drivers() { return fake::installed_asio; }
std::vector<Device> devices(const std::string &, const DeviceQuery &query) {
    fake::queries.push_back(query);
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
    uint32_t frames = 0;
};
Stream::Stream(EngineOwner &owner, const StreamSelection &selection, double rate, uint32_t block,
               std::function<void()> producers, bool allow_preferred_retry)
    : impl_(std::make_unique<Impl>()) {
    fake::attempts.push_back(selection.name);
    fake::blocks.push_back(block);
    if (fake::accept_block && block != fake::accept_block) {
        if (allow_preferred_retry && fake::preferred) throw PreferredBufferSize(fake::preferred);
        throw std::runtime_error("fake ASIO buffer refused");
    }
    impl_->frames = block;
    if (resolve_device(fake::outputs, selection.name) < 0 || fake::refuse || selection.name == fake::refuse_id)
        throw std::runtime_error(fake::open_error.empty() ? "fake unavailable/ambiguous output" : fake::open_error);
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
                fake::completed_blocks.fetch_add(1, std::memory_order_release);
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
uint32_t Stream::buffer_frames() const noexcept { return impl_->frames; }
} // namespace wirescape::audio
using namespace wirescape::sidecar;
using namespace std::chrono_literals;
void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
Json audio(const Session &s) { return s.status()["status"]["audio"]; }
// Only loopback UDP: no real audio, MIDI, device enumeration, or LAN binding.
void send_local_osc(uint16_t port) {
    wirescape::osc::Message m;
    m.address = "/reconnect";
    m.count = 1;
    m.arguments[0].type = 'f';
    m.arguments[0].real = 0.5f;
    uint8_t packet[256]{};
    bool truncated = false;
    const auto size = wirescape::osc::encode(m, packet, sizeof(packet), truncated);
    check(size > 0 && !truncated, "encode bounded OSC loopback message");
    const auto sender = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    check(sender != -1, "create UDP loopback sender");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const auto sent = ::sendto(sender, reinterpret_cast<const char *>(packet), int(size), 0,
                               reinterpret_cast<sockaddr *>(&address), sizeof(address));
#ifdef _WIN32
    closesocket(sender);
#else
    close(sender);
#endif
    check(sent == int(size), "send OSC packet to localhost only");
}
int main() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("wirescape-audio-recovery-" + std::to_string(app_engine_clock_ns()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::filesystem::remove_all(path); }
    } cleanup{root};
    try {
        std::filesystem::create_directories(root);
        using wirescape::audio::classify_rates;
        const auto supported = classify_rates({0, -1, 0}, -1);
        const auto invalid = classify_rates({-1, -1, -1}, -1);
        const auto busy = classify_rates({-2, -2, -2}, -1);
        const auto mixed = classify_rates({0, -1, -2}, -1);
        check(supported.known && supported.rates == std::vector<uint32_t>{44100, 96000},
              "successful/invalid-rate results produce the exact supported set");
        check(invalid.known && invalid.rates.empty(), "only invalid-rate results prove no supported rate");
        check(!busy.known && busy.rates.empty() && !mixed.known && mixed.rates.empty(),
              "busy/unknown errors omit the set, including mixed success and unknown results");
        // The UI's driver picker receives a real host-API string through the
        // sidecar protocol, exercised here with link-time fakes (no devices opened).
        const auto opens_before_list = fake::attempts.size();
        fake::outputs = {{"ASIO|ASIO4ALL v2", "ASIO4ALL v2", "ASIO", 7, false, "ASIO", 2, {48000}, true, ""}};
        const auto asio_list = audio_devices("", false);
        check(asio_list.size() == 1 && asio_list[0]["hostApi"] == "ASIO" && asio_list[0]["backend"] == "ASIO" &&
                  asio_list[0]["id"] == "ASIO|ASIO4ALL v2",
              "ASIO fake host API is published with its stable device ID");
        check(fake::attempts.size() == opens_before_list, "listAudioDevices never opens a fake or real stream");

        const auto missing_asio = wirescape::audio::unavailable_asio_drivers(
            {"ASIO4ALL v2", "FlexASIO"}, {{"ASIO4ALL v2", "ASIO"}, {"Headphones", "Windows WASAPI"}});
        check(missing_asio == std::vector<std::string>{"FlexASIO"},
              "registry drivers missing from PortAudio are distinct, never selectable devices");
        std::vector<std::string> many_asio;
        many_asio.reserve(30);
        for (int i = 0; i < 30; ++i) many_asio.push_back("Unseen ASIO " + std::to_string(i));
        check(wirescape::audio::unavailable_asio_drivers(many_asio, {}).size() == 16,
              "registry diagnostics are limited to sixteen entries");
        const std::string long_asio = std::string(63, 'a') + "\xc3\xa9" + "driver";
        const auto clipped = wirescape::audio::unavailable_asio_drivers({long_asio, long_asio}, {});
        check(clipped.size() == 1 && clipped.front() == std::string(63, 'a'),
              "ASIO names truncate at UTF-8 boundaries and are deduplicated");
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
            auto call = [&](const char *id, const char *method, const Json &payload = Json::object()) {
                Json cmd = {{"v", 0}, {"id", id}, {"type", method}};
                cmd.update(payload);
                return s.reply({cmd, {}, {}});
            };
            auto hello = s.hello();
            const auto caps = Json::array(
                {"midiLearn", "telemetry.playhead", "telemetry.monitor", "setSampleRate", "setBufferFrames", "osc"});
            check(hello["audioDevices"].size() == 2 && hello["capabilities"] == caps,
                  "hello must expose the supported device catalogue and methods");
            auto listed = call("list", "listAudioDevices")["result"]["devices"];
            check(listed.size() == 2 && listed[0]["backend"] == "FakeAudio" && listed[0]["outputChannels"] == 2 &&
                      listed[0]["supported"] == true,
                  "backend/channels must be reported honestly");
            check(listed[1]["supported"] == false && listed[1]["unsupportedReason"] == "mono-output" &&
                      std::none_of(listed.begin(), listed.end(), [](const Json &d) { return d["id"] == "No Output"; }),
                  "mono outputs stay disabled and input-only endpoints are filtered");
            fake::queries.clear();
            const auto known_rates = listed[0]["sampleRates"];
            fake::outputs[0].sample_rates.clear();
            fake::outputs[0].supported = false;
            fake::outputs[0].unsupported_reason = "rate-unsupported"; // busy warm enumeration is not evidence
            for (int tick = 0; tick < 8; ++tick) {
                s.poll();
                const auto entry = s.devices()["audioDevices"][0];
                check(entry["supported"] == true && entry["sampleRates"] == known_rates,
                      "repeated warm polls preserve active support and known rates");
                check(!fake::queries.back().probe_rates && fake::queries.back().active_id == "USB Stereo",
                      "warm polls never request format probes and identify the held endpoint");
            }
            const auto active_list = call("held-list", "listAudioDevices")["result"]["devices"];
            check(fake::queries.back().probe_rates && fake::queries.back().active_id == "USB Stereo" &&
                      active_list[0]["supported"] == true && active_list[0]["sampleRates"] == known_rates,
                  "explicit enumeration probes only endpoints not held by the current stream");
            fake::outputs[0].sample_rates = {44100, 48000, 96000};
            fake::outputs[0].supported = true;
            fake::outputs[0].unsupported_reason.clear();
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
            // A failing candidate must report its real open error in the reply,
            // even though the successful rollback clears status.audio.error.
            const auto before_open_error = fake::attempts.size();
            fake::refuse_id = "Rate DAC";
            auto attempted = call("route-open-error", "selectAudioDevice", {{"deviceId", "Rate DAC"}});
            check(attempted["ok"] == false && attempted["code"] == "unavailable" &&
                      attempted["message"] == "fake unavailable/ambiguous output" &&
                      fake::attempts.size() == before_open_error + 2 && audio(s)["deviceId"] == "USB Stereo" &&
                      audio(s)["state"] == "running" && !audio(s).contains("error"),
                  "failed target open preserves its error and restores the previous playing output");
            // Both a split UTF-8 cap and arbitrary host-codepage bytes must
            // serialize in strict JSON on refusal; rollback keeps audio running.
            for (const auto &diagnostic :
                 {std::string(199, 'x') + "\xc3\xa9", std::string("bad ") + "\xff\xc0\xaf" + " device"}) {
                fake::refuse_id = "Rate DAC";
                fake::open_error = diagnostic;
                auto error = call("utf8-open-error", "selectAudioDevice", {{"deviceId", "Rate DAC"}});
                const auto message = error["message"].get<std::string>();
                check(error["code"] == "unavailable" && message.size() <= 200 && !message.empty() &&
                          audio(s)["deviceId"] == "USB Stereo" && error.dump().find("unavailable") != std::string::npos,
                      "failed ASIO open returns valid, bounded UTF-8 and preserves old playing stream");
                check(Json::parse(error.dump())["message"] == message,
                      "host errors must survive strict protocol JSON serialization");
            }
            fake::open_error.clear();
            fake::refuse_id.clear();
            fake::active = false;
            const auto lost = std::chrono::steady_clock::now() + 20s;
            s.poll(lost);
            check(audio(s)["state"] == "device-lost" && !audio(s).contains("reason"),
                  "real device loss supersedes a previous refused setting");
            s.poll(lost + 100ms);
            check(audio(s)["state"] == "running", "real device loss still reconnects the remembered route");
            s.shutdown();
        }
        options.settings = (root / "unknown-rates.json").string();
        fake::outputs = {output("Unknown stereo", 7, true)};
        fake::outputs[0].sample_rates.clear();
        {
            Session s(options);
            s.start();
            for (int tick = 0; tick < 3; ++tick) s.poll();
            const auto entry = s.devices()["audioDevices"][0];
            check(entry["supported"] == true && !entry.contains("sampleRates") && audio(s)["sampleRate"] == 48000,
                  "unknown supported rates stay omitted while status reports the proven current rate");
            auto changed = s.reply({{{"v", 0}, {"id", "unknown-rate"}, {"type", "setSampleRate"}, {"hz", 44100}}, {}, {}});
            check(changed["ok"] == true && audio(s)["sampleRate"] == 44100,
                  "unknown metadata does not prevent a stream-validated rate change");
        }
        fake::outputs = {output("USB Stereo", 3, true)};
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
            // Starting a stream does not mean its first callback ran (notably on Windows).
            // Panic marks a pending engine reset; wait for real render progress before an edit.
            const auto completed = fake::completed_blocks.load(std::memory_order_acquire);
            const auto deadline = std::chrono::steady_clock::now() + 1s;
            while (fake::completed_blocks.load(std::memory_order_acquire) == completed &&
                   std::chrono::steady_clock::now() < deadline)
                std::this_thread::yield();
            check(fake::completed_blocks.load(std::memory_order_acquire) > completed,
                  "the restored stream must render before testing its editable graph");
            auto edit = s.reply({{{"v", 0},
                                  {"id", "old-graph"},
                                  {"type", "setParam"},
                                  {"moduleId", 2},
                                  {"paramId", "rate"},
                                  {"value", 4}},
                                 {},
                                 {}});
            if (edit["ok"] != true)
                throw std::runtime_error("the previous document remains editable after rollback: " + edit.dump());
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
        // Registry and PortAudio enumeration are separately faked: no OS registry,
        // PortAudio DLL or physical device is ever touched by this test executable.
        fake::installed_asio = {"ASIO4ALL v2", "FlexASIO"};
        fake::outputs = {{"ASIO|ASIO4ALL v2", "ASIO4ALL v2", "ASIO", 3, true}};
        options.settings = (root / "asio-diagnostics.json").string();
        {
            Session s(options);
            s.start(base);
            const auto all = s.devices();
            check(all["audioDevices"].size() == 1 && all["audioDevices"][0]["id"] == "ASIO|ASIO4ALL v2",
                  "existing PortAudio identity is preserved");
            check(all["audio"]["unavailableDrivers"].size() == 1 &&
                      all["audio"]["unavailableDrivers"][0]["name"] == "FlexASIO" &&
                      all["audio"]["unavailableDrivers"][0]["hostApi"] == "ASIO" &&
                      all["audio"]["unavailableDrivers"][0]["reason"] == "driver-init-failed",
                  "missing ASIO init is a separate optional diagnostic, not a selectable endpoint");
            const auto listed = s.reply({{{"v", 0}, {"id", "asio-list"}, {"type", "listAudioDevices"}}, {}, {}});
            check(listed["ok"] == true && listed["result"]["devices"].size() == 1 &&
                      listed["result"]["audio"]["unavailableDrivers"].size() == 1 &&
                      s.hello()["audio"]["unavailableDrivers"].size() == 1,
                  "hello and explicit enumeration expose the same optional unavailable-driver diagnostic");
        }
        fake::refuse = true;
        options.settings = (root / "asio-error.json").string();
        {
            Session s(options);
            s.start(base);
            const auto status = audio(s);
            check(status["state"] == "reconnecting" && status["error"] == "fake unavailable/ambiguous output",
                  "control-thread opening failure must reach bounded audio status");
            fake::refuse = false;
            s.poll(base + 100ms);
            check(audio(s)["state"] == "running" && !audio(s).contains("error"),
                  "successful ASIO recovery clears the preceding failure text");
        }
        fake::refuse = true;
        for (const auto &diagnostic :
             {std::string(199, 'x') + "\xc3\xa9", std::string("host ") + "\xff\xc0" + " refusal"}) {
            fake::open_error = diagnostic;
            options.settings = (root / "asio-invalid-utf8-status.json").string();
            Session s(options);
            s.start(base);
            const auto status = s.status();
            const auto error = status["status"]["audio"]["error"].get<std::string>();
            check(error.size() <= 200 && Json::parse(status.dump())["status"]["audio"]["error"] == error,
                  "malformed/split host failure must serialize as bounded status.audio.error");
            check(error.find('\xff') == std::string::npos && error.find('\xc0') == std::string::npos,
                  "invalid host bytes must not survive in status.audio.error");
            fake::refuse = false;
            s.poll(base + 100ms);
            check(audio(s)["state"] == "running" && !audio(s).contains("error"),
                  "successful reopen clears the sanitized host error");
            fake::refuse = true;
        }
        fake::refuse = false;
        fake::open_error.clear();
        fake::installed_asio.clear();
        // Keel #627 ruling: an ASIO driver that refuses the block gets exactly one
        // retry at its preferred size 1..4096 after a quiescent graph reprepare.
        fake::refuse = false;
        fake::outputs = {{"ASIO|ASIO4ALL v2", "ASIO4ALL v2", "ASIO", 3, true}};
        const auto saved_block = options.block;
        options.block = 256;
        fake::accept_block = 512;
        fake::preferred = 512;
        options.settings = (root / "asio-forced.json").string();
        {
            fake::blocks.clear();
            Session s(options);
            s.start(base);
            const auto status = audio(s);
            check(fake::blocks == std::vector<uint32_t>{256, 512} && status["state"] == "running" &&
                      status["bufferFrames"] == 512 && status["requestedBufferFrames"] == 256 &&
                      !status.contains("error"),
                  "preferred size in range: one retry at the driver size, actual size reported");
            std::ifstream saved(options.settings);
            const auto prefs = Json::parse(saved);
            check(!prefs.contains("audio") || prefs["audio"].value("bufferFrames", 0) != 512,
                  "the driver-forced size is never persisted");
            s.poll(base + 100ms);
            check(fake::blocks.size() == 2, "a running forced stream is not reopened");
        }
        fake::preferred = 8192;
        options.settings = (root / "asio-out-of-range.json").string();
        {
            fake::blocks.clear();
            Session s(options);
            s.start(base);
            const auto status = audio(s);
            check(fake::blocks == std::vector<uint32_t>{256} && status["state"] == "reconnecting" &&
                      status["error"] == "ASIO driver requires 8192 frames (supported 1-4096)" &&
                      status["bufferFrames"] == 256 && !status.contains("requestedBufferFrames"),
                  "preferred size out of range: no second open, bounded error, configured size kept");
        }
        fake::accept_block = 1024; // neither the request nor the preferred size opens
        fake::preferred = 512;
        options.settings = (root / "asio-retry-fails.json").string();
        {
            fake::blocks.clear();
            Session s(options);
            s.start(base);
            const auto status = audio(s);
            check(fake::blocks == std::vector<uint32_t>{256, 512} && status["state"] == "reconnecting" &&
                      status["error"] == "fake ASIO buffer refused" && status["bufferFrames"] == 256 &&
                      !status.contains("requestedBufferFrames"),
                  "failed retry: exactly one retry, error set, no loop and configured size kept");
        }
        // Device-off regression for Codex's OSC worker UAF: the worker holds a
        // borrowed AppEngine* while an ASIO preferred-size retry swaps graphs.
        // Prove loopback ingress still works after both commit and rollback.
        fake::accept_block = 0;
        fake::preferred = 0;
        options.settings = (root / "asio-osc-rebind.json").string();
        fake::render = true; // live loadGraph needs the paced fake callback for its D5 handoff
        {
            Session s(options);
            s.start(base);
            constexpr uint16_t port = 49715;
            Json patch = {{"format", "wirescape.native.patch"},
                          {"version", 2},
                          {"modules", Json::array({{{"id", 1}, {"module", "essentials.output"}},
                                                   {{"id", 2}, {"module", "osc.in"},
                                                    {"config", {{"port", port}, {"lan", false},
                                                                {"rows", Json::array({{{"pattern", "/reconnect"},
                                                                                       {"kind", "cv"}}})}}}}})},
                          {"connections", Json::array()}};
            auto loaded = s.reply({{{"v", 0}, {"id", "osc-rebind"}, {"type", "loadGraph"}, {"patch", patch}}, {}, {}});
            check(loaded["ok"] == true, "OSC graph loads into fake ASIO playback");
            auto packet_received = [&](uint64_t minimum) {
                const auto deadline = std::chrono::steady_clock::now() + 1200ms;
                while (std::chrono::steady_clock::now() < deadline) {
                    const auto routes = s.status()["status"]["osc"]["routes"];
                    if (!routes.empty() && routes[0]["received"].get<uint64_t>() >= minimum) return true;
                    std::this_thread::sleep_for(10ms);
                }
                return false;
            };
            send_local_osc(port);
            check(packet_received(1), "OSC works before ASIO device loss");
            fake::active = false;
            s.poll(base + 100ms);
            fake::accept_block = 512;
            fake::preferred = 512;
            s.poll(base + 200ms);
            check(audio(s)["state"] == "running" && audio(s)["bufferFrames"] == 512,
                  "preferred-size reconnect swaps the live graph");
            send_local_osc(port);
            check(packet_received(1), "OSC worker rebinds to the new graph after preferred-size retry");
            fake::active = false;
            s.poll(base + 300ms);
            fake::accept_block = 1024;
            fake::preferred = 256;
            s.poll(base + 400ms);
            check(audio(s)["state"] == "reconnecting" && audio(s)["bufferFrames"] == 512,
                  "failed ASIO retry restores old block and graph");
            send_local_osc(port);
            check(packet_received(1), "OSC worker rebinds to the restored graph after failed retry");
            s.shutdown();
        }
        fake::render = false;
        fake::accept_block = 0;
        fake::preferred = 0;
        options.block = saved_block;
        std::cout << "audio recovery fake-backend tests passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

