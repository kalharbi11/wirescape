// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "wirescape_sdk.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace wirescape::engine2;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"feedback line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
template<bool Emit>struct Mixed {
 static constexpr ws::Info info{"test.mixed","Mixed SCC",1};
 static constexpr std::array inputs{ws::port("signal","Signal"),ws::port("events","Events",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT)};
 static constexpr std::array outputs{ws::port("signal","Signal"),ws::port("events","Events",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT),ws::port("received","Received")};
 static constexpr std::array params{ws::param("gain","Gain",0,2,1)};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){
  b.out(0).set_channels(1);b.out(0).mark_constant(false);b.out(1).set_channels(0);b.out(2).set_channels(1);b.out(2).mark_constant(false);
  for(uint32_t i=0;i<b.frames();++i){b.out(0).ch(0)[i]=.5f*b.in(0).value(0,i)*b.param(0).value(0,i)+(b.frame_position()+i==0?1.f:0.f);b.out(2).ch(0)[i]=0;}
  for(const auto&e:b.events_in(1))b.out(2).ch(0)[e.offset]=1;
  if(Emit&&b.frame_position()<=255&&b.frame_position()+b.frames()>255){ws2_event e{};e.struct_size=sizeof(e);e.type=WS2_NOTE_ON;e.offset=static_cast<uint32_t>(255-b.frame_position());e.value=1;CHECK(b.emit(1,e));}
 }
};
static uint64_t run(uint32_t delay,const std::array<uint32_t,3>&order,bool reverse){
 Config cfg;cfg.feedback_block=delay;cfg.max_block=256;cfg.ring_capacity=16;cfg.future_capacity=32;
 Engine e(cfg);for(auto id:order)CHECK(e.add_node(id,id==30?ws::Module<Mixed<true>>::descriptor():ws::Module<Mixed<false>>::descriptor()));
 for(int i=0;i<5;++i){int edge=reverse?4-i:i;switch(edge){
 case 0:CHECK(e.connect(10,0,20,0));break;case 1:CHECK(e.connect(20,0,30,0));break;case 2:CHECK(e.connect(30,0,10,0));break;
 case 3:CHECK(e.connect(30,1,10,1));break;case 4:CHECK(e.modulate(30,0,10,0,.1f));break;}}
 CHECK(e.output(30,0,0));CHECK(e.output(10,2,1));CHECK(e.prepare());CHECK(e.feedback_edges()==3);
 uint64_t hash=UINT64_C(14695981039346656037);uint32_t received=0;
 for(uint32_t block=0;block<3;++block){float a[256]{},b[256]{};float*out[]={a,b};CHECK(e.render(out,2,256)==WS2_OK);
  for(uint32_t i=0;i<256;++i){CHECK(b[i]==(block*256+i==255+delay?1.f:0.f));received+=b[i]!=0;uint32_t bits;std::memcpy(&bits,a+i,4);hash=(hash^bits)*UINT64_C(1099511628211);}}
 CHECK(received==1);CHECK(e.meter().faults==0);return hash;
}
int main(){for(uint32_t delay:{1u,32u,256u}){std::array<uint32_t,3>order{10,20,30};const auto expected=run(delay,order,false);do{CHECK(run(delay,order,false)==expected);CHECK(run(delay,order,true)==expected);}while(std::next_permutation(order.begin(),order.end()));}std::puts("PASS mixed SCC, insertion permutations and last-span event across callbacks");}
