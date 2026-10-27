// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "wirescape_sdk.h"
#include "../../tests2/test_modules.h"
#include "../../modules2/lfo/dsp.h"
#include "../../tests2/allocation_watch.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#if defined(_MSC_VER)&&defined(_DEBUG)
#include <crtdbg.h>
#endif
using namespace wirescape::engine2;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"engine2 check line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
struct Source {
 static constexpr ws::Info info{"test.source","Source",1};
 static constexpr std::array<ws2_port,0>inputs{};
 static constexpr std::array outputs{ws::port("out","Out",WS2_CV_BI)};
 static constexpr std::array params{ws::param("value","Value",-1,1,1)};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(b.param(0).constant());for(uint32_t c=0;c<b.channels();++c)for(uint32_t i=0;i<(b.param(0).constant()?1u:b.frames());++i)b.out(0).ch(c)[i]=b.param(0).value(c,i)+.1f*c;}
};
template<int Policy>struct Pass {
 static constexpr ws::Info info{"test.pass","Pass",1};
 static constexpr std::array inputs{ws::port("in","In",WS2_AUDIO,Policy,.25f)};
 static constexpr std::array outputs{ws::port("out","Out",WS2_AUDIO)};
 static constexpr std::array params{ws::param("gain","Gain",0,1,1)};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());bool constant=b.in(0).constant()&&b.param(0).constant();b.out(0).mark_constant(constant);for(uint32_t c=0;c<b.channels();++c)for(uint32_t i=0;i<(constant?1u:b.frames());++i)b.out(0).ch(c)[i]=b.in(0).value(c,i)*b.param(0).value(c,i);}
};
struct NonfiniteSource:Source {
 void process(ws::Block&b){Source::process(b);b.out(0).ch(0)[0]=std::nanf("");}
};
static void producer_validation(){
 Engine e;CHECK(e.add_node(1,ws::Module<NonfiniteSource>::descriptor()));CHECK(e.add_node(2,ws::Module<Pass<WS2_POLY>>::descriptor()));CHECK(e.connect(1,0,2,0));CHECK(e.prepare());
 float out[256]{};float*outputs[]={out};CHECK(e.render(outputs,1,256)==WS2_OK);CHECK(e.meter().faults==1);CHECK(e.output_constant(1,0));CHECK(e.output_sample(1,0,0,0)==0);CHECK(e.output_sample(2,0,0,0)==0);
 float finite[]={0.f,-0.f,1.f,-2.f,3.e38f};CHECK(ws::finite_samples(finite,5));
 finite[4]=std::nanf("");CHECK(!ws::finite_samples(finite,5));finite[4]=INFINITY;CHECK(!ws::finite_samples(finite,5));
 // 1 and 2 are finite although their exponent bits OR to the Inf/NaN pattern.
 CHECK(ws::finite_samples(nullptr,0));
 std::array<float,258>samples{};
 for(uint32_t shift:{0u,1u})for(uint32_t count:{1u,2u,3u,4u,7u,17u,256u,257u}){
  for(uint32_t i=0;i<count;++i)samples[shift+i]=(i&1)?2.f:1.f;
  CHECK(ws::finite_samples(samples.data()+shift,count));
  for(float invalid:{std::nanf(""),INFINITY,-INFINITY})for(uint32_t index:{0u,count/2,count-1}){
   const float saved=samples[shift+index];samples[shift+index]=invalid;
   CHECK(!ws::finite_samples(samples.data()+shift,count));samples[shift+index]=saved;
  }
 }

}
struct AddressSource:Source {
 static inline const float*written=nullptr;
 void process(ws::Block&b){written=b.out(0).ch(0);Source::process(b);}
};
template<int Policy>struct AddressPass:Pass<Policy> {
 static inline const float*read=nullptr;
 void process(ws::Block&b){read=b.in(0).ch(0).p;Pass<Policy>::process(b);}
};
static void zero_copy(){
 for(uint32_t channels:{1u,4u}){
  Engine e;CHECK(e.add_node(1,ws::Module<AddressSource>::descriptor()));CHECK(e.add_node(2,ws::Module<AddressPass<WS2_POLY>>::descriptor()));CHECK(e.add_node(3,ws::Module<AddressPass<WS2_SUM>>::descriptor()));CHECK(e.set_channels(1,channels));CHECK(e.connect(1,0,2,0));CHECK(e.connect(1,0,3,0));CHECK(e.prepare());
  float out[17]{};float*outputs[]={out};CHECK(e.render(outputs,1,17)==WS2_OK);
  CHECK(AddressSource::written==AddressPass<WS2_POLY>::read);
  CHECK((AddressSource::written==AddressPass<WS2_SUM>::read)==(channels==1));
  CHECK(e.output_sample(2,0,channels-1,0)==1.f+.1f*(channels-1));
 }
}
struct Feedback {
 static constexpr ws::Info info{"test.feedback","Feedback",1};
 static constexpr std::array inputs{ws::port("in","In")};
 static constexpr std::array outputs{ws::port("out","Out")};
 static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(1);b.out(0).mark_constant(false);for(uint32_t i=0;i<b.frames();++i)b.out(0).ch(0)[i]=.5f*b.in(0).value(0,i)+(b.frame_position()+i==0?1.f:0.f);}
};
struct Reporter {
 static constexpr ws::Info info{"test.reporter","Reporter",1};
 static constexpr std::array inputs{ws::port("in","In")};
 static constexpr std::array outputs{ws::port("out","Out")};
 static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(1);b.out(0).mark_constant();b.out(0).ch(0)[0]=static_cast<float>(b.frames());}
};
struct Receiver {
 static constexpr ws::Info info{"test.receiver","Receiver",1};
 static constexpr std::array inputs{ws::port("events","Events",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT)};
 static constexpr std::array outputs{ws::port("out","Out")};
 static constexpr std::array params{ws::param("gain","Gain",0,1,1)};
 float held=0;
 void prepare(const ws::Setup&){}void reset(){held=0;}
 void process(ws::Block&b){b.out(0).set_channels(1);b.out(0).mark_constant(false);b.split([&](uint32_t a,uint32_t z){for(uint32_t i=a;i<z;++i)b.out(0).ch(0)[i]=held*b.param(0).value(0,i);},[&](const ws2_event&e){if(e.type==WS2_NOTE_ON)held=e.value;else if(e.type==WS2_NOTE_OFF||e.type==WS2_NOTE_CHOKE)held=0;});}
};
struct Echo {
 static constexpr ws::Info info{"test.echo","Echo",1};
 static constexpr std::array inputs{ws::port("events","Events",WS2_EV_MESSAGE,WS2_POLY,0,nullptr,WS2_EVENT)};
 static constexpr std::array outputs{ws::port("events","Events",WS2_EV_MESSAGE,WS2_POLY,0,nullptr,WS2_EVENT)};
 static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(0);for(auto&e:b.events())b.emit(0,e);}
};
struct BadEmitter:Echo {
 void process(ws::Block&b){b.out(0).set_channels(0);auto e=ws2_event{};e.struct_size=sizeof(e);e.type=WS2_NOTE_ON;e.value2=std::nanf("");CHECK(!b.emit(0,e));e.value2=0;e.ump_words=5;CHECK(!b.emit(0,e));}
};
static void invalid_events(){
 Engine e;CHECK(e.add_node(1,ws::Module<BadEmitter>::descriptor()));CHECK(e.register_producer()==0);CHECK(e.prepare());
 auto bad=ws2_event{};bad.struct_size=sizeof(bad);bad.type=WS2_NOTE_ON;bad.value2=std::nanf("");CHECK(e.enqueue(0,1,0,bad,0)==WS2_INVALID);bad.value2=0;bad.ump_words=5;CHECK(e.enqueue(0,1,0,bad,0)==WS2_INVALID);
 float out[17]{};float*outputs[]={out};CHECK(e.render(outputs,1,17)==WS2_OK);Outgoing outgoing;CHECK(!e.pop_egress(outgoing));CHECK(e.meter().events==0);
}
struct ParamProbe {
 static constexpr ws::Info info{"test.param","Parameter",1};
 static constexpr std::array<ws2_port,0>inputs{};
 static constexpr std::array outputs{ws::port("out","Out")};
 static constexpr std::array params{ws::param("value","Value",1,100,10,"",WS2_MODULATABLE|WS2_PER_VOICE,WS2_LOG)};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(b.param(0).constant());for(uint32_t c=0;c<b.channels();++c)for(uint32_t i=0;i<(b.param(0).constant()?1u:b.frames());++i)b.out(0).ch(c)[i]=b.param(0).value(c,i);}
};
template<uint32_t Taper>struct CachedTaperProbe:ParamProbe {
 static constexpr std::array params{ws::param("value","Value",1,100,10,"",WS2_MODULATABLE,Taper)};
};
template<uint32_t Taper>static void cached_taper(){
 Engine e;CHECK(e.add_node(1,ws::Module<Source>::descriptor()));CHECK(e.add_node(2,ws::Module<CachedTaperProbe<Taper>>::descriptor()));CHECK(e.modulate(1,0,2,0,.125f));CHECK(e.prepare());
 float out[256]{};float*outputs[]={out};CHECK(e.render(outputs,1,256)==WS2_OK);
 const auto&p=CachedTaperProbe<Taper>::params[0];float expected=static_cast<float>(ws::to_plain(p,ws::to_normal(p,p.initial)+.125f));
 CHECK(e.output_sample(2,0,0,0)==expected);
}
struct LatencyProbe:Pass<WS2_POLY>{uint32_t latency_frames()const{return 7;}};
static ws2_event event(uint32_t type,float value=1,uint32_t index=0){ws2_event e{};e.struct_size=sizeof(e);e.type=type;e.value=value;e.index=index;e.note_id=1;e.channel=0;e.key=60;e.source_id=1;return e;}
static void render(Engine&e,uint32_t frames=256){std::array<float,4096>a{},b{};float*out[]={a.data(),b.data()};CHECK(e.render(out,2,frames)==WS2_OK);}
static Config cfg(){Config c;c.ring_capacity=256;c.future_capacity=1024;return c;}
static void signals(){
 Engine e(cfg());CHECK(e.add_node(1,ws::Module<Source>::descriptor()));CHECK(e.add_node(2,ws::Module<Pass<WS2_POLY>>::descriptor()));CHECK(e.add_node(3,ws::Module<Pass<WS2_SUM>>::descriptor()));CHECK(e.add_node(4,ws::Module<Pass<WS2_FIRST>>::descriptor()));
 CHECK(e.connect(1,0,2,0));CHECK(e.connect(1,0,3,0));CHECK(e.connect(1,0,4,0));CHECK(e.set_channels(1,4));CHECK(e.output(2,0,0));CHECK(e.output(2,0,1));CHECK(e.prepare());render(e);
 CHECK(e.output_channels(2,0)==4);CHECK(e.output_channels(3,0)==1);CHECK(std::abs(e.output_sample(3,0,0,0)-4.6f)<1e-6);CHECK(e.output_sample(4,0,0,0)==1);CHECK(e.output_constant(2,0));
 {allocation_watch::Scope watch;for(uint32_t n:{1u,16u,3u}){CHECK(e.set_channels(1,n));render(e,17);CHECK(e.output_channels(2,0)==n);}e.reset();CHECK(watch.allocations()==0);}
 CHECK(e.set_channels(1,0));render(e,1);CHECK(e.output_sample(2,0,0,0)==.25f);
}
static std::vector<float>loop(uint32_t delay,bool reverse){Config c=cfg();c.max_block=128;c.feedback_block=delay;Engine e(c);
 if(reverse){CHECK(e.add_node(2,ws::Module<Reporter>::descriptor()));CHECK(e.add_node(1,ws::Module<Feedback>::descriptor()));}
 else{CHECK(e.add_node(1,ws::Module<Feedback>::descriptor()));CHECK(e.add_node(2,ws::Module<Reporter>::descriptor()));}
 CHECK(e.connect(1,0,1,0));CHECK(e.connect(1,0,2,0));CHECK(e.output(1,0,0));CHECK(e.prepare());CHECK(e.feedback_edges()==1);
 std::vector<float>result(256);std::array<float,128>right{};
 for(uint32_t off=0;off<256;off+=128){float*out[]={result.data()+off,right.data()};CHECK(e.render(out,2,128)==WS2_OK);CHECK(e.output_sample(2,0,0,0)==128);}
 for(uint32_t i=0;i<256;++i){float expected=i%delay?0.f:std::ldexp(1.f,-static_cast<int>(i/delay));if(std::fpclassify(expected)==FP_SUBNORMAL)expected=0;CHECK(result[i]==expected);}return result;}
static void events(){
 Engine e(cfg());for(uint32_t i=1;i<=3;++i)CHECK(e.add_node(i,ws::Module<Echo>::descriptor()));CHECK(e.add_node(4,ws::Module<Receiver>::descriptor()));
 for(uint32_t i=1;i<=3;++i)CHECK(e.connect(i,0,4,0));for(int i=0;i<3;++i)CHECK(e.register_producer()==i);CHECK(e.prepare());
 for(uint32_t p=0;p<3;++p){auto n=event(WS2_NOTE_ON,.1f*(p+1));CHECK(e.enqueue(p,p+1,0,n,5)==WS2_OK);}
 render(e,17);CHECK(e.output_sample(4,0,0,4)==0);CHECK(std::abs(e.output_sample(4,0,0,5)-.3f)<1e-6);
 Outgoing out;uint32_t count=0;while(e.pop_egress(out)){CHECK(out.frame==5);++count;}CHECK(count==3);
 auto gen=e.meter().generation;e.panic();CHECK(e.meter().applied_generation==gen);CHECK(e.enqueue(0,1,0,event(WS2_NOTE_ON),20)==WS2_STATE);render(e,17);CHECK(e.meter().generation==e.meter().applied_generation);CHECK(e.output_sample(4,0,0,0)==0);
 CHECK(e.enqueue(0,1,0,event(WS2_NOTE_ON),40,Timing::Frames,gen)==WS2_STATE);
}
static void timing(){
 Config c=cfg();c.max_block=16;c.feedback_block=16;c.epoch_ns=1000000000;
 Engine e(c);CHECK(e.add_node(1,ws::Module<Receiver>::descriptor()));CHECK(e.register_producer()==0);CHECK(e.prepare());
 CHECK(e.publish_clock(0,c.epoch_ns));CHECK(!e.publish_clock(0,c.epoch_ns-1));
 CHECK(e.enqueue(0,1,0,event(WS2_NOTE_ON,.4f),1001000000,Timing::Timestamped)==WS2_OK);
 for(int i=0;i<4;++i){render(e,16);CHECK(e.output_sample(1,0,0,0)==0);}render(e,16);CHECK(e.output_sample(1,0,0,0)==.4f);
 CHECK(e.enqueue(0,1,0,event(WS2_NOTE_OFF),0,Timing::Immediate)==WS2_OK);render(e,16);CHECK(e.output_sample(1,0,0,0)==0);
}
static void overflow(){
 Config c=cfg();c.event_budget=8;c.release_reserve=2;c.ring_capacity=32;c.future_capacity=128;Engine e(c);
 CHECK(e.add_node(1,ws::Module<Receiver>::descriptor()));CHECK(e.register_producer()==0);CHECK(e.prepare());CHECK(e.enqueue(0,1,0,event(WS2_NOTE_ON),0)==WS2_OK);render(e,17);
 for(int i=0;i<20;++i)CHECK(e.enqueue(0,1,WS2_DIRECT_PORT,event(WS2_PARAM,float(i)/20),17)==WS2_OK);
 CHECK(e.enqueue(0,1,0,event(WS2_NOTE_OFF),17)==WS2_OK);render(e,17);CHECK(e.output_sample(1,0,0,0)==0);CHECK(e.meter().coalesced>0);CHECK(e.meter().release_emergencies==0);
 for(int i=0;i<12;++i)CHECK(e.enqueue(0,1,0,event(WS2_NOTE_OFF),34)==WS2_OK);render(e,17);CHECK(e.meter().release_emergencies==1);CHECK(e.meter().generation==e.meter().applied_generation);
 Config small=cfg();small.message_arena_bytes=8;Engine arena(small);CHECK(arena.add_node(1,ws::Module<Echo>::descriptor()));CHECK(arena.register_producer()==0);CHECK(arena.prepare());
 uint8_t bytes[16]{};auto message=event(WS2_MESSAGE);message.bytes=16;message.data=bytes;CHECK(arena.enqueue(0,1,0,message,0)==WS2_OK);render(arena);CHECK(arena.meter().arena_full==1);
}
static void rows(){
 Engine e(cfg());CHECK(e.add_node(1,ws::Module<ws2test::Rows>::descriptor(),{3}));CHECK(e.add_node(2,ws::Module<Pass<WS2_POLY>>::descriptor()));CHECK(e.add_node(3,ws::Module<Pass<WS2_POLY>>::descriptor()));CHECK(e.connect(1,0,2,0));CHECK(e.connect(1,2,3,0));CHECK(e.prepare());
 auto saved=e.save_state(1);CHECK(saved.size()==20);CHECK(e.load_state(1,saved)==WS2_OK);CHECK(e.save_state(1)==saved);
 auto future=saved;future[0]=9;CHECK(e.load_state(1,future)==WS2_UNSUPPORTED);CHECK(e.preserved_state(1)==future);CHECK(e.save_state(1)==future);CHECK(e.load_state(1,saved)==WS2_OK);
 CHECK(e.reconfigure(1,{2}));CHECK(e.dropped_connections()==1);render(e,17);CHECK(std::abs(e.output_sample(2,0,0,0)-.1f)<1e-6);CHECK(e.output_sample(3,0,0,0)==.25f);
}
static void producer_stress(){
 Config c=cfg();c.max_block=48;c.feedback_block=32;c.ring_capacity=1024;Engine e(c);CHECK(e.add_node(1,ws::Module<Receiver>::descriptor()));for(int i=0;i<3;++i)e.register_producer();CHECK(e.prepare());
 {allocation_watch::Scope watch;for(uint64_t block=0;block<10000;++block){for(uint32_t i=0;i<10;++i)CHECK(e.enqueue(i%3,1,WS2_DIRECT_PORT,event(WS2_PARAM,float(i)/10),block*48+i*4)==WS2_OK);render(e,48);}CHECK(watch.allocations()==0);}
 CHECK(e.meter().events==100000);CHECK(e.meter().faults==0);
 // Actual producer threads are joined before final drain/render/destruction.
 std::array<std::thread,3>threads;for(uint32_t p=0;p<3;++p)threads[p]=std::thread([&,p](){for(int i=0;i<100;++i)CHECK(e.enqueue(p,1,WS2_DIRECT_PORT,event(WS2_PARAM,.5f),0,Timing::Immediate)==WS2_OK);});
 for(auto&t:threads)t.join();render(e,48);CHECK(e.meter().faults==0);
 // Concurrent producer/render ownership, bounded batches avoid overload.
 std::atomic<int>done{0};for(uint32_t p=0;p<3;++p)threads[p]=std::thread([&,p](){for(int i=0;i<100;++i)CHECK(e.enqueue(p,1,WS2_DIRECT_PORT,event(WS2_PARAM,.5f),0,Timing::Immediate)==WS2_OK);++done;});
 while(done.load()!=3){render(e,48);std::this_thread::yield();}for(auto&t:threads)t.join();render(e,48);CHECK(e.meter().faults==0);
}
struct GlobalParamProbe:ParamProbe {
 static constexpr std::array params{ws::param("value","Value",1,100,10,"",WS2_MODULATABLE,WS2_LOG)};
 void process(ws::Block&b){CHECK(b.param(0).channels()==1);ParamProbe::process(b);}
};
static void parameter_channels(){
 Engine e;CHECK(e.add_node(1,ws::Module<ws2ports::Lfo>::descriptor()));CHECK(e.add_node(2,ws::Module<GlobalParamProbe>::descriptor()));CHECK(e.add_node(3,ws::Module<ParamProbe>::descriptor()));CHECK(e.set_channels(1,4));CHECK(e.modulate(1,0,2,0,.25f));CHECK(e.modulate(1,0,3,0,.25f));CHECK(e.register_producer()==0);CHECK(e.prepare());
 auto offset=event(WS2_PARAM_MOD,.2f);offset.channel=1;CHECK(e.enqueue(0,1,WS2_DIRECT_PORT,offset,0)==WS2_OK);render(e);
 CHECK(e.output_channels(2,0)==1);CHECK(e.output_channels(3,0)==4);CHECK(e.output_sample(3,0,0,17)!=e.output_sample(3,0,1,17));
 for(uint32_t i=0;i<256;++i){float expected=static_cast<float>(ws::to_plain(GlobalParamProbe::params[0],.5+double(.25f*e.output_sample(1,0,0,i))));CHECK(e.output_sample(2,0,0,i)==expected);}
}
static void modulation_spans(){
 Engine e;CHECK(e.add_node(1,ws::Module<ws2ports::Lfo>::descriptor()));CHECK(e.add_node(2,ws::Module<Source>::descriptor()));CHECK(e.add_node(3,ws::Module<ParamProbe>::descriptor()));CHECK(e.modulate(1,0,3,0,.2f));CHECK(e.modulate(2,0,3,0,.125f));CHECK(e.prepare());
 for(uint32_t frames:{1u,17u,256u}){render(e,frames);for(uint32_t i=0;i<frames;++i){
  double sum=double(.2f*e.output_sample(1,0,0,i))+double(.125f*e.output_sample(2,0,0,i));
  float expected=static_cast<float>(ws::to_plain(ParamProbe::params[0],.5+sum));CHECK(e.output_sample(3,0,0,i)==expected);
 }}
}
static void modulation(){
 for(float amount:{-.5f,.5f,1.f}){
  Engine e(cfg());CHECK(e.add_node(1,ws::Module<Source>::descriptor()));CHECK(e.add_node(2,ws::Module<Source>::descriptor()));CHECK(e.add_node(3,ws::Module<ParamProbe>::descriptor()));CHECK(e.set_channels(1,4));CHECK(e.set_parameter(1,0,.2f));CHECK(e.set_parameter(2,0,.4f));CHECK(e.modulate(1,0,3,0,amount));CHECK(e.modulate(2,0,3,0,.25f));CHECK(e.prepare());render(e,17);CHECK(e.output_channels(3,0)==4);CHECK(e.output_constant(3,0));
  for(uint32_t c=0;c<4;++c){double n=.5+amount*(.2f+.1f*c)+.25*.4f;float expected=static_cast<float>(std::pow(100.,std::clamp(n,0.,1.)));CHECK(std::abs(e.output_sample(3,0,c,0)-expected)<2e-5);}
 }
 Engine e(cfg());CHECK(e.add_node(1,ws::Module<ParamProbe>::descriptor()));CHECK(e.register_producer()==0);CHECK(e.prepare());render(e,17);CHECK(e.enqueue(0,1,WS2_DIRECT_PORT,event(WS2_PARAM,100),17)==WS2_OK);render(e,17);CHECK(!e.output_constant(1,0));CHECK(std::abs(e.output_sample(1,0,0,0)-std::pow(100.,.5+.5/240))<1e-5);
 Engine latency(cfg());CHECK(latency.add_node(1,ws::Module<LatencyProbe>::descriptor()));CHECK(latency.add_node(2,ws::Module<LatencyProbe>::descriptor()));CHECK(latency.connect(1,0,2,0));CHECK(latency.output(2,0,0));CHECK(latency.prepare());CHECK(latency.meter().latency_frames==14);
}
int main(){
#ifdef _MSC_VER
 _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
#endif
#if defined(_MSC_VER)&&defined(_DEBUG)
 // Warm thread runtime bookkeeping before comparing normal CRT heap blocks.
 {std::thread warm([](){});warm.join();}
 _CrtMemState before{},after{},difference{};_CrtMemCheckpoint(&before);
#endif
 invalid_events();parameter_channels();modulation_spans();producer_validation();zero_copy();cached_taper<WS2_LINEAR>();cached_taper<WS2_LOG>();cached_taper<WS2_EXP>();signals();CHECK(loop(32,false)==loop(32,true));CHECK(loop(1,false)==loop(1,true));events();timing();overflow();rows();producer_stress();modulation();
#if defined(_MSC_VER)&&defined(_DEBUG)
 CHECK(_CrtCheckMemory());_CrtMemCheckpoint(&after);if(_CrtMemDifference(&difference,&before,&after)){
  std::fprintf(stderr,"Debug CRT normal blocks %zu -> %zu, bytes %zu -> %zu; client blocks %zu -> %zu\n",before.lCounts[_NORMAL_BLOCK],after.lCounts[_NORMAL_BLOCK],before.lSizes[_NORMAL_BLOCK],after.lSizes[_NORMAL_BLOCK],before.lCounts[_CLIENT_BLOCK],after.lCounts[_CLIENT_BLOCK]);return 1;
 }std::puts("PASS Debug CRT shutdown heap balance");
#endif
 std::puts("PASS engine2 channels/modulation/events/feedback/arena/state/generation/fake clocks/concurrent stress");}
