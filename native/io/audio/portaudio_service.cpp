// SPDX-License-Identifier: GPL-3.0-only
#include "portaudio_service.h"
#include "../../ui/third_party/portaudio.h"
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
namespace {
struct Backend {
 void* library=nullptr;
 decltype(&Pa_Initialize) initialize=nullptr;decltype(&Pa_Terminate) terminate=nullptr;
 decltype(&Pa_GetDeviceCount) count=nullptr;decltype(&Pa_GetDeviceInfo) info=nullptr;
 decltype(&Pa_GetDefaultOutputDevice) default_device=nullptr;decltype(&Pa_GetErrorText) text=nullptr;
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
  LOAD(initialize,"Pa_Initialize");LOAD(terminate,"Pa_Terminate");LOAD(count,"Pa_GetDeviceCount");LOAD(info,"Pa_GetDeviceInfo");LOAD(default_device,"Pa_GetDefaultOutputDevice");LOAD(text,"Pa_GetErrorText");LOAD(open,"Pa_OpenStream");LOAD(start,"Pa_StartStream");LOAD(stop,"Pa_StopStream");LOAD(abort,"Pa_AbortStream");LOAD(close,"Pa_CloseStream");LOAD(active,"Pa_IsStreamActive");LOAD(load,"Pa_GetStreamCpuLoad");
#undef LOAD
  check(initialize());initialized=true;
 }
 ~Backend(){if(initialized)terminate();}
 void check(PaError result){if(result<0)throw std::runtime_error(text(result));}
};
struct Context {
 std::function<void()> stop_producers;
 ~Context(){if(stop_producers)stop_producers();}
 EngineOwner engine;std::vector<float> scratch;std::atomic<uint64_t> xruns{0},faults{0};
 Context(EngineOwner e,uint32_t block):engine(std::move(e)),scratch(block*2){}
};
int callback(const void*,void*output,unsigned long frames,const PaStreamCallbackTimeInfo*,PaStreamCallbackFlags flags,void*data){
 auto&c=*static_cast<Context*>(data);auto*out=static_cast<float*>(output);
 if(flags&paOutputUnderflow)c.xruns.fetch_add(1,std::memory_order_relaxed);
 if(!out||frames>c.scratch.size()/2){c.faults.fetch_add(1,std::memory_order_relaxed);if(out)std::fill_n(out,frames*2,0.f);return paAbort;}
 float*rows[]={c.scratch.data(),c.scratch.data()+c.scratch.size()/2};
 if(app_engine_render_channels(c.engine.get(),rows,2,uint32_t(frames))!=WS_OK){std::fill_n(out,frames*2,0.f);c.faults.fetch_add(1,std::memory_order_relaxed);return paAbort;}
 for(unsigned long i=0;i<frames;++i){out[2*i]=std::clamp(rows[0][i]*WS_OUTPUT_GAIN,-1.f,1.f);out[2*i+1]=std::clamp(rows[1][i]*WS_OUTPUT_GAIN,-1.f,1.f);}return paContinue;
}
}
void list_devices(const std::string&directory){Backend b(directory);int count=b.count();b.check(count);for(int i=0;i<count;++i){const auto*d=b.info(i);if(d&&d->maxOutputChannels>=2)std::cout<<i<<": "<<d->name<<" ("<<d->defaultSampleRate<<" Hz default)\n";}}
void play(EngineOwner engine,const std::string&directory,const std::string&name,double rate,uint32_t block,const std::atomic<bool>&quit,std::function<void()> stop_producers){
 struct StopOnError {std::function<void()>&stop;~StopOnError(){if(stop)stop();}} cleanup{stop_producers};
 Backend b(directory);auto context=std::make_unique<Context>(std::move(engine),block);context->stop_producers=stop_producers;int device=b.default_device();
 if(!name.empty()){size_t consumed=0;try{device=std::stoi(name,&consumed);}catch(const std::exception&){}if(consumed!=name.size()){
   device=paNoDevice;int count=b.count();b.check(count);for(int i=0;i<count;++i){const auto*d=b.info(i);if(d&&name==d->name){device=i;break;}}
 }}
 const auto*info=device>=0?b.info(device):nullptr;if(!info||info->maxOutputChannels<2)throw std::runtime_error("No matching stereo output device");
 PaStreamParameters output{device,2,paFloat32,info->defaultLowOutputLatency,nullptr};PaStream*stream=nullptr;
 b.check(b.open(&stream,nullptr,&output,rate,block,paNoFlag,callback,context.get()));
 PaError start=b.start(stream),run_status=paNoError;
 if(start>=0){while(!quit.load(std::memory_order_relaxed)){
   int active=b.active(stream);if(active<=0){run_status=active;break;}
   std::cerr<<"cpu="<<b.load(stream)*100<<"% xruns="<<context->xruns.load()<<" faults="<<context->faults.load()<<'\n';
   for(int tick=0;tick<10&&!quit.load(std::memory_order_relaxed);++tick)std::this_thread::sleep_for(std::chrono::milliseconds(100));
 }}
 if(stop_producers)stop_producers(); // producers join/drain while render still owns its engine.
 PaError stopped=b.stop(stream);if(stopped<0)stopped=b.abort(stream);PaError closed=b.close(stream);
 PaError terminated=b.terminate();b.initialized=false;
 if(terminated<0&&(stopped<0||closed<0)){
   // Keep callback state and its engine alive if the backend cannot prove stop.
   context.release();throw std::runtime_error("Audio shutdown failed; engine quarantined until process exit");
 }
 if(start<0)b.check(start);b.check(run_status);if(context->faults.load())throw std::runtime_error("Audio callback stopped after a render fault");
 b.check(terminated);
}
}
