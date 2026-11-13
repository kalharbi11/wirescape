// SPDX-License-Identifier: GPL-3.0-only
// Exercises the production audio::Stream against only the device-off fake PortAudio DLL/so.
#include "../../document/runtime_patch.h"
#include "portaudio_service.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        require(argc == 4, "fake backend directory, library, and sidecar directory required");
        require(std::filesystem::weakly_canonical(argv[1]) != std::filesystem::weakly_canonical(argv[3]),
                "fake PortAudio must never be placed beside wirescape-engine");
        const auto fake_dir = std::filesystem::weakly_canonical(argv[1]);
        const auto library_dir = std::filesystem::weakly_canonical(argv[2]).parent_path();
        require(library_dir == fake_dir, "fake PortAudio library must reside inside its test-only directory");
#ifdef _WIN32
        auto *library = LoadLibraryA(argv[2]);
        require(library != nullptr, "load fake DLL exclusively");
        auto symbol = [&](const char *name) { return GetProcAddress(library, name); };
#else
        auto *library = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
        require(library != nullptr, "load fake PortAudio shared library exclusively");
        auto symbol = [&](const char *name) { return dlsym(library, name); };
#endif
        const auto fail = reinterpret_cast<void (*)(int)>(symbol("ws_fake_fail"));
        const auto stats = reinterpret_cast<void (*)(int *)>(symbol("ws_fake_stats"));
        require(fail && stats, "fake-only symbol signatures present");
        const ws_engine_config config{sizeof(ws_engine_config), 48000, 128, 16, 2048};
        wirescape::audio::EngineOwner catalogue(app_engine_create(&config), app_engine_destroy);
        require(bool(catalogue), "create test catalogue");
        const std::string patch = R"({"format":"wirescape.native.patch","version":2,
          "settings":{"sample_rate":48000},
          "modules":[{"id":1,"module":"essentials.output","version":2,"parameters":{}}],
          "connections":[],"outputs":[{"module":1,"port":"audio","bus_channel":0}]})";
        const auto make = [&] {
            std::string error;
            wirescape::audio::EngineOwner engine(
                wirescape::document::load_runtime_patch(patch, catalogue.get(), config, error), app_engine_destroy);
            require(bool(engine), "prepare fake Stream graph");
            return engine;
        };
        const wirescape::audio::StreamSelection selection{argv[1], ""};
        for (int mode : {1, 2}) {
            auto engine = make();
            fail(mode);
            bool refused = false;
            try {
                wirescape::audio::Stream stream(engine, selection, 48000, 128);
            } catch (const std::exception &) {
                refused = true;
            }
            require(refused && engine != nullptr, "failed open/start must return graph ownership and clean up backend");
        }
        auto engine = make();
        fail(0);
        int stopped_producers = 0;
        wirescape::audio::Stream stream(engine, selection, 48000, 128, [&] { ++stopped_producers; });
        require(!engine && stream.buffer_frames() == 128, "stream owns engine and real block size");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        int before[5]{};
        do {
            stats(before);
            if (before[2] > 0) break;
            std::this_thread::yield();
        } while (std::chrono::steady_clock::now() < deadline);
        require(before[0] >= 3 && before[1] >= 2 && before[2] > 0, "fake callbacks actually entered production Stream");
        engine = stream.stop();
        require(bool(engine) && stopped_producers > 0, "quiescent stop returns engine after stopping producers");
        int stopped[5]{};
        stats(stopped);
        std::this_thread::yield();
        int later[5]{};
        stats(later);
        require(stopped[2] == later[2] && stopped[3] >= 2 && stopped[4] >= 2,
                "no callback runs after Stream stop; start failure also closed");
        auto abort_engine = make();
        fail(3);
        wirescape::audio::Stream abort_stream(abort_engine, selection, 48000, 128);
        abort_engine = abort_stream.stop();
        require(bool(abort_engine), "stop failure uses abort then returns graph ownership");
        fail(0);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
