// No PortAudio DLL, physical stream, Qt application or native engine is needed.
#include "audio_device.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Mock {
    static Mock* instance;
    PaStreamCallback* callback = nullptr;
    void* user = nullptr;
    int openCode = 0, startCode = 0, stopCode = 0, abortCode = 0, closeCode = 0, activeCode = 1;
    int openedDevice = -99, openedChannels = 0;
    double openedRate = 0;
    int opens = 0, starts = 0, closes = 0, terminates = 0, renders = 0;
    bool validDevice = true, disconnected = true;
    int formatCode = 0, countCode = 5, defaultId = 0;
    int renderCode = WS_OK;
    std::atomic<bool> holdRender{false}, entered{false}, release{false};
    PaDeviceInfo info{1, "Mock stereo", 0, 0, 2, 0, .01, 0, .02, 48000};
    PaDeviceInfo monoInfo{1, "Mock mono", 0, 0, 1, 0, .01, 0, .02, 44100};
    PaDeviceInfo unknownHostInfo{1, "Mock unknown host", 99, 0, 2, 0, .01, 0, .02, 96000};
    PaDeviceInfo inputOnlyInfo{1, "Mock input only", 0, 1, 0, 0, .01, 0, .02, 48000};
    PaHostApiInfo hostInfo{1, paDirectSound, "Mock host", 5, paNoDevice, 0};
    Mock() { instance = this; }
    static PaError init() { return paNoError; }
    static PaError terminate() { ++instance->terminates; return paNoError; }
    static PaDeviceIndex count() { return instance->countCode; }
    static PaError format(const PaStreamParameters*, const PaStreamParameters*, double) { return instance->formatCode; }
    static const PaDeviceInfo *device(PaDeviceIndex id) {
        auto &m = *instance;
        if (id == 0) return m.validDevice ? &m.info : nullptr;
        if (id == 1) return &m.monoInfo;
        if (id == 2) return &m.unknownHostInfo;
        if (id == 3) return m.disconnected ? nullptr : &m.info;
        if (id == 4) return &m.inputOnlyInfo;
        return nullptr;
    }
    static const PaHostApiInfo *host(PaHostApiIndex id) { return id == 0 ? &instance->hostInfo : nullptr; }
    static PaDeviceIndex defaultDevice() { return instance->defaultId; }
    static PaError open(PaStream **stream, const PaStreamParameters *, const PaStreamParameters *output, double rate,
                        unsigned long, PaStreamFlags, PaStreamCallback *cb, void *user) {
        auto& m = *instance; ++m.opens;
        m.openedDevice = output ? output->device : -99;
        m.openedChannels = output ? output->channelCount : 0;
        m.openedRate = rate;
        if (m.openCode) return m.openCode;
        *stream = reinterpret_cast<PaStream*>(&m); m.callback = cb; m.user = user; return 0;
    }
    static PaError start(PaStream*) { ++instance->starts; return instance->startCode; }
    static PaError stop(PaStream*) { return instance->stopCode; }
    static PaError abort(PaStream*) { return instance->abortCode; }
    static PaError close(PaStream*) { ++instance->closes; return instance->closeCode; }
    static PaError active(PaStream*) { return instance->activeCode; }
    static const char* error(PaError) { return "injected failure"; }
    AudioDevice::Backend backend() { return {init, terminate, count, device, host, defaultDevice, open, start, stop, abort, close, active, error, format}; }
    int invoke(void* output, PaStreamCallbackFlags flags = 0) { return callback(nullptr, output, 32, nullptr, flags, user); }
};
Mock* Mock::instance = nullptr;
ws_engine* engine() { return reinterpret_cast<ws_engine*>(static_cast<uintptr_t>(1)); }
}
extern "C" int WS_CALL ws_engine_render(ws_engine*, float* output, uint32_t frames) {
    auto& mock = *Mock::instance; ++mock.renders;
    mock.entered.store(true);
    if (mock.holdRender.load()) while (!mock.release.load()) std::this_thread::yield();
    std::fill_n(output, frames, .5f); return mock.renderCode;
}
int main() {
    try {
        {
            Mock m; AudioDevice audio(m.backend());
            require(audio.initialize({}), "mock initialization");
            require(m.opens == 0 && !audio.running(), "initialize must not start audio");
            const auto rows = audio.devices();
            require(rows.size() == 4 && m.opens == 0,
                    "enumeration includes stereo, mono, unknown host and default without opening");
            const auto defaultRow = rows[0].toMap();
            require(defaultRow.value("id").toInt() == -1 && defaultRow.value("device_id").toInt() == 0 &&
                        defaultRow.value("is_default").toBool() && defaultRow.value("supported").toBool() &&
                        defaultRow.value("name").toString().contains("Mock host"),
                    "resolved default metadata");
            require(rows[1].toMap().value("id").toInt() == 0 && rows[1].toMap().value("device_id").toInt() == 0 &&
                        rows[2].toMap().value("id").toInt() == 1 && !rows[2].toMap().value("supported").toBool() &&
                        rows[2].toMap().value("reason").toString().contains("Mono") &&
                        rows[2].toMap().value("name").toString().startsWith("Unsupported output"),
                    "stable stereo/mono IDs and reason");
            require(rows[3].toMap().value("id").toInt() == 2 &&
                        rows[3].toMap().value("name").toString().contains("Unknown host API"),
                    "unknown host is explicit");
            m.defaultId = 1;
            require(audio.devices()[0].toMap().value("device_id").toInt() == 1 &&
                        !audio.devices()[0].toMap().value("supported").toBool(),
                    "default row reflects unsupported mono default");
            require(!audio.start(engine(), 1, 48000, 128) && m.opens == 0 && audio.error().contains("mono"),
                    "mono rejected with specific error before open");
            m.defaultId = 0;
            require(!audio.start(engine(), 99, 48000, 128) && m.opens == 0 && audio.error().contains("unavailable"),
                    "absent device rejected before open");
            m.validDevice = false;
            require(!audio.start(engine(), -1, 48000, 128) && m.opens == 0, "invalid device rejected");
            m.validDevice = true; m.formatCode = paInvalidSampleRate;
            require(!audio.start(engine(), -1, 48000, 128) && m.opens == 0, "unsupported format rejected before open");
            m.formatCode = 0; m.countCode = paInternalError;
            require(!audio.refresh({}) && audio.error().contains("Enumerate"), "refresh enumeration failure");
            m.countCode = 1; require(audio.refresh({}), "stopped refresh reinitializes");
            m.openCode = paDeviceUnavailable;
            require(!audio.start(engine(), -1, 48000, 128) && !audio.engineInUse() && !audio.hasStream(), "open failure cleanup");
            m.openCode = 0; m.startCode = paInternalError; m.closeCode = paInternalError;
            require(!audio.start(engine(), -1, 48000, 128), "start failure");
            require(audio.hasStream() && !audio.engineInUse() && audio.state() == AudioDevice::State::CleanupPending, "close failure retained ownership");
            require(audio.error().contains("Start audio") && audio.error().contains("Close audio"), "compound errors retained");
            float out[64]; const int before = m.renders;
            require(m.invoke(out) == paAbort && m.renders == before, "late callback cannot enter detached engine");
            require(!audio.start(engine(), -1, 48000, 128), "cleanup blocks restart");
            m.closeCode = 0; require(audio.stop() && !audio.hasStream(), "close retry");
            m.startCode = 0; require(audio.start(engine(), -1, 48000, 128), "start succeeds");
            require(m.invoke(out, paOutputUnderflow) == paContinue && out[0] == .1f && audio.underruns() == 1, "render and telemetry");
            m.stopCode = paInternalError; m.abortCode = paInternalError; m.closeCode = paInternalError;
            require(audio.stop() && !audio.engineInUse() && audio.hasStream(), "stop/abort/close failure detaches engine");
            require(audio.error().contains("Stop audio") && audio.error().contains("Abort audio") && audio.error().contains("Close audio"), "cleanup errors surfaced");
            m.closeCode = 0; m.stopCode = 0; m.abortCode = 0; audio.stop();
            require(audio.start(engine(), -1, 48000, 128), "explicit restart");
            m.activeCode = 0;
            require(audio.poll() && !audio.running() && !audio.engineInUse() && !audio.hasStream(), "unexpected backend stop reconciled");
            require(audio.error().contains("unexpectedly"), "unexpected stop diagnostic");
            m.activeCode = 1; require(audio.start(engine(), -1, 48000, 128), "restart after stop");
            m.renderCode = WS_STATE;
            require(m.invoke(out) == paAbort && audio.faults() == 1, "callback processing fault aborts");
            require(audio.poll() && !audio.running() && audio.error().contains("callback fault"), "callback fault reconciled");
            m.renderCode = WS_OK; require(audio.start(engine(), -1, 48000, 128), "restart after fault");
            require(m.invoke(nullptr) == paAbort && audio.poll(), "invalid callback output surfaced");
            require(audio.start(engine(), -1, 48000, 128), "restart for backend query failure");
            m.activeCode = paInternalError;
            require(audio.poll() && audio.error().contains("Query audio stream"), "backend query failure surfaced");
        }
        {
            Mock m; AudioDevice audio(m.backend()); audio.initialize({}); audio.start(engine(), -1, 48000, 128);
            m.holdRender.store(true);
            float output[64];
            std::thread callback([&] { m.invoke(output); });
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (!m.entered.load() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            const bool entered = m.entered.load();
            // Releasing from another thread proves stop waits for the in-flight engine render.
            std::thread release([&] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); m.release.store(true); });
            const bool stopped = audio.stop();
            callback.join(); release.join();
            require(entered && stopped && !audio.engineInUse(), "in-flight render quiesces before engine release");
        }
        {
            Mock m;
            AudioDevice audio(m.backend());
            require(audio.initialize({}) && audio.start(engine(), 2, 48000, 128),
                    "explicit nondefault stereo starts on fake backend");
            const auto details = audio.diagnostics();
            require(m.openedDevice == 2 && m.openedChannels == 2 && m.openedRate == 48000 &&
                        details.value("device_id").toInt() == 2 && details.value("output_channels").toInt() == 2 &&
                        details.value("sample_rate").toDouble() == 48000,
                    "selected physical ID and active format roundtrip");
            require(!audio.refresh({}) && audio.error().contains("Stop"), "refresh refused while fake stream runs");
            require(audio.stop() && audio.refresh({}),
                    "stop and cleanup permit refresh without starting another stream");
            require(m.opens == 1 && !audio.running(), "refresh never auto starts or replaces route");
        }
        std::cout << "audio adapter mock lifecycle checks passed; no hardware stream opened\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

