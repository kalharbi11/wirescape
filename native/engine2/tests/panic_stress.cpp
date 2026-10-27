// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "../../tests2/test_modules.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace wirescape::engine2;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"panic stress line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
int main(){
 Config cfg;cfg.ring_capacity=128;cfg.future_capacity=256;
 Engine e(cfg);CHECK(e.add_node(1,ws::Module<ws2test::NotesToCv>::descriptor()));CHECK(e.register_producer()==0);CHECK(e.prepare());
 std::atomic<bool>stop{false},stop_egress{false};std::atomic<uint64_t>accepted{0},delivered{0},panics{0};
 std::thread producer([&](){uint32_t serial=0;while(!stop.load()){
  ws2_event note{};note.struct_size=sizeof(note);note.type=WS2_NOTE_ON;note.note_id=serial++;note.key=60;note.channel=0;note.value=1;
  int result=e.enqueue(0,1,0,note,0,Timing::Immediate);CHECK(result==WS2_OK||result==WS2_STATE||result==WS2_CAPACITY);if(result==WS2_OK)++accepted;
  std::this_thread::sleep_for(std::chrono::microseconds(100));
 }});
 std::thread panic([&](){uint32_t random=731;while(!stop.load()){
  random^=random<<13;random^=random>>17;random^=random<<5;
  e.panic();++panics;std::this_thread::sleep_for(std::chrono::microseconds(50+random%500));
 }});
 std::thread consumer([&](){Outgoing event;while(!stop_egress.load()){
  const auto acknowledged=e.meter().applied_generation;
  if(e.pop_egress(event)){CHECK(event.generation>=acknowledged);++delivered;}
  else std::this_thread::yield();
 }});
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
 float data[256]{};float*out[]={data};uint64_t blocks=0;
 while(std::chrono::steady_clock::now()<deadline){CHECK(e.render(out,1,256)==WS2_OK);++blocks;}
 stop=true;producer.join();panic.join();e.panic();CHECK(e.render(out,1,256)==WS2_OK);
 CHECK(e.meter().generation==e.meter().applied_generation);
 for(uint32_t c=0;c<e.output_channels(1,1);++c)for(uint32_t i=0;i<256;++i)CHECK(e.output_sample(1,1,c,i)==0);
 stop_egress=true;consumer.join();CHECK(blocks>0&&accepted>0&&delivered>0&&panics>0);CHECK(e.meter().faults==0);
 std::printf("PASS panic/render/egress: %llu renders, %llu accepted, %llu delivered, %llu panics\n",(unsigned long long)blocks,(unsigned long long)accepted.load(),(unsigned long long)delivered.load(),(unsigned long long)panics.load());
}
