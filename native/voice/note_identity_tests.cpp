// SPDX-License-Identifier: MIT
#include "note_to_cv.h"
#include <array>
#include <cstdio>
#include <cstdlib>
using namespace wirescape::voice;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
int main(){
 for(int voices:{1,4}){
  NoteToCv core;Config config;config.max_voices=static_cast<uint8_t>(voices);CHECK(core.configure(config)==Status::Ok);
  std::array<ws2_event,16>log{};int count=0;auto emit=[&](const ws2_event&e){CHECK(count<16);log[count++]=e;};
  ws2_event e{};e.struct_size=sizeof(e);e.type=WS2_NOTE_ON;e.note_id=-1;e.source_id=12;e.channel=0;e.key=60;e.value=1;
  core.event(e,emit);CHECK(count==1);int32_t oldest=log[0].note_id;CHECK(oldest>=0);
  core.event(e,emit);int32_t newest=log[count-1].note_id;CHECK(newest>=0&&newest!=oldest);
  if(voices==1){CHECK(count==3);CHECK(log[1].type==WS2_NOTE_CHOKE&&log[1].note_id==oldest);CHECK(log[2].type==WS2_NOTE_ON);}
  e.type=WS2_NOTE_OFF;core.event(e,emit);CHECK(core.allocator().active_count()==1);
  CHECK(core.allocator().voice(voices==1?0:1).note_id==newest);
  core.event(e,emit);CHECK(core.allocator().active_count()==0);
  core.reset();count=0;e.type=WS2_NOTE_ON;core.event(e,emit);CHECK(log[0].note_id==oldest);
  core.release_all(12,e,emit);CHECK(core.allocator().active_count()==0);
 }
 std::puts("PASS anonymous activation identity, FIFO, steal tombstones, reset");
}
