// SPDX-License-Identifier: GPL-3.0-only
#include "osc_host.h"
#include "monitor_log.h"
#include "runtime_patch.h"
#include "../tests2/allocation_watch.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"line %d %s\n",__LINE__,#x);std::abort();}}while(0)
int main(){
 const std::string lan=R"({"modules":[{"id":1,"module":"osc.in","config":{"lan":true}}]})";
 CHECK(wirescape::osc::Host::needs_lan(lan));wirescape::osc::Host denied;std::string error;CHECK(!denied.start(nullptr,lan,0,error));CHECK(error.find("LAN")!=std::string::npos);
 const std::string patch=R"({"format":"wirescape.native.patch","version":2,"settings":{},"modules":[{"id":1,"module":"osc.in","version":1,"parameters":{},"config":{"rows":[{"pattern":"/knob","smoothing_ms":0}]}},{"id":2,"module":"monitor","version":1,"parameters":{},"config":{"prefix":"Pico"}},{"id":3,"module":"essentials.vco","version":1,"parameters":{}}],"connections":[{"kind":"event","from":{"module":1,"port":"messages"},"to":{"module":2,"port":"events"}},{"kind":"signal","from":{"module":1,"port":"row1"},"to":{"module":2,"port":"signal1"}}],"outputs":[{"module":3,"port":"audio","bus_channel":0}]})";
 ws_engine_config config{sizeof(config),48000,128,64,2048};std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>catalog(app_engine_create(&config),app_engine_destroy);
 std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>engine(wirescape::document::load_runtime_patch(patch,catalog.get(),config,error),app_engine_destroy);if(!engine)std::fprintf(stderr,"%s\n",error.c_str());CHECK(bool(engine));
 auto taps=app_engine_monitor_taps(engine.get());CHECK(taps.size()==1&&taps[0].prefix=="Pico");CHECK(!wirescape::osc::Host::needs_lan(patch));
 const uint8_t packet[]={'/','k','n','o','b',0,0,0,',','f',0,0,0x3f,0,0,0};ws2_event event{};event.struct_size=sizeof(event);event.type=WS2_MESSAGE;event.message_kind=WS2_MSG_OSC;event.bytes=sizeof(packet);event.data=packet;event.source_id=9;
 CHECK(app_engine_native(engine.get()).enqueue(0,1,WS2_DIRECT_PORT,event,0)==WS2_OK);float samples[128];int result;{allocation_watch::Scope scope;result=app_engine_render(engine.get(),samples,128);CHECK(scope.allocations()==0);}CHECK(result==WS_OK);CHECK(app_engine_native(engine.get()).output_sample(1,0,0,0)==.5f);
 engine.reset();bool message=false,signal=false;wirescape::monitor::Entry entry;while(taps[0].tap->pop(entry)){auto text=format_monitor_entry(taps[0],entry);message|=text.find("OSC /knob f:0.5")!=std::string::npos;signal|=text.find("= 0.5")!=std::string::npos;}CHECK(message&&signal);
 {
  std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>keys(app_engine_create(&config),app_engine_destroy);CHECK(app_engine_add_node(keys.get(),1,"host.keyboard")==WS_OK);CHECK(app_engine_prepare(keys.get(),1,0)==WS_OK);CHECK(app_engine_initialize_parameter(keys.get(),1,0,1)==WS_OK);
  for(uint32_t block=0;block<1100;++block){ws2_event note{};note.struct_size=sizeof(note);note.type=block%2?WS2_NOTE_OFF:WS2_NOTE_ON;note.note_id=7;note.source_id=3;note.channel=0;note.key=60;note.value=block%2?0:1;CHECK(app_engine_native(keys.get()).enqueue(0,1,WS2_DIRECT_PORT,note,0,wirescape::engine2::Timing::Immediate)==WS2_OK);allocation_watch::Scope scope;CHECK(app_engine_render(keys.get(),samples,128)==WS_OK);CHECK(scope.allocations()==0);}
  CHECK(app_engine_native(keys.get()).meter().release_emergencies==0);CHECK(app_engine_native(keys.get()).output_sample(1,1,0,127)==0);
 }
 std::puts("PASS OSC LAN refusal, runtime configuration, catalogue, Tap lifetime and zero render allocation");
}
