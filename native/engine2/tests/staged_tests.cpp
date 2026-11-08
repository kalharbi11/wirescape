// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "../../tests2/dsp_fixture.h"
#include "../../tests2/test_modules.h"
#include "../../tests2/allocation_watch.h"
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
using namespace wirescape::engine2;
template<uint32_t Delay>struct LatencyTap {
 static constexpr ws::Info info{"test.latency","Latency",1};
 static constexpr std::array inputs{ws::port("signal","Signal"),ws::port("event","Event",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT)};
 static constexpr std::array outputs{ws::port("signal","Signal"),ws::port("event","Event",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT)};
 static constexpr std::array params{ws::param("gain","Gain",0,1,1)};
 void prepare(const ws::Setup&){}void reset(){}
 uint32_t latency_frames()const{return Delay;}
 void process(ws::Block&b){b.out(0).set_channels(1);b.out(0).mark_constant();b.out(0).ch(0)[0]=b.in(0).value(0,0);b.out(1).set_channels(0);for(auto&e:b.events())VERIFY(b.emit(1,e));}
};
struct Rows:ws2test::Rows {
 static inline int prepares=0;
 void prepare(const ws::Setup&s){++prepares;ws2test::Rows::prepare(s);}
};
static void latency(){
 Engine e;VERIFY(e.add_node(1,ws::Module<LatencyTap<7>>::descriptor()));VERIFY(e.add_node(2,ws::Module<LatencyTap<11>>::descriptor()));
 VERIFY(e.add_node(3,ws::Module<LatencyTap<99>>::descriptor()));VERIFY(e.add_node(4,ws::Module<LatencyTap<101>>::descriptor()));VERIFY(e.add_node(5,ws::Module<LatencyTap<1000>>::descriptor()));VERIFY(e.add_node(6,ws::Module<LatencyTap<13>>::descriptor()));
 VERIFY(e.connect(1,0,2,0));VERIFY(e.modulate(3,0,2,0,.1f));VERIFY(e.connect(4,1,2,1));
 VERIFY(e.output(2,0,0));VERIFY(e.output(6,0,1));VERIFY(e.output(1,0,1));VERIFY(e.prepare());
 VERIFY(e.latency_frames(0)==18);VERIFY(e.latency_frames(1)==13);VERIFY(e.latency_frames(2)==0);VERIFY(e.latency_frames(32)==0);VERIFY(e.meter().latency_frames==18);VERIFY(e.feedback_loops().empty());
 for(uint32_t delay:{1u,32u,256u}){
  Config c;c.feedback_block=delay;Engine loop(c);VERIFY(loop.add_node(1,ws::Module<LatencyTap<7>>::descriptor()));VERIFY(loop.add_node(2,ws::Module<LatencyTap<11>>::descriptor()));VERIFY(loop.add_node(3,ws::Module<LatencyTap<13>>::descriptor()));
  VERIFY(loop.connect(1,0,1,0));VERIFY(loop.connect(2,0,3,0));VERIFY(loop.connect(2,1,3,1));VERIFY(loop.connect(3,1,2,1));VERIFY(loop.output(1,0,0));VERIFY(loop.output(3,0,1));VERIFY(loop.prepare());
  VERIFY(loop.latency_frames(0)==7);VERIFY(loop.latency_frames(1)==24);VERIFY(loop.meter().latency_frames==24);
  auto loops=loop.feedback_loops();VERIFY(loops.size()==2);for(auto&l:loops)VERIFY(l.delay_frames==delay);
  VERIFY(loops[0].nodes==std::vector<uint32_t>({1}));VERIFY(loops[1].nodes==std::vector<uint32_t>({2,3}));
 }
 Engine large;VERIFY(large.add_node(1,ws::Module<LatencyTap<UINT32_MAX>>::descriptor()));VERIFY(large.add_node(2,ws::Module<LatencyTap<11>>::descriptor()));VERIFY(large.connect(1,0,2,0));VERIFY(large.output(2,0,0));VERIFY(large.prepare());VERIFY(large.latency_frames(0)==UINT32_MAX);
}
struct DynamicLatency {
 static constexpr ws::Info info{"test.dynamic_latency","Dynamic latency",1};
 static constexpr std::array inputs{ws::port("signal","Signal")};
 static constexpr std::array outputs{ws::port("signal","Signal")};
 static constexpr std::array params{ws::param("frames","Frames",0,64,7,"frames",
     WS2_STEPPED|WS2_AUTOMATABLE,WS2_LINEAR,0,4,1)};
 std::atomic<uint32_t> published{7};
 void prepare(const ws::Setup&){published.store(7,std::memory_order_relaxed);}
 void reset(){}
 uint32_t latency_frames()const{return published.load(std::memory_order_relaxed);}
 void process(ws::Block&b){
  published.store(static_cast<uint32_t>(b.param(0).value(0,b.frames()-1)),std::memory_order_relaxed);
  b.out(0).set_channels(1);b.out(0).mark_constant();b.out(0).ch(0)[0]=b.in(0).value(0,0);
 }
};
static void dynamic_latency(){
 auto graph=[](Engine& e){
  VERIFY(e.add_node(1,ws::Module<DynamicLatency>::descriptor()));
  VERIFY(e.add_node(2,ws::Module<LatencyTap<11>>::descriptor()));
  VERIFY(e.add_node(3,ws::Module<LatencyTap<13>>::descriptor()));
  VERIFY(e.connect(1,0,2,0));
  VERIFY(e.output(2,0,0));VERIFY(e.output(1,0,1));VERIFY(e.output(3,0,1));
  VERIFY(e.register_producer()==0);VERIFY(e.prepare());
 };
 Engine live;graph(live);
 VERIFY(live.latency_frames(0)==18&&live.latency_frames(1)==13);
 float left[17]{},right[17]{};float*outputs[]{left,right};
 auto change=[&](float frames){
  ws2_event event{};event.struct_size=sizeof(event);event.type=WS2_PARAM;
  event.index=0;event.value=frames;
  VERIFY(live.enqueue(0,1,WS2_DIRECT_PORT,event,0,Timing::Immediate)==WS2_OK);
  VERIFY(live.render(outputs,2,17)==WS2_OK);
 };
 change(23);
 VERIFY(live.latency_frames(0)==18); // Still the prepared reporting snapshot.
 {
  allocation_watch::Scope watch;
  live.refresh_latency();
  VERIFY(watch.allocations()==0);
 }
 VERIFY(live.latency_frames(0)==34&&live.latency_frames(1)==23);
 VERIFY(live.meter().latency_frames==34);
 change(31);
 VERIFY(live.latency_frames(0)==34);
 Engine candidate;graph(candidate);VERIFY(candidate.latency_frames(0)==18);
 std::atomic<bool> done{false},success{false};
 std::thread control([&]{
  success.store(live.replan(candidate,16000000),std::memory_order_release);
  done.store(true,std::memory_order_release);
 });
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 while(!done.load(std::memory_order_acquire)&&std::chrono::steady_clock::now()<deadline){
  VERIFY(live.render(outputs,2,17)==WS2_OK);
  std::this_thread::yield();
 }
 control.join();
 VERIFY(success.load(std::memory_order_acquire));
 VERIFY(live.latency_frames(0)==42&&live.latency_frames(1)==31);
 VERIFY(live.meter().latency_frames==42);
 {
  allocation_watch::Scope watch;
  live.refresh_latency();
  VERIFY(watch.allocations()==0);
 }
 VERIFY(live.latency_frames(0)==42&&live.latency_frames(1)==31);
}
static void continuity(uint32_t frames,uint32_t channels){
 Engine a,b;
 for(auto*e:{&a,&b}){
  VERIFY(e->add_node(1,ws::Module<ws2ports::Vco>::descriptor()));VERIFY(e->add_node(2,ws::Module<ws2ports::Vcf>::descriptor()));
  VERIFY(e->add_node(9,ws::Module<Rows>::descriptor(),{3}));VERIFY(e->add_node(10,ws::Module<LatencyTap<0>>::descriptor()));
  VERIFY(e->set_channels(1,channels));VERIFY(e->connect(1,0,2,0));VERIFY(e->output(2,1,0));VERIFY(e->register_producer()==0);VERIFY(e->prepare());
 }
 float x[256]{},y[256]{};float*xo[]={x};float*yo[]={y};
 auto block=[&](){allocation_watch::Scope watch;VERIFY(a.render(xo,1,frames)==WS2_OK);VERIFY(b.render(yo,1,frames)==WS2_OK);VERIFY(watch.allocations()==0);VERIFY(std::memcmp(x,y,frames*sizeof(float))==0);};
 for(int i=0;i<7;++i)block();int before=Rows::prepares;
 VERIFY(a.reconfigure(9,{2}));VERIFY(Rows::prepares==before+1);VERIFY(a.meter().frame==b.meter().frame);
 VERIFY(a.reconfigure(9,{2}));VERIFY(Rows::prepares==before+1);
 VERIFY(!a.reconfigure(9,{0}));VERIFY(Rows::prepares==before+1);
 for(int i=0;i<11;++i)block();VERIFY(a.meter().faults==0);
 ws2_event event{};event.struct_size=sizeof(event);event.type=WS2_NOTE_ON;event.value=1;event.channel=0;event.note_id=1;
 VERIFY(a.enqueue(0,10,1,event,a.meter().frame)==WS2_OK);VERIFY(a.render(xo,1,frames)==WS2_OK);Outgoing out;VERIFY(a.pop_egress(out));VERIFY(out.node==10&&out.port==1&&out.event.type==WS2_NOTE_ON);
 // Repeating staging must not accumulate latency or leave stale callbacks.
 VERIFY(a.reconfigure(9,{3}));VERIFY(a.latency_frames(0)==0);VERIFY(a.enqueue(0,10,1,event,a.meter().frame)==WS2_OK);VERIFY(a.render(xo,1,frames)==WS2_OK);VERIFY(a.pop_egress(out));
}
// Reconfiguration preserves the tap layout even when most DSP instances survive.
static void telemetry_reconfigure(){
  Engine e;
  VERIFY(e.add_node(9,ws::Module<Rows>::descriptor(),{3}));
  VERIFY(e.add_node(10,ws::Module<LatencyTap<0>>::descriptor()));
  VERIFY(e.register_producer()==0);
  VERIFY(e.set_telemetry_taps({Tap{Tap::EventOut,10,1},Tap{Tap::Param,10,0},Tap{Tap::Output,9,0}}));
  VERIFY(e.prepare());
  float audio[17]{};float*out[]={audio};float values[3]{};uint32_t counts[3]{};uint64_t frame=999;
  VERIFY(e.render(out,1,17)==WS2_OK);
  VERIFY(e.read_telemetry(values,counts,3,frame)==3&&frame==17&&counts[0]==0&&values[1]==1);
  for(uint8_t rows:{uint8_t{2},uint8_t{3}}){
    VERIFY(e.reconfigure(9,{rows}));
    VERIFY(e.read_telemetry(values,counts,3,frame)==3&&frame==0&&counts[0]==0);
    ws2_event note{};note.struct_size=sizeof(note);note.type=WS2_NOTE_ON;note.value=1;note.channel=0;note.note_id=1;
    VERIFY(e.enqueue(0,10,1,note,e.meter().frame)==WS2_OK);
    VERIFY(e.render(out,1,17)==WS2_OK);
    VERIFY(e.read_telemetry(values,counts,3,frame)==3&&frame==e.meter().frame);
    VERIFY(counts[0]==1&&values[1]==1);
    Outgoing emitted;VERIFY(e.pop_egress(emitted)&&emitted.node==10&&emitted.port==1);
  }
  Engine removed;
  VERIFY(removed.add_node(9,ws::Module<Rows>::descriptor(),{3}));
  VERIFY(removed.set_telemetry_taps({Tap{Tap::Output,9,2}}));VERIFY(removed.prepare());
  VERIFY(!removed.reconfigure(9,{2})); // Explicit refusal: keep reader slot and graph intact.
  VERIFY(removed.render(out,1,17)==WS2_OK);
  VERIFY(removed.read_telemetry(values,counts,3,frame)==1&&frame==17);
}
struct Feedback {
 static constexpr ws::Info info{"test.loop","Loop",1};
 static constexpr std::array inputs{ws::port("in","In")};static constexpr std::array outputs{ws::port("out","Out")};static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(1);b.out(0).mark_constant(false);for(uint32_t i=0;i<b.frames();++i)b.out(0).ch(0)[i]=.5f*b.in(0).value(0,i)+(b.frame_position()+i==0?1.f:0.f);}
};
static void feedback_continuity(){
 Engine a,b;for(auto*e:{&a,&b}){VERIFY(e->add_node(1,ws::Module<Feedback>::descriptor()));VERIFY(e->add_node(9,ws::Module<Rows>::descriptor(),{3}));VERIFY(e->connect(1,0,1,0));VERIFY(e->output(1,0,0));VERIFY(e->prepare());}
 float x[17]{},y[17]{};float*xo[]={x};float*yo[]={y};
 for(int i=0;i<10;++i){if(i==3)VERIFY(a.reconfigure(9,{2}));allocation_watch::Scope watch;VERIFY(a.render(xo,1,17)==0);VERIFY(b.render(yo,1,17)==0);VERIFY(std::memcmp(x,y,sizeof(x))==0);VERIFY(watch.allocations()==0);}
}
int main(){latency();dynamic_latency();for(uint32_t frames:{1u,17u,256u})for(uint32_t channels:{1u,4u,16u})continuity(frames,channels);telemetry_reconfigure();feedback_continuity();std::puts("PASS per-bus signal latency, SCC delays, staged continuity/rollback/callbacks/noalloc");}
