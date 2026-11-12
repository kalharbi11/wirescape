// SPDX-License-Identifier: GPL-3.0-only
// Device-off PortAudio stand-in loaded by the production audio::Stream code.
// Never enumerates or opens OS audio devices.
#include "../third_party/portaudio/portaudio.h"
#include <atomic>
#include <chrono>
#include <thread>
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
namespace {
struct FakeStream {
    PaStreamCallback* callback = nullptr;
    void* user = nullptr;
    unsigned long frames = 0;
    std::atomic<bool> active{false};
    std::thread worker;
};
std::atomic<int> opens{0}, starts{0}, callbacks{0}, stops{0}, closes{0}, fail{0};
const PaDeviceInfo device{2, "fake-device-no-hardware", 0, 0, 2, 0, 0.01, 0, 0.02, 48000};
const PaHostApiInfo api{1, paInDevelopment, "FAKE", 1, paNoDevice, 0};
FakeStream* cast(PaStream* stream) { return static_cast<FakeStream*>(stream); }
} // namespace
extern "C" {
EXPORT void ws_fake_fail(int code) { fail = code; }
EXPORT void ws_fake_stats(int* out) {
    const int counts[]{opens.load(), starts.load(), callbacks.load(), stops.load(), closes.load()};
    for (int i = 0; i < 5; ++i) out[i] = counts[i];
}
EXPORT PaError Pa_Initialize(void) { return paNoError; }
EXPORT PaError Pa_Terminate(void) { return paNoError; }
EXPORT PaDeviceIndex Pa_GetDeviceCount(void) { return 1; }
EXPORT const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex id) { return id == 0 ? &device : nullptr; }
EXPORT const PaHostApiInfo* Pa_GetHostApiInfo(PaHostApiIndex id) { return id == 0 ? &api : nullptr; }
EXPORT PaDeviceIndex Pa_GetDefaultOutputDevice(void) { return 0; }
EXPORT const char* Pa_GetErrorText(PaError) { return "injected fake PortAudio failure"; }
EXPORT PaError Pa_IsFormatSupported(const PaStreamParameters*, const PaStreamParameters*, double) {
    return paNoError;
}
EXPORT PaError Pa_OpenStream(PaStream** out, const PaStreamParameters*,
                            const PaStreamParameters*, double, unsigned long frames,
                            PaStreamFlags, PaStreamCallback* cb, void* user) {
    ++opens;
    if (fail == 1) return paInvalidDevice;
    auto* stream = new FakeStream;
    stream->callback = cb; stream->user = user; stream->frames = frames;
    *out = static_cast<PaStream*>(stream);
    return paNoError;
}
EXPORT PaError Pa_StartStream(PaStream* opaque) {
    ++starts;
    if (fail == 2) return paInternalError;
    auto* stream = cast(opaque);
    stream->active = true;
    stream->worker = std::thread([stream] {
        float output[256]{};
        while (stream->active) {
            const PaStreamCallbackTimeInfo times{};
            ++callbacks;
            const int result = stream->callback(nullptr, output, stream->frames, &times, 0, stream->user);
            if (result != paContinue) stream->active = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    return paNoError;
}
EXPORT PaError Pa_StopStream(PaStream* opaque) {
    ++stops;
    auto* stream = cast(opaque);
    stream->active = false;
    if (stream->worker.joinable()) stream->worker.join(); // quiescence guarantee
    if (fail == 3) return paInternalError;
    return paNoError;
}
EXPORT PaError Pa_AbortStream(PaStream* opaque) {
    auto* stream = cast(opaque);
    stream->active = false;
    if (stream->worker.joinable()) stream->worker.join();
    return paNoError;
}
EXPORT PaError Pa_CloseStream(PaStream* opaque) {
    ++closes;
    auto* stream = cast(opaque);
    if (stream->worker.joinable()) {
        stream->active = false;
        stream->worker.join();
    }
    delete stream;
    return paNoError;
}
EXPORT PaError Pa_IsStreamActive(PaStream* stream) { return cast(stream)->active ? 1 : 0; }
EXPORT double Pa_GetStreamCpuLoad(PaStream*) { return 0; }
} // extern "C"
