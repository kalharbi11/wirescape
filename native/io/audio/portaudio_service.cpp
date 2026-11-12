// SPDX-License-Identifier: GPL-3.0-only
#include "portaudio_service.h"
#include "../../third_party/portaudio/portaudio.h"
#include "callback_render.h"
#include "engine_slot.h"
#include "wirescape/output_policy.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace wirescape::audio {
std::vector<std::string> installed_asio_drivers() {
    std::vector<std::string> result;
#ifdef _WIN32
    // Registry enumeration is read-only on the control thread. A missing hive
    // is ordinary (e.g. test runners and machines with no ASIO driver).
    HKEY key = nullptr;
    const auto opened = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0, KEY_READ | KEY_WOW64_64KEY, &key);
    if (opened != ERROR_SUCCESS) return result;
    for (DWORD index = 0; index < 128; ++index) {
        wchar_t name[256]{};
        DWORD length = 255;
        const auto status = RegEnumKeyExW(key, index, name, &length, nullptr, nullptr, nullptr, nullptr);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS || !length) continue;
        const int chars = int(length);
        const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, chars, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) continue;
        std::string utf8(size_t(bytes), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, int(length), utf8.data(), bytes, nullptr,
                                nullptr) != bytes)
            continue;
        result.push_back(std::move(utf8));
    }
    RegCloseKey(key);
#endif
    return result;
}
namespace {
struct Backend {
 void* library=nullptr;
 decltype(&Pa_Initialize) initialize=nullptr;decltype(&Pa_Terminate) terminate=nullptr;
 decltype(&Pa_GetDeviceCount) count=nullptr;decltype(&Pa_GetDeviceInfo) info=nullptr;
 decltype(&Pa_GetHostApiInfo) host_api = nullptr;
 decltype(&Pa_IsFormatSupported) format_supported = nullptr;
 decltype(&Pa_GetDefaultOutputDevice) default_device = nullptr;
 decltype(&Pa_GetErrorText) text = nullptr;
 decltype(&Pa_GetLastHostErrorInfo) last_host_error = nullptr;
 // pa_asio.h is not vendored; this optional export exists only in ASIO-enabled builds.
 using AsioBufferSizes = PaError (*)(PaDeviceIndex, long *, long *, long *, long *);
 AsioBufferSizes asio_buffer_sizes = nullptr;
 decltype(&Pa_OpenStream) open=nullptr;decltype(&Pa_StartStream) start=nullptr;
 decltype(&Pa_StopStream) stop=nullptr;decltype(&Pa_AbortStream) abort=nullptr;
 decltype(&Pa_CloseStream) close=nullptr;decltype(&Pa_IsStreamActive) active=nullptr;
 decltype(&Pa_GetStreamCpuLoad) load=nullptr;
 bool initialized=false;
 template<class T>void symbol(T&out,const char*name){
#ifdef _WIN32
  out=reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(library),name));
#else
  out=reinterpret_cast<T>(dlsym(library,name));
#endif
  if(!out)throw std::runtime_error(std::string("PortAudio symbol missing: ")+name);
 }
 explicit Backend(const std::string&directory){
#ifdef _WIN32
  for(const char*name:{"libportaudio64bit.dll","portaudio.dll"}){auto path=std::filesystem::path(directory)/name;library=LoadLibraryW(path.wstring().c_str());if(library)break;}
#else
  auto path=std::filesystem::path(directory)/"libportaudio.so.2";library=dlopen(path.c_str(),RTLD_NOW|RTLD_LOCAL);if(!library)library=dlopen("libportaudio.so.2",RTLD_NOW|RTLD_LOCAL);
#endif
  if(!library)throw std::runtime_error("PortAudio unavailable: install libportaudio2 on Linux or place the DLL beside the runner");
  // Resident through process exit, including a broken-backend shutdown path.
#define LOAD(member,name) symbol(member,name)
  LOAD(initialize, "Pa_Initialize");
  LOAD(terminate, "Pa_Terminate");
  LOAD(count, "Pa_GetDeviceCount");
  LOAD(info, "Pa_GetDeviceInfo");
  LOAD(host_api, "Pa_GetHostApiInfo");
  LOAD(format_supported, "Pa_IsFormatSupported");
  LOAD(default_device, "Pa_GetDefaultOutputDevice");
  LOAD(text, "Pa_GetErrorText");
#ifdef _WIN32
  last_host_error = reinterpret_cast<decltype(last_host_error)>(
      GetProcAddress(static_cast<HMODULE>(library), "Pa_GetLastHostErrorInfo"));
  asio_buffer_sizes = reinterpret_cast<AsioBufferSizes>(
      GetProcAddress(static_cast<HMODULE>(library), "PaAsio_GetAvailableBufferSizes"));
#else
  last_host_error = reinterpret_cast<decltype(last_host_error)>(dlsym(library, "Pa_GetLastHostErrorInfo"));
#endif
  LOAD(open, "Pa_OpenStream");
  LOAD(start, "Pa_StartStream");
  LOAD(stop, "Pa_StopStream");
  LOAD(abort, "Pa_AbortStream");
  LOAD(close, "Pa_CloseStream");
  LOAD(active, "Pa_IsStreamActive");
  LOAD(load, "Pa_GetStreamCpuLoad");
#undef LOAD
  check(initialize());initialized=true;
 }
 ~Backend(){if(initialized)terminate();}
 void check(PaError result, bool asio = false) {
     if (result == paInvalidSampleRate) throw RateUnsupported();
     if (result >= 0) return;
     std::string message = text(result) ? text(result) : "PortAudio error";
     if (asio && last_host_error) {
         const auto *host = last_host_error();
         if (host && host->errorText && *host->errorText) {
             message += ": ";
             message += host->errorText;
         }
     }
     if (message.size() > 200) message.resize(200);
     throw std::runtime_error(message);
 }
};
struct Context {
    std::function<void()> stop_producers;
    ~Context() {
        if (stop_producers) stop_producers();
    }
    double rate = 0;
    CallbackMetrics metrics;
    EngineOwner engine;
    std::vector<float> scratch;
    std::atomic<uint64_t> faults{0};
    // `engine` is control-owned; the callback renders only `live`, which it alone
    // writes after the stream starts, adopting exchanged engines at block starts.
    EngineSlot slot;
    AppEngine *live = nullptr;
    Context(EngineOwner e, uint32_t block) : engine(std::move(e)), scratch(size_t{block} * 2), live(engine.get()) {}
};
int callback(const void *, void *output, unsigned long frames, const PaStreamCallbackTimeInfo *,
             PaStreamCallbackFlags flags, void *data) {
    auto &c = *static_cast<Context *>(data);
    auto *out = static_cast<float *>(output);
    const auto size = uint32_t(c.scratch.size() / 2);
    c.live = c.slot.adopt(c.live);
    const auto now = app_engine_clock_ns();
    const bool ok = render_callback_block(c.live, c.scratch.data(), size, out, frames, now, c.faults);
    const auto end = app_engine_clock_ns();
    c.metrics.publish(end >= now ? end - now : 0, frames, c.rate, (flags & paOutputUnderflow) != 0);
    return ok ? paContinue : paAbort;
}
} // namespace
namespace {
std::vector<Device> enumerate(Backend &b, const DeviceQuery &query = {}) {
    int count = b.count();
    b.check(count);
    const int fallback = b.default_device();
    std::vector<RawDevice> raw;
    std::vector<Device> result;
    result.reserve(size_t(count));
    raw.reserve(size_t(count));
    for (int i = 0; i < count; ++i) {
        const auto *d = b.info(i);
        if (!d || d->maxOutputChannels == 0) continue;
        Device entry;
        entry.name = d->name;
        entry.index = i;
        entry.is_default = i == fallback;
        entry.output_channels = d->maxOutputChannels;
        entry.default_sample_rate = d->defaultSampleRate;
        if (const auto *api = b.host_api(d->hostApi)) entry.backend = api->name ? api->name : "Audio";
        entry.host_api = entry.backend.empty() ? "Audio" : entry.backend;
        entry.sample_rates.clear();
        entry.supported = d->maxOutputChannels >= 2;
        if (!entry.supported) entry.unsupported_reason = "mono-output";
        result.push_back(std::move(entry));
    }
    // Preserve published stereo-output ids when adding disabled mono outputs.
    std::stable_partition(result.begin(), result.end(), [](const Device &d) { return d.output_channels >= 2; });
    for (const auto &d : result) raw.push_back({d.name, d.host_api});
    auto ids = device_ids(raw);
    for (size_t i = 0; i < result.size(); ++i) {
        auto &entry = result[i];
        entry.id = std::move(ids[i]);
        if (!query.probe_rates || !entry.supported || entry.id == query.active_id ||
            (!query.target_id.empty() && entry.id != query.target_id))
            continue;
        const auto *d = b.info(entry.index);
        PaStreamParameters output{entry.index, 2, paFloat32, d->defaultLowOutputLatency, nullptr};
        const auto support = classify_rates({b.format_supported(nullptr, &output, 44100),
                                             b.format_supported(nullptr, &output, 48000),
                                             b.format_supported(nullptr, &output, 96000)},
                                            paInvalidSampleRate);
        entry.sample_rates = support.rates;
        if (support.known && support.rates.empty()) {
            entry.supported = false;
            entry.unsupported_reason = "rate-unsupported";
        }
    }
    return result;
}
} // namespace
void list_devices(const std::string &directory) {
    Backend b(directory);
    for (const auto &d : enumerate(b))
        std::cout << d.index << ": " << d.id << (d.is_default ? "  [default]" : "") << " ("
                  << b.info(d.index)->defaultSampleRate << " Hz default)\n";
}
std::vector<Device> devices(const std::string &directory, const DeviceQuery &query) {
    Backend b(directory);
    return enumerate(b, query);
}
struct Stream::Impl {
    std::unique_ptr<Backend> backend;
    std::unique_ptr<Context> context;
    PaStream *stream = nullptr;
    uint64_t retired_dropouts = 0;
    uint32_t frames = 0;
    Impl(EngineOwner &engine, const StreamSelection &selection, double rate, uint32_t block,
         std::function<void()> producers, bool allow_preferred_retry)
        : backend(std::make_unique<Backend>(selection.directory)), frames(block) {
        auto &b = *backend;
        int device = b.default_device();
        if (!selection.name.empty()) {
            const auto list = enumerate(b, {false, {}, {}});
            const int found = resolve_device(list, selection.name);
            if (found < 0) throw std::runtime_error("No unique audio device matches the selection");
            device = list[size_t(found)].index;
        }
        const auto *info = device >= 0 ? b.info(device) : nullptr;
        if (!info) throw std::runtime_error("No matching audio output device");
        if (info->maxOutputChannels < 2)
            throw std::runtime_error(info->maxOutputChannels == 0 ? "no-output" : "mono-output");
        PaStreamParameters output{device, 2, paFloat32, info->defaultLowOutputLatency, nullptr};
        const auto *api = b.host_api(info->hostApi);
        const bool is_asio = api && api->name && std::string(api->name) == "ASIO";
        b.check(b.format_supported(nullptr, &output, rate), is_asio);
        context = std::make_unique<Context>(EngineOwner(nullptr, app_engine_destroy), block);
        context->rate = rate;
        context->stop_producers = std::move(producers);
        context->engine = std::move(engine);
        context->live = context->engine.get();
        auto opened = b.open(&stream, nullptr, &output, rate, block, paNoFlag, callback, context.get());
        if (opened < 0) {
            engine = std::move(context->engine);
            context.reset();
            // Control thread, before any callback: report the driver's preferred
            // size once so the owner can reprepare its graph and retry exactly once.
            if (is_asio && allow_preferred_retry && opened != paInvalidSampleRate && b.asio_buffer_sizes) {
                long min = 0, max = 0, preferred = 0, granularity = 0;
                if (b.asio_buffer_sizes(device, &min, &max, &preferred, &granularity) == paNoError &&
                    preferred != long(block))
                    throw PreferredBufferSize(int64_t(preferred));
            }
            b.check(opened, is_asio);
        }
        auto started = b.start(stream);
        if (started < 0) {
            engine = retire();
            b.check(started, is_asio);
        }
    }
    EngineOwner retire() {
        if (!context) return EngineOwner(nullptr, app_engine_destroy);
        if (context->stop_producers) context->stop_producers();
        auto &b = *backend;
        auto stopped = b.stop(stream);
        if (stopped < 0) stopped = b.abort(stream);
        auto closed = b.close(stream);
        stream = nullptr;
        auto terminated = b.terminate();
        b.initialized = false;
        if (terminated < 0 && (stopped < 0 || closed < 0)) {
            [[maybe_unused]] auto *quarantined = context.release();
            throw ShutdownFailure("Audio shutdown failed; engine quarantined until process exit");
        }
        if (terminated < 0) std::cerr << "PortAudio termination reported failure after stream closed\n";
        if (context->faults.load()) std::cerr << "Audio callback stopped after render fault\n";
        retired_dropouts = context->metrics.underflows.load(std::memory_order_relaxed);
        auto owner = std::move(context->engine);
        context.reset();
        return owner;
    }
    ~Impl() {
        try {
            retire();
        } catch (const std::exception &e) {
            std::cerr << e.what() << '\n';
        }
    }
};
Stream::Stream(EngineOwner &e, const StreamSelection &selection, double rate, uint32_t block,
               std::function<void()> producers, bool allow_preferred_retry)
    : impl_(std::make_unique<Impl>(e, selection, rate, block, std::move(producers), allow_preferred_retry)) {}
Stream::~Stream() = default;
EngineOwner Stream::stop() { return impl_->retire(); }
bool Stream::exchange(EngineOwner &next, std::chrono::steady_clock::time_point deadline) {
    auto *context = impl_->context.get();
    if (!context || !next || !impl_->stream) return false;
    if (!context->slot.offer(next.get(), deadline)) return false;
    std::swap(context->engine, next); // `next` now owns the retired engine.
    return true;
}
uint32_t Stream::buffer_frames() const noexcept { return impl_->frames; }
bool Stream::running() const { return impl_->stream && impl_->backend->active(impl_->stream) > 0; }
double Stream::cpu_percent() const {
    return impl_->context ? impl_->context->metrics.read() : std::numeric_limits<double>::quiet_NaN();
}
uint64_t Stream::dropouts() const {
    if (impl_->context) return impl_->context->metrics.underflows.load(std::memory_order_relaxed);
    return impl_->retired_dropouts;
}
void play(EngineOwner engine,const std::string&directory,const std::string&name,double rate,uint32_t block,const std::atomic<bool>&quit,std::function<void()> stop_producers){
 struct StopOnError {std::function<void()>&stop;~StopOnError(){if(stop)stop();}} cleanup{stop_producers};
 Backend b(directory);auto context=std::make_unique<Context>(std::move(engine),block);context->stop_producers=stop_producers;int device=b.default_device();
 context->rate = rate;
 if(!name.empty()){size_t consumed=0;try{device=std::stoi(name,&consumed);}catch(const std::exception&){consumed=0;}if(consumed!=name.size()){
         const auto list = enumerate(b, {false, {}, {}});
         const int found = resolve_device(list, name);
         device = found < 0 ? paNoDevice : list[size_t(found)].index;
 }}
 const auto*info=device>=0?b.info(device):nullptr;if(!info||info->maxOutputChannels<2)throw std::runtime_error("No matching stereo output device");
 PaStreamParameters output{device,2,paFloat32,info->defaultLowOutputLatency,nullptr};PaStream*stream=nullptr;
 b.check(b.open(&stream,nullptr,&output,rate,block,paNoFlag,callback,context.get()));
 PaError start=b.start(stream),run_status=paNoError;
 if(start>=0){while(!quit.load(std::memory_order_relaxed)){
   int active=b.active(stream);if(active<=0){run_status=active;break;}
   std::cerr << "cpu=" << context->metrics.read()
             << "% xruns=" << context->metrics.underflows.load(std::memory_order_relaxed)
             << " faults=" << context->faults.load() << '\n';
   for(int tick=0;tick<10&&!quit.load(std::memory_order_relaxed);++tick)std::this_thread::sleep_for(std::chrono::milliseconds(100));
 }}
 if(stop_producers)stop_producers(); // producers join/drain while render still owns its engine.
 PaError stopped=b.stop(stream);if(stopped<0)stopped=b.abort(stream);PaError closed=b.close(stream);
 PaError terminated=b.terminate();b.initialized=false;
 if(terminated<0&&(stopped<0||closed<0)){
   // Keep callback state and its engine alive if the backend cannot prove stop.
   [[maybe_unused]] auto *quarantined = context.release();
   throw ShutdownFailure("Audio shutdown failed; engine quarantined until process exit");
 }
 if(start<0)b.check(start);b.check(run_status);if(context->faults.load())throw std::runtime_error("Audio callback stopped after a render fault");
 b.check(terminated);
}
}

