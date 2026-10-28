// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "../../tests2/dsp_fixture.h"
#include "../../tests2/test_modules.h"
#include "../../tests2/allocation_watch.h"
#include <cstring>
using namespace wirescape::engine2;
template<uint32_t Delay>struct Tap {
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
 Engine e;VERIFY(e.add_node(1,ws::Module<Tap<7>>::descriptor()));VERIFY(e.add_node(2,ws::Module<Tap<11>>::descriptor()));
 VERIFY(e.add_node(3,ws::Module<Tap<99>>::descriptor()));VERIFY(e.add_node(4,ws::Module<Tap<101>>::descriptor()));VERIFY(e.add_node(5,ws::Module<Tap<1000>>::descriptor()));VERIFY(e.add_node(6,ws::Module<Tap<13>>::descriptor()));
 VERIFY(e.connect(1,0,2,0));VERIFY(e.modulate(3,0,2,0,.1f));VERIFY(e.connect(4,1,2,1));
 VERIFY(e.output(2,0,0));VERIFY(e.output(6,0,1));VERIFY(e.output(1,0,1));VERIFY(e.prepare());
 VERIFY(e.latency_frames(0)==18);VERIFY(e.latency_frames(1)==13);VERIFY(e.latency_frames(2)==0);VERIFY(e.latency_frames(32)==0);VERIFY(e.meter().latency_frames==18);VERIFY(e.feedback_loops().empty());
 for(uint32_t delay:{1u,32u,256u}){
  Config c;c.feedback_block=delay;Engine loop(c);VERIFY(loop.add_node(1,ws::Module<Tap<7>>::descriptor()));VERIFY(loop.add_node(2,ws::Module<Tap<11>>::descriptor()));VERIFY(loop.add_node(3,ws::Module<Tap<13>>::descriptor()));
  VERIFY(loop.connect(1,0,1,0));VERIFY(loop.connect(2,0,3,0));VERIFY(loop.connect(2,1,3,1));VERIFY(loop.connect(3,1,2,1));VERIFY(loop.output(1,0,0));VERIFY(loop.output(3,0,1));VERIFY(loop.prepare());
  VERIFY(loop.latency_frames(0)==7);VERIFY(loop.latency_frames(1)==24);VERIFY(loop.meter().latency_frames==24);
  auto loops=loop.feedback_loops();VERIFY(loops.size()==2);for(auto&l:loops)VERIFY(l.delay_frames==delay);
  VERIFY(loops[0].nodes==std::vector<uint32_t>({1}));VERIFY(loops[1].nodes==std::vector<uint32_t>({2,3}));
 }
 Engine large;VERIFY(large.add_node(1,ws::Module<Tap<UINT32_MAX>>::descriptor()));VERIFY(large.add_node(2,ws::Module<Tap<11>>::descriptor()));VERIFY(large.connect(1,0,2,0));VERIFY(large.output(2,0,0));VERIFY(large.prepare());VERIFY(large.latency_frames(0)==UINT32_MAX);
}
static void continuity(uint32_t frames,uint32_t channels){
 Engine a,b;
 for(auto*e:{&a,&b}){
  VERIFY(e->add_node(1,ws::Module<ws2ports::Vco>::descriptor()));VERIFY(e->add_node(2,ws::Module<ws2ports::Vcf>::descriptor()));
  VERIFY(e->add_node(9,ws::Module<Rows>::descriptor(),{3}));VERIFY(e->add_node(10,ws::Module<Tap<0>>::descriptor()));
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
int main(){latency();for(uint32_t frames:{1u,17u,256u})for(uint32_t channels:{1u,4u,16u})continuity(frames,channels);feedback_continuity();std::puts("PASS per-bus signal latency, SCC delays, staged continuity/rollback/callbacks/noalloc");}
