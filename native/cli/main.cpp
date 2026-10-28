#include "app_engine.h"
#include "play.h"
#include "wirescape/output_policy.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
using Engine = std::unique_ptr<AppEngine, decltype(&app_engine_destroy)>;
Engine make(const std::string &dir, double rate, uint32_t block) {
    ws_engine_config c{sizeof(c), rate, block, 32, 8192};
    Engine e(app_engine_create(&c), app_engine_destroy);
    if (!e || app_engine_build_essentials(e.get(), dir.c_str()))
        throw std::runtime_error(e ? app_engine_error(e.get()) : "Cannot create engine");
    return e;
}
void send(AppEngine *e, uint64_t frame, uint32_t node, uint32_t type, float value,
          uint32_t parameter = 0, uint32_t note = 1) {
    ws_event ev{sizeof(ev), frame, node, type, parameter, value, 1, note, app_engine_generation(e)};
    if (app_engine_enqueue(e, &ev))
        throw std::runtime_error("Event rejected");
}
void u16(std::ofstream &o, uint16_t v) {
    o.put(static_cast<char>(v));
    o.put(static_cast<char>(v >> 8));
}
void u32(std::ofstream &o, uint32_t v) {
    u16(o, static_cast<uint16_t>(v));
    u16(o, static_cast<uint16_t>(v >> 16));
}
void render(const std::string &dir, const std::string &path) {
    constexpr uint32_t rate = 48000, frames = rate * 4;
    auto e = make(dir, rate, 512);
    send(e.get(), 0, 3, WS_PARAMETER, .3f);
    const int notes[] = {57, 60, 64, 69};
    for (uint32_t i = 0; i < 4; ++i) {
        auto f = static_cast<uint64_t>(i) * rate;
        send(e.get(), f, 1, WS_NOTE_ON, static_cast<float>(notes[i]));
        send(e.get(), f, 2, WS_NOTE_ON, static_cast<float>(notes[i]), 0, i + 1);
        send(e.get(), f + rate * 3 / 5, 2, WS_NOTE_OFF, 0, 0, i + 1);
    }
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Cannot write WAV");
    out.write("RIFF", 4);
    u32(out, 36 + frames * 2);
    out.write("WAVEfmt ", 8);
    u32(out, 16);
    u16(out, 1);
    u16(out, 1);
    u32(out, rate);
    u32(out, rate * 2);
    u16(out, 2);
    u16(out, 16);
    out.write("data", 4);
    u32(out, frames * 2);
    std::vector<float> buffer(512);
    for (uint32_t pos = 0; pos < frames;) {
        auto n = std::min<uint32_t>(512, frames - pos);
        if (app_engine_render(e.get(), buffer.data(), n))
            throw std::runtime_error("Render failed");
        for (uint32_t i = 0; i < n; ++i)
            u16(out, static_cast<uint16_t>(static_cast<int16_t>(
                         std::lround(std::clamp(buffer[i] * WS_OUTPUT_GAIN, -1.f, 1.f) * 32767))));
        pos += n;
    }
    if (!out)
        throw std::runtime_error("WAV write failed");
    std::cout << "Rendered deterministic 4-second mono WAV: " << path << "\n";
}
double percentile(const std::vector<double> &v, double p) {
    return v[static_cast<size_t>(std::ceil(p * (v.size() - 1)))];
}
void benchmark(const std::string &dir, const std::string &path, uint32_t blocks) {
    std::ostringstream json;
    json << "{\n  \"schema_version\": 1,\n  \"engine\": \"Wirescape native engine2\",\n "
            " \"environment\": {\"os\": \""
#ifdef _WIN32
         << "Windows"
#elif defined(__linux__)
         << "Linux"
#else
         << "Other"
#endif
         << "\", \"architecture\": \""
#if defined(_M_ARM64) || defined(__aarch64__)
         << "arm64"
#elif defined(_M_X64) || defined(__x86_64__)
         << "x86_64"
#else
         << "other"
#endif
         << "\", \"compiler\": \""
#ifdef _MSC_VER
         << "MSVC " << _MSC_VER
#else
         << __VERSION__
#endif
         << "\", \"build_type\": \""
#ifdef NDEBUG
         << "Release"
#else
         << "Debug"
#endif
         << "\", \"hardware_threads\": " << std::thread::hardware_concurrency()
         << "},\n  \"scenarios\": [\n";
    bool first = true;
    for (double rate : {44100., 48000., 96000.})
        for (uint32_t block : {32u, 64u, 128u, 256u, 512u})
          for (uint32_t traffic : {0u, 16u, 64u}) {
            const uint32_t events = std::min(traffic, block);
            auto e = make(dir, rate, block);
            std::vector<float> buffer(block);
            send(e.get(), 0, 2, WS_NOTE_ON, 60);
            for (int i = 0; i < 200; ++i)
                app_engine_render(e.get(), buffer.data(), block);
            std::vector<double> times;
            times.reserve(blocks);
            uint32_t missed = 0;
            double deadline = block / rate * 1e6;
            for (uint32_t i = 0; i < blocks; ++i) {
                for (uint32_t k = 0; k < events; ++k) {
                    const uint64_t frame = static_cast<uint64_t>(200 + i) * block + static_cast<uint64_t>(k) * block / events;
                    if (k % 8 == 0) send(e.get(), frame, 2, WS_NOTE_ON, 60);
                    else if (k % 8 == 1) send(e.get(), frame, 2, WS_NOTE_OFF, 0);
                    else if (k % 8 == 2) send(e.get(), frame, 3, WS_PARAMETER, .3f);
                    else send(e.get(), frame, 1, WS_PARAMETER, 220.f + (k % 7) * 55.f);
                }
                auto start = std::chrono::steady_clock::now();
                if (app_engine_render(e.get(), buffer.data(), block))
                    throw std::runtime_error("Benchmark render failed");
                double us = std::chrono::duration<double, std::micro>(
                                std::chrono::steady_clock::now() - start)
                                .count();
                times.push_back(us);
                if (us > deadline)
                    ++missed;
            }
            std::sort(times.begin(), times.end());
            if (!first)
                json << ",\n";
            first = false;
            json << "    {\"sample_rate\": " << rate << ", \"block_size\": " << block
                 << ", \"events_per_block\": " << events
                 << ", \"traffic_target\": " << traffic
                 << ", \"blocks\": " << blocks
                 << ", \"frames\": " << static_cast<uint64_t>(blocks) * block
                 << ", \"deadline_us\": " << deadline << ", \"deadline_misses\": " << missed
                 << ", \"render_us\": {\"p50\": " << percentile(times, .5)
                 << ", \"p95\": " << percentile(times, .95)
                 << ", \"p99\": " << percentile(times, .99) << ", \"max\": " << times.back()
                 << "}}";
        }
    json << "\n  ],\n  \"scope\": \"Offline DSP only; OS scheduling outliers included; no "
            "audio-device or end-to-end latency claim.\"\n}\n";
    if (path.empty())
        std::cout << json.str();
    else {
        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("Cannot write benchmark JSON");
        out << json.str();
        std::cout << "Benchmark JSON: " << path << "\n";
    }
}
} // namespace
int main(int argc, char **argv) {
    try {
        if(argc>1&&std::string(argv[1])=="play")return wirescape_play(argc,argv);
        if (argc < 2) {
            std::cout << "wirescape_cli render --modules DIR --output demo.wav\nwirescape_cli "
                         "benchmark --modules DIR [--json report.json] [--blocks 3000]\n";
            return 0;
        }
        std::string command = argv[1], directory = ".", output, json;
        uint32_t blocks = 3000;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (i + 1 >= argc)
                throw std::runtime_error("Missing option value");
            auto value = std::string(argv[++i]);
            if (arg == "--modules")
                directory = value;
            else if (arg == "--output")
                output = value;
            else if (arg == "--json")
                json = value;
            else if (arg == "--blocks") {
                blocks = static_cast<uint32_t>(std::stoul(value));
                if (!blocks || blocks > 1000000)
                    throw std::runtime_error("blocks must be 1..1000000");
            } else
                throw std::runtime_error("Unknown option: " + arg);
        }
        if (command == "render")
            render(directory, output.empty() ? "essentials-demo.wav" : output);
        else if (command == "benchmark")
            benchmark(directory, json, blocks);
        else
            throw std::runtime_error("Unknown command");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
