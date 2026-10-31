#pragma once
#include "third_party/portaudio.h"
#include "wirescape/engine.h"
#include <QLibrary>
#include <QString>
#include <QVariantList>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

// Control-thread service. Callback touches only its separately owned context.
class AudioDevice final {
public:
    struct Backend {
        decltype(&Pa_Initialize) initialize = nullptr;
        decltype(&Pa_Terminate) terminate = nullptr;
        decltype(&Pa_GetDeviceCount) deviceCount = nullptr;
        decltype(&Pa_GetDeviceInfo) deviceInfo = nullptr;
        decltype(&Pa_GetHostApiInfo) hostInfo = nullptr;
        decltype(&Pa_GetDefaultOutputDevice) defaultDevice = nullptr;
        decltype(&Pa_OpenStream) open = nullptr;
        decltype(&Pa_StartStream) start = nullptr;
        decltype(&Pa_StopStream) stop = nullptr;
        decltype(&Pa_AbortStream) abort = nullptr;
        decltype(&Pa_CloseStream) close = nullptr;
        decltype(&Pa_IsStreamActive) active = nullptr;
        decltype(&Pa_GetErrorText) errorText = nullptr;
        decltype(&Pa_IsFormatSupported) formatSupported = nullptr;
    };
    enum class State { Uninitialized, Stopped, Running, Faulted, CleanupPending };
    AudioDevice();
    explicit AudioDevice(Backend backend); // Injected backend: no DLL/device access in tests.
    explicit AudioDevice(std::optional<Backend> backend);
    ~AudioDevice();
    bool initialize(const QString& directory);
    bool refresh(const QString& directory);
    bool start(ws_engine* engine, int device, double sampleRate, uint32_t block);
    bool startRender(void*, int (*)(void*,float*,uint32_t), int device, double sampleRate, uint32_t block);
    // False means a callback still owns the borrowed engine: do not reset/destroy it.
    bool stop();
    bool poll(); // Control-thread monitoring; true if state/error changed.
    bool running() const { return state_ == State::Running; }
    bool engineInUse() const;
    bool hasStream() const { return stream_ != nullptr; }
    State state() const { return state_; }
    QString error() const { return error_; }
    QVariantList devices() const;
    double cpuLoad() const;
    uint64_t underruns() const;
    uint64_t faults() const;
    QVariantMap diagnostics() const;
private:
    struct CallbackContext {
        void* engine = nullptr;
        int (*render)(void*,float*,uint32_t) = nullptr;
        double sampleRate = 48000;
        std::vector<float> scratch;
        std::atomic<bool> enabled{false}, failed{false};
        std::atomic<unsigned> inFlight{0};
        std::atomic<double> load{0};
        std::atomic<uint64_t> underruns{0}, faults{0};
    };
    static int callback(const void*, void*, unsigned long, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void*);
    bool detachEngine();
    void backendError(const char* operation, PaError code);
    template<typename T> bool resolve(T& target, const char* name) {
        target = reinterpret_cast<T>(library_.resolve(name));
        if (!target) error_ = QStringLiteral("The audio library is missing %1").arg(QString::fromLatin1(name));
        return target != nullptr;
    }
    QLibrary library_;
    Backend backend_;
    QString error_;
    State state_ = State::Uninitialized;
    bool initialized_ = false, injected_ = false;
    int device_ = -1;
    uint32_t block_ = 0;
    QString deviceName_, hostName_;
    PaStream* stream_ = nullptr;
    std::unique_ptr<CallbackContext> context_;
};
