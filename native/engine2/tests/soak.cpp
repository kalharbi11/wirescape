// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "soak_stats.h"
#include "../../tests2/module_loader.h"
#include "../../tests2/allocation_watch.h"
#include "../../modules2/vco/dsp.h"
#include "../../modules2/vcf/dsp.h"
#include "../../modules2/adsr/dsp.h"
#include "../../modules2/vca/dsp.h"
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <cstdint>
#ifdef WS_SOAK_DOCUMENT
#include "app_patch.h"
#include <QFile>
#endif
#ifndef _WIN32
#include <time.h>
#endif
using namespace wirescape::engine2;
using Clock=std::chrono::steady_clock;
static void require(bool ok){if(!ok)throw std::runtime_error("graph setup/render failed");}
static uint64_t thread_cpu_ns(){
#ifdef _WIN32
 // GetThreadTimes is too coarse for per-block classification.
 return 0;
#else
 timespec t{};require(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0);return uint64_t(t.tv_sec)*1000000000ULL+uint64_t(t.tv_nsec);
#endif
}
static double number(const std::string&s){size_t used=0;double v=std::stod(s,&used);if(used!=s.size()||!std::isfinite(v))throw std::invalid_argument("invalid number");return v;}
struct Options{double rate=48000,seconds=10;uint32_t block=128;std::string graph="poly8",modules,patch;bool budget=false;};
static Options parse(int argc,char**argv){
 Options o;
 for(int i=1;i<argc;++i){std::string key=argv[i];if(key=="--budget"){o.budget=true;continue;}
  if(i+1==argc)throw std::invalid_argument("missing argument");std::string value=argv[++i];
  if(key=="--rate")o.rate=number(value);
  else if(key=="--seconds")o.seconds=number(value);
  else if(key=="--block"){double n=number(value);if(n<1||n>4096||std::floor(n)!=n)throw std::invalid_argument("block must be 1..4096");o.block=static_cast<uint32_t>(n);}
  else if(key=="--graph")o.graph=value;
  else if(key=="--modules")o.modules=value;
  else if(key=="--patch")o.patch=value;
  else throw std::invalid_argument("unknown option");
 }
 if(o.rate<8000||o.rate>384000||o.seconds<.001||o.seconds>3600)throw std::invalid_argument("rate 8000..384000; seconds .001..3600");
 if(std::ceil(o.rate*o.seconds/o.block)>5000000)throw std::invalid_argument("at most five million blocks per run");
 if(!o.patch.empty()){if(o.budget||o.graph!="poly8")throw std::invalid_argument("--patch cannot combine with --graph or --budget");o.graph="document";}
 return o;
}
static uint32_t voices(const std::string&graph){
 if(graph=="mono")return 1;if(graph=="poly8")return 8;
 if(graph.rfind("voices:",0)!=0)throw std::invalid_argument("unknown graph");
 auto suffix=graph.substr(7);if(suffix.empty()||suffix.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("voice count must be digits");
 double n=number(suffix);if(n<1||n>16||std::floor(n)!=n)throw std::invalid_argument("voices must be 1..16");return static_cast<uint32_t>(n);
}
static std::unique_ptr<Engine> graph(const Options&o,ModuleLoader&loader){
 Config c;c.sample_rate=o.rate;c.max_block=o.block;c.max_channels=16;
 auto e=std::make_unique<Engine>(c);
 if(o.graph=="six_essentials_shim"){
  if(o.modules.empty())throw std::invalid_argument("shim graph requires --modules directory");
  const char*names[]={"noise","clock","function_envelope","slew","vcf","ladder","vca","output"};
  for(uint32_t i=0;i<8;++i)require(e->add_abi1(i+1,loader.abi1(names[i])));
  const uint32_t edges[][4]={{1,0,5,0},{5,1,6,0},{6,0,7,0},{2,0,3,0},{3,0,4,0},{4,0,7,1},{7,0,8,0}};
  for(auto&a:edges)require(e->connect(a[0],a[1],a[2],a[3]));require(e->output(8,0,0));require(e->prepare());
  struct Param{uint32_t node,index;float value;};
  const Param params[]={{1,0,.2f},{2,0,137},{2,1,1},{2,2,.5f},{2,3,1},{3,0,.01f},{3,1,.1f},{3,2,0},{4,0,.02f},{4,1,.02f},{5,0,1800},{5,1,.2f},{6,0,3500},{6,1,.2f},{6,2,.5f},{7,0,1}};
  for(auto&p:params)require(e->set_parameter(p.node,p.index,p.value));
 }else{
  uint32_t n=voices(o.graph);
  require(e->add_node(1,ws::Module<ws2ports::Vco>::descriptor()));
  require(e->add_node(2,ws::Module<ws2ports::Vcf>::descriptor()));
  require(e->add_node(3,ws::Module<ws2ports::Adsr>::descriptor()));
  require(e->add_node(4,ws::Module<ws2ports::Vca>::descriptor()));
  for(uint32_t id=1;id<=4;++id)require(e->set_channels(id,n));
  require(e->connect(1,0,2,0));require(e->connect(2,1,4,0));require(e->connect(3,0,4,1));require(e->output(4,0,0));
  require(e->register_producer()==0);require(e->prepare());
  for(uint32_t ch=0;ch<n;++ch){ws2_event ev{};ev.struct_size=sizeof(ev);ev.type=WS2_NOTE_ON;ev.channel=ch;ev.key=48+ch;ev.note_id=ch+1;ev.source_id=1;ev.value=1;
   require(e->enqueue(0,1,WS2_DIRECT_PORT,ev,0)==WS2_OK);require(e->enqueue(0,3,WS2_DIRECT_PORT,ev,0)==WS2_OK);
  }
 }
 return e;
}
static bool run(const Options&o){
 ModuleLoader loader(o.modules.c_str());auto builtin=o.patch.empty()?graph(o,loader):nullptr;Engine*e=builtin.get();
#ifdef WS_SOAK_DOCUMENT
 std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>app(nullptr,app_engine_destroy);
 if(!o.patch.empty()){
  using namespace wirescape::document;
  ws_engine_config config{sizeof(config),o.rate,o.block,64,8192};
  std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>catalog(app_engine_create(&config),app_engine_destroy);require(bool(catalog));
  QFile file(QString::fromStdString(o.patch));if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("cannot read document patch");
  auto patch=read_patch(file.readAll(),app_catalog(catalog.get()));QString error;
  app.reset(build_app_patch(catalog.get(),patch,config,error));if(!app)throw std::runtime_error(error.toStdString());e=&app_engine_native(app.get());
 }
#else
 if(!o.patch.empty())throw std::runtime_error("document soak requires Qt Core development files at build time");
#endif
 size_t count=static_cast<size_t>(std::ceil(o.rate*o.seconds/o.block));
 std::vector<double>times(count),cpu_times(count);std::vector<float>buffer(o.block);float*out[]={buffer.data()};
 uint64_t allocations=0;double peak=0;
 auto render=[&](){int result;{allocation_watch::Scope watch;
#ifdef WS_SOAK_DOCUMENT
  result=app?app_engine_render(app.get(),buffer.data(),o.block):e->render(out,1,o.block);
#else
  result=e->render(out,1,o.block);
#endif
  allocations+=watch.allocations();}require(result==WS2_OK);};
 for(int i=0;i<256;++i)render();
 double period=1e9*o.block/o.rate;size_t late=0;auto start=Clock::now();
 for(size_t i=0;i<count;++i){
  auto cpu_before=thread_cpu_ns();auto before=Clock::now();render();auto after=Clock::now();auto cpu_after=thread_cpu_ns();
  times[i]=std::chrono::duration<double,std::nano>(after-before).count();
  cpu_times[i]=double(cpu_after-cpu_before);
  for(float sample:buffer){require(std::isfinite(sample));peak=std::max(peak,double(std::abs(sample)));}
  auto deadline=start+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(double(i+1)*o.block/o.rate));
  if(after>deadline)++late;std::this_thread::sleep_until(deadline);
 }
 require(allocations==0&&e->meter().faults==0&&peak>0);
#ifndef _WIN32
 auto preempted=preempted_blocks(times,cpu_times,period);
#endif
 SoakStats s(times,period);
 std::printf("{\"graph\":\"%s\",\"sample_rate\":%.9g,\"block\":%u,\"seconds\":%.9g,\"blocks\":%zu,\"period_ns\":%.9g,\"render_ns\":{\"min\":%.9g,\"median\":%.9g,\"p99\":%.9g,\"p99_9\":%.9g,\"max\":%.9g},",o.graph.c_str(),o.rate,o.block,o.seconds,count,period,s.min,s.median,s.p99,s.p999,s.max);
#ifdef _WIN32
 std::printf("\"cpu_clock\":\"unsupported\",\"cpu_ns\":null,\"preempted\":null,");
#else
 SoakStats cpu(cpu_times,period);
 std::printf("\"cpu_clock\":\"CLOCK_THREAD_CPUTIME_ID\",\"cpu_ns\":{\"min\":%.9g,\"median\":%.9g,\"p99\":%.9g,\"p99_9\":%.9g,\"max\":%.9g},\"preempted\":%zu,",cpu.min,cpu.median,cpu.p99,cpu.p999,cpu.max,preempted);
#endif
 std::printf("\"over_50_percent\":%zu,\"over_100_percent\":%zu,\"late_deadlines\":%zu,\"allocations\":%llu,\"faults\":%llu,\"peak\":%.9g}",s.over_half,s.over_period,late,static_cast<unsigned long long>(allocations),static_cast<unsigned long long>(e->meter().faults),peak);
 return s.p99<period*.5;
}
int main(int argc,char**argv){
 try{auto o=parse(argc,argv);
  if(o.budget){unsigned largest=0;std::printf("{\"budget_runs\":[");
   for(unsigned n=1;n<=16;++n){if(n>1)std::putchar(',');o.graph="voices:"+std::to_string(n);if(run(o))largest=n;}
   std::printf("],\"largest_voices_p99_under_50_percent\":%u}\n",largest);
  }else{run(o);std::putchar('\n');}return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"wirescape_soak: %s\n",e.what());return 1;}
}
