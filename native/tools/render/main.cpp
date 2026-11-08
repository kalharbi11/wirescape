// SPDX-License-Identifier: GPL-3.0-only
// Device-free ABI2 evidence renderer; Python evidence.py owns assertions and provenance reports.
#include "runtime_patch.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using Json = nlohmann::json;
using App = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
static void need(bool ok, const std::string &message) {
    if (!ok) throw std::runtime_error(message);
}
static std::string read_bounded(const std::string &path, uintmax_t limit) {
    need(std::filesystem::file_size(path) <= limit, "input exceeds size limit: " + path);
    std::ifstream in(path, std::ios::binary);
    need(bool(in), "cannot open " + path);
    return {std::istreambuf_iterator<char>(in), {}};
}
static uint32_t u32(const Json &x, const char *what) {
    need(x.is_number_integer(), std::string("invalid ") + what);
    const int64_t value = x.get<int64_t>();
    need(value >= 0 && value <= UINT32_MAX, std::string("out-of-range ") + what);
    return uint32_t(value);
}
static void put16(std::ostream &out, uint16_t x) {
    out.put(char(x & 255)); out.put(char((x >> 8) & 255));
}
static void put32(std::ostream &out, uint32_t x) {
    put16(out, uint16_t(x)); put16(out, uint16_t(x >> 16));
}
static void wav(const std::string &path, const std::vector<float> &samples, uint32_t rate) {
    std::filesystem::path dest(path);
    if (!dest.parent_path().empty()) std::filesystem::create_directories(dest.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    need(bool(out), "cannot create WAV: " + path);
    const uint32_t size = uint32_t(samples.size() * sizeof(float));
    out.write("RIFF", 4); put32(out, 36 + size); out.write("WAVEfmt ", 8);
    put32(out, 16); put16(out, 3); put16(out, 2); // IEEE float, stereo
    put32(out, rate); put32(out, rate * 8); put16(out, 8); put16(out, 32);
    out.write("data", 4); put32(out, size);
    for (float sample : samples) {
        uint32_t bits;
        std::memcpy(&bits, &sample, sizeof(bits));
        put32(out, bits); // explicit little-endian IEEE-754; never clamp
    }
    out.flush();
    need(bool(out), "WAV write failed: " + path);
}
int main(int argc, char **argv) {
    try {
        std::map<std::string, std::string> args;
        for (int i = 1; i < argc; i += 2) {
            need(i + 1 < argc, "missing option value");
            const std::string key(argv[i]);
            need(key == "--patch" || key == "--script" || key == "--out" ||
                 key == "--rate" || key == "--block" || key == "--seconds" || key == "--seed",
                 "unknown option: " + key);
            need(args.emplace(key, argv[i + 1]).second, "duplicate option: " + key);
        }
        for (const char *key : {"--patch", "--script", "--out", "--rate", "--block", "--seconds", "--seed"})
            need(args.count(key) != 0, std::string("missing ") + key);
        const auto rate = std::stoul(args.at("--rate")), block = std::stoul(args.at("--block"));
        const double seconds = std::stod(args.at("--seconds"));
        const auto seed = std::stoull(args.at("--seed"));
        need(rate >= 8000 && rate <= 96000 && block >= 1 && block <= 1024 &&
             std::isfinite(seconds) && seconds > 0 && seconds <= 10, "invalid rate/block/duration");
        const uint64_t frames = uint64_t(std::llround(rate * seconds));
        need(frames > 0 && frames * 8 <= 20u * 1024u * 1024u, "WAV exceeds 20 MiB");
        Json patch = Json::parse(read_bounded(args.at("--patch"), wirescape::document::RuntimePatchLimit));
        need(patch.is_object() && patch.value("version", 0) == 2, "requires ABI2 patch v2");
        patch["settings"]["seed"] = seed;
        const Json script = Json::parse(read_bounded(args.at("--script"), 65536));
        need(script.is_array() && script.size() <= 128, "script must have <=128 events");
        ws_engine_config cfg{sizeof(cfg), double(rate), uint32_t(block), 64, 2048};
        App catalog(app_engine_create(&cfg), app_engine_destroy);
        need(bool(catalog), "catalogue creation failed");
        std::string error;
        App engine(wirescape::document::load_runtime_patch(patch.dump(), catalog.get(), cfg, error),
                   app_engine_destroy);
        need(bool(engine), "patch load failed: " + error);
        uint64_t previous = 0;
        for (const auto &entry : script) {
            need(entry.is_object(), "script entry must be an object");
            const uint64_t frame = u32(entry.at("frame"), "event frame");
            need(frame >= previous && frame < frames, "events must be sorted and within duration");
            previous = frame;
            const uint32_t node = u32(entry.at("node"), "event node");
            const std::string type = entry.at("type").get<std::string>();
            ws_event event{};
            event.struct_size = sizeof(event);
            event.frame = frame;
            event.node_id = node;
            event.source_id = 1;
            event.generation = app_engine_generation(engine.get());
            if (type == "noteOn" || type == "noteOff") {
                const auto key = u32(entry.at("key"), "MIDI key");
                need(key <= 127, "MIDI key >127");
                event.type = type == "noteOn" ? WS_NOTE_ON : WS_NOTE_OFF;
                event.note_id = key;
                event.value = float(key);
            } else if (type == "gate") {
                event.type = WS_GATE;
                event.value = entry.at("value").get<float>();
                need(std::isfinite(event.value), "non-finite gate");
            } else if (type == "setParam") {
                event.type = WS_PARAMETER;
                event.value = entry.at("value").get<float>();
                need(std::isfinite(event.value), "non-finite parameter");
                std::string module;
                for (const auto &m : patch.at("modules"))
                    if (u32(m.at("id"), "module id") == node)
                        module = m.at("module").get<std::string>();
                need(!module.empty(), "parameter targets missing node");
                const std::string parameter = entry.at("parameter").get<std::string>();
                bool found = false;
                for (uint32_t i = 0; i < app_engine_module_count(engine.get()); ++i) {
                    const auto *descriptor = app_engine_module(engine.get(), i);
                    if (module != descriptor->id) continue;
                    for (uint32_t p = 0; p < descriptor->parameter_count; ++p)
                        if (parameter == app_engine_parameter_id(engine.get(), module.c_str(), p)) {
                            const auto &d = descriptor->parameters[p];
                            need(event.value >= d.minimum && event.value <= d.maximum, "parameter outside range");
                            event.parameter_id = d.id;
                            found = true;
                        }
                }
                need(found, "unknown parameter: " + parameter);
            } else throw std::runtime_error("unknown event type: " + type);
            need(app_engine_enqueue(engine.get(), &event) == WS_OK,
                 "frame-addressed event enqueue failed at frame " + std::to_string(frame));
        }
        std::vector<float> interleaved(size_t(frames) * 2);
        std::vector<float> left(block), right(block);
        for (uint64_t start = 0; start < frames; start += block) {
            const auto n = uint32_t(std::min<uint64_t>(block, frames - start));
            float *rows[]{left.data(), right.data()};
            need(app_engine_render_channels(engine.get(), rows, 2, n) == WS_OK,
                 "render failed at frame " + std::to_string(start));
            for (uint32_t i = 0; i < n; ++i) {
                interleaved[size_t(start + i) * 2] = left[i];
                interleaved[size_t(start + i) * 2 + 1] = right[i];
            }
        }
        need(app_engine_native(engine.get()).meter().faults == 0, "engine faults during offline render");
        wav(args.at("--out"), interleaved, uint32_t(rate));
        std::cout << "rendered " << frames << " ABI2 frames; devices off\n";
    } catch (const std::exception &e) {
        std::cerr << "wirescape_render: " << e.what() << '\n';
        return 1;
    }
}
