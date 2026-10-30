// SPDX-License-Identifier: GPL-3.0-only
#include "play.h"
#include "midi_host.h"
#include "runtime_patch.h"
#include "../io/audio/portaudio_service.h"
#include "wirescape/output_policy.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
std::atomic<bool> quit{false};
static_assert(ATOMIC_BOOL_LOCK_FREE==2,"Signal handler requires lock-free stop flag");
void interrupt(int){quit.store(true,std::memory_order_relaxed);}
void u16(std::ofstream&out,uint16_t v){out.put(char(v));out.put(char(v>>8));}
void u32(std::ofstream&out,uint32_t v){u16(out,uint16_t(v));u16(out,uint16_t(v>>16));}
uint32_t integer(const std::string&text,uint32_t maximum){size_t used=0;auto n=std::stoul(text,&used);if(used!=text.size()||!n||n>maximum)throw std::runtime_error("Option outside range");return uint32_t(n);}
}
int wirescape_play(int argc,char**argv){
 std::string midiName;bool confirmMidi=false;
 std::string patch,device,output="build/play-offline.wav",directory=std::filesystem::absolute(argv[0]).parent_path().string();
 uint32_t rate=48000,block=128,seconds=10;bool confirm=false,list=false,pace=false;
 for(int i=2;i<argc;++i){std::string arg=argv[i];
  if(arg=="--confirm-audio")confirm=true;
  else if(arg=="--list-devices")list=true;
  else if(arg=="--pace")pace=true;
  else if(arg=="--confirm-midi")confirmMidi=true;
  else if(arg=="--confirm-lan")throw std::runtime_error("MIDI/OSC service hooks await J4/J5");
  else if(arg=="--device"||arg=="--rate"||arg=="--block"||arg=="--output"||arg=="--seconds"||arg=="--modules"||arg=="--midi"||arg=="--osc-port"){
   if(++i>=argc)throw std::runtime_error("Missing option value");std::string value=argv[i];
   if(arg=="--midi")midiName=value;else if(arg=="--device")device=value;else if(arg=="--rate")rate=integer(value,192000);else if(arg=="--block")block=integer(value,4096);else if(arg=="--output")output=value;else if(arg=="--seconds")seconds=integer(value,600);else if(arg=="--modules")directory=value;
   else throw std::runtime_error("MIDI/OSC needs its explicit confirmation flag and J4/J5 service");
  }else if(!arg.empty()&&arg[0]!='-'&&patch.empty())patch=arg;else throw std::runtime_error("Unknown play argument: "+arg);
 }
 if(!midiName.empty()&&!confirmMidi)throw std::runtime_error("--midi requires explicit confirmation with --confirm-midi");
 if(list){wirescape::audio::list_devices(directory);return 0;}
 if(patch.empty())throw std::runtime_error("play requires a native v1/v2 patch.json");
 std::ifstream input(patch,std::ios::binary);if(!input)throw std::runtime_error("Cannot read patch");
 std::string bytes(wirescape::document::RuntimePatchLimit+1,'\0');input.read(bytes.data(),std::streamsize(bytes.size()));bytes.resize(size_t(input.gcount()));
 if(bytes.size()>wirescape::document::RuntimePatchLimit)throw std::runtime_error("Patch exceeds 2 MiB limit");
 ws_engine_config config{sizeof(config),double(rate),block,64,2048};
 wirescape::audio::EngineOwner catalog(app_engine_create(&config),app_engine_destroy);
 if(!catalog)throw std::runtime_error("Cannot create catalogue");
 if(std::filesystem::is_directory(directory))for(const auto&entry:std::filesystem::directory_iterator(directory)){
  auto name=entry.path().filename().string();
#ifdef _WIN32
  bool module=name.rfind("ws_",0)==0&&entry.path().extension()==".dll";
#else
  bool module=name.rfind("libws_",0)==0&&entry.path().extension()==".so";
#endif
  if(module&&app_engine_load_module(catalog.get(),entry.path().string().c_str())!=WS_OK)throw std::runtime_error("Cannot load ABI1 module: "+name);
 }
 std::string error;wirescape::audio::EngineOwner engine(wirescape::document::load_runtime_patch(bytes,catalog.get(),config,error),app_engine_destroy);
 if(!engine)throw std::runtime_error(error);
 wirescape::midi::Host midi(confirmMidi);
 if(confirmMidi){midi.restore(bytes);if(!midiName.empty())midi.select({midiName,""},true);if(!midi.start(engine.get(),bytes,error))throw std::runtime_error(error);}
 quit=false;auto previous=std::signal(SIGINT,interrupt);
 if(confirm){try{wirescape::audio::play(std::move(engine),directory,device,rate,block,quit,[&midi]{midi.stop();});}catch(...){std::signal(SIGINT,previous);throw;}std::signal(SIGINT,previous);return 0;}
 // No PortAudio code is initialized on the default path.
 const uint64_t frames=uint64_t(rate)*seconds;std::filesystem::path path(output);if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
 if(std::filesystem::absolute(path).lexically_normal()==std::filesystem::absolute(patch).lexically_normal())throw std::runtime_error("Offline WAV cannot overwrite its patch");
 std::ofstream wav(output,std::ios::binary);if(!wav)throw std::runtime_error("Cannot create offline WAV");
 wav.write("RIFF",4);u32(wav,uint32_t(36+frames*4));wav.write("WAVEfmt ",8);u32(wav,16);u16(wav,1);u16(wav,2);u32(wav,rate);u32(wav,rate*4);u16(wav,4);u16(wav,16);wav.write("data",4);u32(wav,uint32_t(frames*4));
 std::vector<float>left(block),right(block);float*rows[]={left.data(),right.data()};std::vector<double>times;times.reserve(size_t((frames+block-1)/block));
 double peak=0,sum=0;uint64_t rendered=0,overruns=0;const double period=double(block)/rate*1e6;auto epoch=std::chrono::steady_clock::now();
 for(uint64_t at=0;at<frames&&!quit.load(std::memory_order_relaxed);){uint32_t n=uint32_t(std::min<uint64_t>(block,frames-at));auto begin=std::chrono::steady_clock::now();
  if(app_engine_render_channels(engine.get(),rows,2,n)!=WS_OK)throw std::runtime_error("Offline render failed");
  double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();times.push_back(us);overruns+=us>double(n)/rate*1e6;
  for(uint32_t i=0;i<n;++i)for(float value:{left[i],right[i]}){value=std::clamp(value*WS_OUTPUT_GAIN,-1.f,1.f);peak=std::max(peak,double(std::abs(value)));sum+=double(value)*value;u16(wav,uint16_t(int16_t(std::lround(value*32767))));}
  at+=n;rendered=at;
  if(pace)std::this_thread::sleep_until(epoch+std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(double(at)/rate)));
 }
 std::signal(SIGINT,previous);if(!wav)throw std::runtime_error("WAV write failed");
 if(rendered!=frames){wav.seekp(4);u32(wav,uint32_t(36+rendered*4));wav.seekp(40);u32(wav,uint32_t(rendered*4));}
 std::sort(times.begin(),times.end());auto percentile=[&](double p){return times.empty()?0.:times[size_t(std::ceil(p*static_cast<double>(times.size()-1)))];};
 std::cout<<"{\"offline\":true,\"loader\":\"runtime_patch\",\"frames\":"<<rendered<<",\"rate\":"<<rate<<",\"block\":"<<block<<",\"blocks\":"<<times.size()<<",\"peak\":"<<peak<<",\"rms\":"<<(rendered?std::sqrt(sum/(static_cast<double>(rendered)*2)):0)<<",\"period_us\":"<<period<<",\"p50_us\":"<<percentile(.5)<<",\"p95_us\":"<<percentile(.95)<<",\"p99_us\":"<<percentile(.99)<<",\"worst_us\":"<<percentile(1)<<",\"overruns\":"<<overruns<<"}\n";
 std::cerr<<"Offline WAV: "<<output<<'\n';return 0;
}
