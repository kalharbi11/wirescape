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
using namespace wirescape::engine2;
using Clock=std::chrono::steady_clock;
static void require(bool ok){if(!ok)throw std::runtime_error("graph setup/render failed");}
static double number(const std::string&s){size_t used=0;double v=std::stod(s,&used);if(used!=s.size()||!std::isfinite(v))throw std::invalid_argument("invalid number");return v;}
struct Options{double rate=48000,seconds=10;uint32_t block=128;std::string graph="poly8",modules;bool budget=false;};
static Options parse(int argc,char**argv){
 Options o;
 for(int i=1;i<argc;++i){std::string key=argv[i];if(key=="--budget"){o.budget=true;continue;}
  if(i+1==argc)throw std::invalid_argument("missing argument");std::string value=argv[++i];
  if(key=="--rate")o.rate=number(value);
  else if(key=="--seconds")o.seconds=number(value);
  else if(key=="--block"){double n=number(value);if(n<1||n>4096||std::floor(n)!=n)throw std::invalid_argument("block must be 1..4096");o.block=static_cast<uint32_t>(n);}
  else if(key=="--graph")o.graph=value;
  else if(key=="--modules")o.modules=value;
  else throw std::invalid_argument("unknown option");
 }
 if(o.rate<8000||o.rate>384000||o.seconds<.001||o.seconds>3600)throw std::invalid_argument("rate 8000..384000; seconds .001..3600");
 if(std::ceil(o.rate*o.seconds/o.block)>5000000)throw std::invalid_argument("at most five million blocks per run");
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
 ModuleLoader loader(o.modules.c_str());auto e=graph(o,loader);
 size_t count=static_cast<size_t>(std::ceil(o.rate*o.seconds/o.block));
 std::vector<double>times(count);std::vector<float>buffer(o.block);float*out[]={buffer.data()};
 uint64_t allocations=0;double peak=0;
 auto render=[&](){int result;{allocation_watch::Scope watch;result=e->render(out,1,o.block);allocations+=watch.allocations();}require(result==WS2_OK);};
 for(int i=0;i<256;++i)render();
 double period=1e9*o.block/o.rate;size_t late=0;auto start=Clock::now();
 for(size_t i=0;i<count;++i){
  auto before=Clock::now();render();auto after=Clock::now();
  times[i]=std::chrono::duration<double,std::nano>(after-before).count();
  for(float sample:buffer){require(std::isfinite(sample));peak=std::max(peak,double(std::abs(sample)));}
  auto deadline=start+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(double(i+1)*o.block/o.rate));
  if(after>deadline)++late;std::this_thread::sleep_until(deadline);
 }
 require(allocations==0&&e->meter().faults==0&&peak>0);
 SoakStats s(times,period);
 std::printf("{\"graph\":\"%s\",\"sample_rate\":%.9g,\"block\":%u,\"seconds\":%.9g,\"blocks\":%zu,\"period_ns\":%.9g,\"render_ns\":{\"min\":%.9g,\"median\":%.9g,\"p99\":%.9g,\"p99_9\":%.9g,\"max\":%.9g},\"over_50_percent\":%zu,\"over_100_percent\":%zu,\"late_deadlines\":%zu,\"allocations\":%llu,\"faults\":%llu,\"peak\":%.9g}",o.graph.c_str(),o.rate,o.block,o.seconds,count,period,s.min,s.median,s.p99,s.p999,s.max,s.over_half,s.over_period,late,static_cast<unsigned long long>(allocations),static_cast<unsigned long long>(e->meter().faults),peak);
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
