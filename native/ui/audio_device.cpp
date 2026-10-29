#include "audio_device.h"
#include "wirescape/output_policy.h"
#include <QDir>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

AudioDevice::AudioDevice() : context_(std::make_unique<CallbackContext>()) {}
AudioDevice::AudioDevice(Backend backend) : AudioDevice() { backend_ = backend; injected_ = true; }
AudioDevice::~AudioDevice() {
    stop();
    if (initialized_) {
        const auto code = backend_.terminate();
        // Successful final termination closes any remaining streams (PortAudio contract).
        if (code == paNoError) stream_ = nullptr;
    }
    if (stream_ || !detachEngine()) {
        // A broken backend must not leave its callback pointing at freed storage/code.
        // Quarantine until process exit; stop() permits retry before destruction.
        context_.release();
        library_.setLoadHints(library_.loadHints() | QLibrary::PreventUnloadHint);
    }
}
void AudioDevice::backendError(const char* operation, PaError code) {
    const auto text = QStringLiteral("%1: %2").arg(QString::fromLatin1(operation), QString::fromUtf8(backend_.errorText(code)));
    if (!error_.isEmpty()) error_ += "; ";
    error_ += text;
}
bool AudioDevice::initialize(const QString& directory) {
    if (initialized_) return true;
    if (!injected_) {
        const QString custom = qEnvironmentVariable("WIRESCAPE_PORTAUDIO");
        QStringList candidates;
        if (!custom.isEmpty()) candidates << custom;
#ifdef Q_OS_WIN
        candidates << QDir(directory).filePath("libportaudio64bit.dll") << QDir(directory).filePath("portaudio.dll");
#elif defined(Q_OS_MACOS)
        candidates << QDir(directory).filePath("libportaudio.2.dylib") << "portaudio.2";
#else
        candidates << QDir(directory).filePath("libportaudio.so.2") << "libportaudio.so.2";
#endif
        // Keep backend code resident through process exit, including catastrophic teardown failure.
        library_.setLoadHints(QLibrary::PreventUnloadHint);
        for (const auto& candidate : candidates) { library_.setFileName(candidate); if (library_.load()) break; }
        if (!library_.isLoaded()) { error_ = "Audio backend unavailable. Install PortAudio or place its library beside Wirescape."; return false; }
#define WS_RESOLVE(member, symbol) resolve(backend_.member, symbol)
        if (!(WS_RESOLVE(initialize, "Pa_Initialize") && WS_RESOLVE(terminate, "Pa_Terminate") &&
              WS_RESOLVE(deviceCount, "Pa_GetDeviceCount") && WS_RESOLVE(deviceInfo, "Pa_GetDeviceInfo") &&
              WS_RESOLVE(hostInfo, "Pa_GetHostApiInfo") && WS_RESOLVE(defaultDevice, "Pa_GetDefaultOutputDevice") &&
              WS_RESOLVE(open, "Pa_OpenStream") && WS_RESOLVE(start, "Pa_StartStream") &&
              WS_RESOLVE(stop, "Pa_StopStream") && WS_RESOLVE(abort, "Pa_AbortStream") &&
              WS_RESOLVE(close, "Pa_CloseStream") && WS_RESOLVE(active, "Pa_IsStreamActive") && WS_RESOLVE(errorText, "Pa_GetErrorText") && WS_RESOLVE(formatSupported, "Pa_IsFormatSupported"))) return false;
#undef WS_RESOLVE
    }
    if (!backend_.initialize || !backend_.terminate || !backend_.deviceCount || !backend_.deviceInfo ||
        !backend_.hostInfo || !backend_.defaultDevice || !backend_.open || !backend_.start || !backend_.stop ||
        !backend_.abort || !backend_.close || !backend_.active || !backend_.errorText || !backend_.formatSupported) {
        error_ = "Audio backend is incomplete."; return false;
    }
    error_.clear();
    const auto code = backend_.initialize();
    if (code != paNoError) { backendError("Initialize audio", code); return false; }
    initialized_ = true; state_ = State::Stopped; return true;
}
bool AudioDevice::refresh(const QString& directory) {
    if (stream_ || engineInUse()) { error_ = "Stop audio and complete cleanup before refreshing devices."; return false; }
    if (initialized_) {
        error_.clear(); const auto code = backend_.terminate();
        if (code != paNoError) { backendError("Refresh audio devices", code); state_ = State::Faulted; return false; }
        initialized_ = false; state_ = State::Uninitialized;
    }
    if (!initialize(directory)) return false;
    const auto count = backend_.deviceCount();
    if (count < 0) { backendError("Enumerate audio outputs", count); state_ = State::Faulted; return false; }
    return true;
}
QVariantMap AudioDevice::diagnostics() const {
    return {{"state", static_cast<int>(state_)}, {"running", running()}, {"cleanup_pending", hasStream() && !running()},
            {"device_id", device_}, {"device", deviceName_}, {"backend", hostName_},
            {"sample_rate", context_->sampleRate}, {"block_frames", block_}, {"output_channels", 2},
            {"underruns", QVariant::fromValue<qulonglong>(underruns())},
            {"callback_faults", QVariant::fromValue<qulonglong>(faults())}, {"error", error_}};
}
QVariantList AudioDevice::devices() const {
    QVariantList result;
    result.append(QVariantMap{{"id", -1}, {"name", "System default output"}});
    if (!initialized_) return result;
    const auto count = backend_.deviceCount();
    for (int i = 0; i < count; ++i) {
        const auto* info = backend_.deviceInfo(i);
        if (!info || info->maxOutputChannels < 2) continue;
        const auto* host = backend_.hostInfo(info->hostApi);
        result.append(QVariantMap{{"id", i}, {"name", QStringLiteral("%1 / %2").arg(QString::fromUtf8(info->name), host ? QString::fromUtf8(host->name) : QString())}});
    }
    return result;
}
bool AudioDevice::engineInUse() const { return context_->engine != nullptr; }
double AudioDevice::cpuLoad() const { return context_->load.load(std::memory_order_relaxed); }
uint64_t AudioDevice::underruns() const { return context_->underruns.load(std::memory_order_relaxed); }
uint64_t AudioDevice::faults() const { return context_->faults.load(std::memory_order_relaxed); }
bool AudioDevice::detachEngine() {
    // Sequential consistency closes the entry race: after disabled, a new callback
    // cannot observe enabled, while any callback that did is counted before access.
    context_->enabled.store(false);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (context_->inFlight.load() != 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            error_ += "; Audio callback did not quiesce; engine retained."; return false;
        }
        std::this_thread::yield();
    }
    context_->engine = nullptr;
    context_->load.store(0);
    return true;
}
bool AudioDevice::start(ws_engine* engine, int device, double rate, uint32_t block) {
    return startRender(engine, [](void* e,float* out,uint32_t n){return ws_engine_render(static_cast<ws_engine*>(e),out,n);},device,rate,block);
}
bool AudioDevice::startRender(void* engine,int (*render)(void*,float*,uint32_t),int device,double rate,uint32_t block) {
    if (stream_ || engineInUse()) { error_ = "Finish audio cleanup before starting again."; return false; }
    if (!initialized_ || !engine || block == 0 || block > 512 || !std::isfinite(rate) || rate <= 0) {
        error_ = "Audio is not initialized or its engine/settings are invalid."; return false;
    }
    error_.clear();
    if (device == -1) device = backend_.defaultDevice();
    const auto* info = device >= 0 ? backend_.deviceInfo(device) : nullptr;
    if (!info || info->maxOutputChannels < 2) { error_ = "Choose an available stereo output device."; state_ = State::Faulted; return false; }
    PaStreamParameters output{device, 2, paFloat32, info->defaultLowOutputLatency, nullptr};
    const auto supported = backend_.formatSupported(nullptr, &output, rate);
    if (supported != paFormatIsSupported) { backendError("Output does not support the requested stereo sample rate", supported); state_ = State::Faulted; return false; }
    device_ = device; block_ = block; deviceName_ = QString::fromUtf8(info->name);
    const auto* host = backend_.hostInfo(info->hostApi);
    hostName_ = host ? QString::fromUtf8(host->name) : QString();
    context_->scratch.assign(512, 0);
    context_->engine = engine; context_->render = render; context_->sampleRate = rate;
    context_->underruns.store(0); context_->faults.store(0); context_->load.store(0); context_->failed.store(false);
    auto code = backend_.open(&stream_, nullptr, &output, rate, block, paNoFlag, &AudioDevice::callback, context_.get());
    if (code == paNoError && !stream_) code = paInternalError;
    if (code == paNoError) { context_->enabled.store(true); code = backend_.start(stream_); }
    if (code != paNoError) { backendError("Start audio", code); stop(); if (!stream_) state_ = State::Faulted; return false; }
    state_ = State::Running;
    return true;
}
bool AudioDevice::stop() {
    context_->enabled.store(false);
    if (stream_) {
        auto code = backend_.stop(stream_);
        if (code != paNoError && code != paStreamIsStopped) {
            backendError("Stop audio", code);
            code = backend_.abort(stream_);
            if (code != paNoError && code != paStreamIsStopped) backendError("Abort audio", code);
        }
        code = backend_.close(stream_);
        if (code == paNoError) stream_ = nullptr;
        else backendError("Close audio", code);
    }
    const bool safe = detachEngine();
    state_ = stream_ || !safe ? State::CleanupPending : initialized_ ? State::Stopped : State::Uninitialized;
    return safe;
}
bool AudioDevice::poll() {
    if (!running()) return false;
    const auto active = backend_.active(stream_);
    if (!context_->failed.load() && active == 1) return false;
    if (context_->failed.load()) error_ = "Audio processing stopped after a native callback fault.";
    else if (active < 0) backendError("Query audio stream", active);
    else error_ = "Audio output stopped unexpectedly. Start audio explicitly to retry.";
    stop();
    if (!stream_ && !engineInUse()) state_ = State::Faulted;
    return true;
}
int AudioDevice::callback(const void*, void* output, unsigned long frames,
                          const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags flags, void* user) {
    auto& self = *static_cast<CallbackContext*>(user);
    self.inFlight.fetch_add(1);
    struct Exit { CallbackContext& self; ~Exit() { self.inFlight.fetch_sub(1); } } exit{self};
    auto* out = static_cast<float*>(output);
    if (!out) { self.faults.fetch_add(1); self.failed.store(true); return paAbort; }
    if (!self.enabled.load()) { std::fill_n(out, frames * 2, 0.f); return paAbort; }
    const auto before = std::chrono::steady_clock::now();
    if (flags & paOutputUnderflow) self.underruns.fetch_add(1, std::memory_order_relaxed);
    for (unsigned long offset = 0; offset < frames;) {
        const auto n = static_cast<uint32_t>(std::min<unsigned long>(frames - offset, static_cast<unsigned long>(self.scratch.size())));
        int code = WS_STATE;
        try { code = self.render(self.engine, self.scratch.data(), n); } catch (...) { code = WS_STATE; }
        if (code != WS_OK) {
            std::fill_n(out + offset * 2, (frames - offset) * 2, 0.f);
            self.faults.fetch_add(1, std::memory_order_relaxed); self.failed.store(true); return paAbort;
        }
        for (uint32_t i = 0; i < n; ++i) {
            const float value = std::isfinite(self.scratch[i]) ? std::clamp(self.scratch[i] * WS_OUTPUT_GAIN, -1.f, 1.f) : 0.f;
            out[(offset + i) * 2] = value; out[(offset + i) * 2 + 1] = value;
        }
        offset += n;
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count();
    self.load.store(frames ? elapsed * self.sampleRate / static_cast<double>(frames) : 0, std::memory_order_relaxed);
    return paContinue;
}
