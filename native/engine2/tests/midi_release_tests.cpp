// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "midi_release.h"
#include "wirescape_sdk.h"
#include <cstdio>
#include <cstdlib>
using namespace wirescape::engine2;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
static uint32_t releases;
struct Receiver {
 static constexpr ws::Info info{"test.midi_release","Release receiver",1};
 static constexpr std::array inputs{ws::port("midi","MIDI",WS2_EV_MIDI,WS2_POLY,0,nullptr,WS2_EVENT)};
 static constexpr std::array<ws2_port,0>outputs{};
 static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.split([](uint32_t,uint32_t){},[](const ws2_event&e){if(midi_release(e))++releases;});}
};
static ws2_event midi(uint32_t a,uint32_t b=0){ws2_event e{};e.struct_size=sizeof(e);e.type=WS2_MIDI;e.channel=0;e.ump_words=(a>>28)==4?2:1;e.ump[0]=a;e.ump[1]=b;return e;}
static void saturated(ws2_event event,bool critical){
 Config cfg;cfg.ring_capacity=8;cfg.event_budget=64;cfg.release_reserve=16;
 Engine engine(cfg);CHECK(engine.add_node(1,ws::Module<Receiver>::descriptor()));CHECK(engine.register_producer()==0);CHECK(engine.prepare());
 auto normal=midi(0x20903c7f);int accepted=0;
 while(engine.enqueue(0,1,0,normal,0)==WS2_OK){CHECK(++accepted<32);}CHECK(accepted>0);
 CHECK(engine.enqueue(0,1,0,event,0)==(critical?WS2_OK:WS2_CAPACITY));
 releases=0;float scratch[16]{};float*out[]={scratch};CHECK(engine.render(out,1,16)==WS2_OK);
 CHECK(releases==(critical?1u:0u));CHECK(engine.meter().release_emergencies==0);
}
int main(){
 for(auto e:{midi(0x20803c40),midi(0x20903c00),midi(0x20b04000),midi(0x20b0403f),midi(0x20b07800),midi(0x20b07b7f),midi(0x40803c00,0xffff0000),midi(0x40b04000,0x7fffffff),midi(0x40b07800,0),midi(0x40b07b00,0)})saturated(e,true);
 for(auto e:{midi(0x20903c01),midi(0x20b04040),midi(0x20b00100),midi(0x40903c00,0),midi(0x40903c00,0xffff0000),midi(0x40b04000,0x80000000),midi(0x40b00100,0)})saturated(e,false);
 auto bad=midi(0x40803c00);bad.ump_words=1;CHECK(!midi_release(bad));bad=midi(0x20803c80);CHECK(!midi_release(bad));
 std::puts("PASS raw MIDI release reserve and MIDI1/MIDI2 boundaries");
}
