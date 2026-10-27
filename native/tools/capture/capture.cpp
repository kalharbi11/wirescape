// SPDX-License-Identifier: GPL-3.0-only
// Manual process-loopback recorder. Never invoke this executable from CTest.
#define NOMINMAX
#include <windows.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <wrl.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
namespace {
void check(HRESULT hr, const char* operation) {
    if (FAILED(hr)) throw std::runtime_error(std::string(operation) + " HRESULT=" + std::to_string(static_cast<unsigned long>(hr)));
}
struct Unavailable : std::runtime_error { using std::runtime_error::runtime_error; };
void check_activation(HRESULT hr, const char* operation) {
    if (FAILED(hr)) throw Unavailable(std::string(operation) + " HRESULT=" + std::to_string(static_cast<unsigned long>(hr)));
}
struct Apartment {
    Apartment() { check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "COM initialization"); }
    ~Apartment() { CoUninitialize(); }
};
struct Event {
    HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Event() { if (!handle) throw std::runtime_error("capture event creation failed"); }
    ~Event() { if (handle) CloseHandle(handle); }
};
struct Stream {
    ComPtr<IAudioClient> client;
    bool started = false;
    void start() { check(client->Start(), "Start"); started = true; }
    void stop() { if (started) { check(client->Stop(), "Stop"); started = false; } }
    ~Stream() { if (started) client->Stop(); }
};
class Activation final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    Microsoft::WRL::FtmBase, IActivateAudioInterfaceCompletionHandler> {
public:
    AUDIOCLIENT_ACTIVATION_PARAMS config{};
    PROPVARIANT params{};
    HANDLE done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HRESULT result = E_PENDING;
    ComPtr<IAudioClient> client;
    ~Activation() { if (done) CloseHandle(done); }
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op) override {
        HRESULT activated = E_FAIL;
        ComPtr<IUnknown> unknown;
        result = op->GetActivateResult(&activated, &unknown);
        if (SUCCEEDED(result)) result = activated;
        if (SUCCEEDED(result)) result = unknown.As(&client);
        SetEvent(done);
        return S_OK;
    }
};
ComPtr<IAudioClient> process_capture() {
    auto handler = Microsoft::WRL::Make<Activation>();
    if (!handler || !handler->done) throw std::runtime_error("activation event unavailable");
    auto& config = handler->config;
    config.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    config.ProcessLoopbackParams.TargetProcessId = GetCurrentProcessId();
    config.ProcessLoopbackParams.ProcessLoopbackMode = PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;
    auto& params = handler->params;
    params.vt = VT_BLOB;
    params.blob.cbSize = sizeof(config);
    params.blob.pBlobData = reinterpret_cast<BYTE*>(&config);
    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    check_activation(ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
                                    &params, handler.Get(), &operation), "process loopback activation (requires build 20348+)");
    if (WaitForSingleObject(handler->done, 3000) != WAIT_OBJECT_0) throw std::runtime_error("activation timeout");
    check_activation(handler->result, "process loopback activation result");
    return handler->client;
}
std::string utf8(const wchar_t* value) {
    int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    std::string text(size, '\0');
    if (size) { WideCharToMultiByte(CP_UTF8, 0, value, -1, text.data(), size, nullptr, nullptr); text.pop_back(); }
    return text;
}
std::string json(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') out += '\\';
        if (c < 32) out += '?'; else out += static_cast<char>(c);
    }
    return out + '"';
}
}
int wmain(int argc, wchar_t** argv) {
    // Parse and validate BEFORE COM, endpoint enumeration, or stream activation.
    if (argc != 3 || std::wstring(argv[1]) != L"--confirm-audio") {
        std::cerr << "Manual use only: wirescape_capture --confirm-audio REPORT_DIRECTORY\n";
        return 2;
    }
    fs::path directory;
    try {
        directory = fs::canonical(argv[2]);
        if (fs::exists(directory / "native.json") || fs::exists(directory / "capture.f32")) throw std::runtime_error("refusing to overwrite an existing capture");
        auto relative = directory.lexically_relative(fs::weakly_canonical(fs::current_path() / "build_reports"));
        if (relative.empty() || relative == "." || relative.is_absolute()) throw std::runtime_error("invalid report directory");
        for (const auto& part : relative) if (part == "..") throw std::runtime_error("reports must remain under build_reports");
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
    std::string status = "application_fault", message, endpoint;
    uint64_t discontinuities = 0, timestamp_errors = 0, position_gaps = 0, starvations = 0, silent_packets = 0;
    uint64_t first_qpc = 0, last_qpc = 0, first_position = 0;
    uint32_t mix_rate = 0;
    std::vector<float> recorded;
    bool stopped = false;
    try {
        auto size = fs::file_size(directory / "reference.f32");
        if (size < 1024 * 8 || size > 48000 * 6 * 8 || size % 8) throw std::runtime_error("invalid reference bounds");
        std::vector<float> reference(static_cast<size_t>(size / 4));
        std::ifstream input(directory / "reference.f32", std::ios::binary);
        if (!input.read(reinterpret_cast<char*>(reference.data()), static_cast<std::streamsize>(size))) throw std::runtime_error("reference read failed");
        for (float value : reference) if (!std::isfinite(value) || std::abs(value) > 0.051f) throw std::runtime_error("reference exceeds low-level ceiling");
        status = "capture_failure";
        Apartment apartment;
        ComPtr<IMMDeviceEnumerator> enumerator;
        check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)), "enumerator");
        ComPtr<IMMDevice> device;
        const HRESULT endpoint_result = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (endpoint_result == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) throw Unavailable("default render endpoint absent");
        check(endpoint_result, "default render endpoint");
        LPWSTR id = nullptr;
        check(device->GetId(&id), "endpoint ID");
        endpoint = utf8(id); CoTaskMemFree(id);
        Event sample_ready; // Outlives both clients, including exception cleanup.
        Stream playback, capture;
        check(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                               reinterpret_cast<void**>(playback.client.GetAddressOf())), "render activation");
        WAVEFORMATEX* mix = nullptr;
        check(playback.client->GetMixFormat(&mix), "endpoint mix format");
        mix_rate = mix->nSamplesPerSec; CoTaskMemFree(mix);
        WAVEFORMATEX format{WAVE_FORMAT_IEEE_FLOAT, 2, 48000, 48000 * 8, 8, 32, 0};
        const DWORD convert = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
        check(playback.client->Initialize(AUDCLNT_SHAREMODE_SHARED, convert, 1000000, 0, &format, nullptr), "render Initialize");
        capture.client = process_capture();
        check(capture.client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | convert,
                                         200000, 0, &format, nullptr), "capture Initialize");
        check(capture.client->SetEventHandle(sample_ready.handle), "capture SetEventHandle");
        ComPtr<IAudioRenderClient> writer;
        ComPtr<IAudioCaptureClient> reader;
        check(playback.client->GetService(IID_PPV_ARGS(&writer)), "render service");
        check(capture.client->GetService(IID_PPV_ARGS(&reader)), "capture service");
        UINT32 capacity = 0;
        check(playback.client->GetBufferSize(&capacity), "render buffer size");
        const size_t frames = reference.size() / 2, maximum = 48000 * 8 * 2;
        recorded.reserve(maximum);
        size_t sent = 0;
        auto feed = [&]() {
            UINT32 padding = 0;
            check(playback.client->GetCurrentPadding(&padding), "render padding");
            if (padding > capacity) throw std::runtime_error("invalid render padding");
            if (playback.started && padding == 0 && sent < frames) ++starvations;
            UINT32 count = static_cast<UINT32>(std::min<size_t>(capacity - padding, frames - sent));
            if (count) {
                BYTE* data = nullptr;
                check(writer->GetBuffer(count, &data), "render GetBuffer");
                std::copy_n(reference.data() + sent * 2, count * 2, reinterpret_cast<float*>(data));
                check(writer->ReleaseBuffer(count, 0), "render ReleaseBuffer");
                sent += count;
            }
        };
        feed(); capture.start();
        const auto start = std::chrono::steady_clock::now();
        uint64_t expected_position = 0;
        bool have_packet = false;
        while (std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() < frames / 48000.0 + 0.75) {
            if (!playback.started && std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(250)) playback.start();
            if (playback.started) feed();
            // Bound each event wait so playback feeding and the overall deadline progress.
            const DWORD ready = WaitForSingleObject(sample_ready.handle, 5);
            if (ready == WAIT_TIMEOUT) continue;
            if (ready != WAIT_OBJECT_0) throw std::runtime_error("capture event wait failed");
            UINT32 packet = 0, packets = 0;
            check(reader->GetNextPacketSize(&packet), "capture packet size");
            while (packet) {
                if (++packets > 128 || packet > (maximum - recorded.size()) / 2) throw std::runtime_error("capture storage/CPU bound exceeded");
                BYTE* data = nullptr; UINT32 count = 0; DWORD flags = 0; UINT64 position = 0, qpc = 0;
                check(reader->GetBuffer(&data, &count, &flags, &position, &qpc), "capture GetBuffer");
                if ((count && !data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) || count > packet) { reader->ReleaseBuffer(count); throw std::runtime_error("unexpected capture packet growth"); }
                if (have_packet && flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ++discontinuities;
                if (flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) ++timestamp_errors;
                if (have_packet && position != expected_position) ++position_gaps;
                if (!have_packet) { first_qpc = qpc; first_position = position; }
                last_qpc = qpc; expected_position = position + count; have_packet = true;
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) { recorded.insert(recorded.end(), count * 2, 0.0f); ++silent_packets; }
                else recorded.insert(recorded.end(), reinterpret_cast<float*>(data), reinterpret_cast<float*>(data) + count * 2);
                check(reader->ReleaseBuffer(count), "capture ReleaseBuffer");
                check(reader->GetNextPacketSize(&packet), "capture packet size");
            }
        }
        playback.stop(); capture.stop(); stopped = true;
        if (sent != frames || recorded.empty()) throw std::runtime_error("incomplete playback or empty capture");
        std::ofstream output(directory / "capture.f32", std::ios::binary);
        output.write(reinterpret_cast<const char*>(recorded.data()), static_cast<std::streamsize>(recorded.size() * 4));
        if (!output) throw std::runtime_error("capture write failed");
        status = "captured";
    } catch (const Unavailable& e) { status = "unavailable"; message = e.what(); }
      catch (const std::exception& e) { message = e.what(); }
    std::ofstream report(directory / "native.json");
    report << "{\"status\":" << json(status) << ",\"message\":" << json(message)
           << ",\"isolation\":\"process_tree\",\"target_pid\":" << GetCurrentProcessId()
           << ",\"endpoint_id\":" << json(endpoint) << ",\"endpoint_mix_rate\":" << mix_rate
           << ",\"rate\":48000,\"channels\":2,\"frames\":" << recorded.size() / 2
           << ",\"first_qpc_100ns\":" << first_qpc << ",\"last_qpc_100ns\":" << last_qpc
           << ",\"first_device_position\":" << first_position << ",\"stopped_cleanly\":" << (stopped ? "true" : "false")
           << ",\"silent_packets\":" << silent_packets << ",\"transport_counters\":{\"discontinuities\":" << discontinuities
           << ",\"timestamp_errors\":" << timestamp_errors << ",\"position_gaps\":" << position_gaps
           << ",\"render_starvations\":" << starvations << "}}\n";
    return !report ? 2 : status == "captured" ? 0 : status == "unavailable" ? 3 : 2;
}
