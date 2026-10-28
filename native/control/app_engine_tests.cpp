// SPDX-License-Identifier: GPL-3.0-only
#include "app_engine.h"
#include "module_catalog.h"
#include "../tests2/allocation_watch.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
static void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
using App=std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>;
static App make(){ws_engine_config c{sizeof(c),48000,128,32,2048};App e(app_engine_create(&c),app_engine_destroy);check(bool(e),"create app engine");return e;}
static void shim(const char*library){
 auto source=make();check(app_engine_module_count(source.get())==14,"thirteen Essentials plus host.keyboard");
 for(auto* native:wirescape::app::builtin_modules()){
  const ws_module_descriptor* metadata=nullptr;for(uint32_t i=0;i<app_engine_module_count(source.get());++i){auto* d=app_engine_module(source.get(),i);if(!std::strcmp(d->id,native->id))metadata=d;}
  check(metadata&&metadata->input_count==native->input_count&&metadata->output_count==native->output_count,"catalogue port counts");
  for(uint32_t i=0;i<native->input_count;++i)check(metadata->inputs[i].kind==static_cast<uint32_t>(native->inputs[i].hint==WS2_AUDIO?WS_AUDIO:WS_CONTROL),"catalogue input kind");
  for(uint32_t i=0;i<native->output_count;++i)check(metadata->outputs[i].kind==static_cast<uint32_t>(native->outputs[i].hint==WS2_AUDIO?WS_AUDIO:WS_CONTROL),"catalogue output kind");
 }
 check(app_engine_load_module(source.get(),library)==WS_OK,"load third-party ABI1 shared library");
 auto e=make();check(app_engine_copy_modules(e.get(),source.get())==WS_OK,"replacement catalogue");source.reset();
 check(std::strcmp(app_engine_parameter_id(e.get(),"test.app_legacy",0),"7")==0,"preserve non-contiguous ABI1 ID and library lifetime");
 check(app_engine_add_node(e.get(),9,"test.app_legacy")==WS_OK,"shim node");
 check(app_engine_prepare(e.get(),9,0)==WS_OK,"shim prepare");
 check(app_engine_initialize_parameter(e.get(),9,7,.5f)==WS_OK,"shim parameter init by ID");
 float out[128];int result;size_t allocations=0;
 {allocation_watch::Scope watch;result=app_engine_render(e.get(),out,128);allocations+=watch.allocations();}
 check(result==WS_OK&&std::all_of(out,out+128,[](float v){return v==.5f;}),"shim initial output exact");
 ws_event input{sizeof(input),128,9,WS_PARAMETER,7,.75f,1,0,app_engine_generation(e.get())};
 check(app_engine_enqueue(e.get(),&input)==WS_OK,"enqueue ABI1 parameter ID, not array index");
 {allocation_watch::Scope watch;result=app_engine_render(e.get(),out,128);allocations+=watch.allocations();}
 check(result==WS_OK&&std::all_of(out,out+128,[](float v){return v==.75f;}),"shim event output exact");
 check(allocations==0,"shim callback no allocation");
 input.parameter_id=99;check(app_engine_enqueue(e.get(),&input)==WS_INVALID,"unknown parameter refused");
}
static void host_notes(){
 auto e=make();check(app_engine_build_essentials(e.get(),"")==WS_OK,"build app graph");
 auto& engine=app_engine_native(e.get());
 for(uint32_t node=1;node<=4;++node)check(engine.descriptor(node)&&engine.descriptor(node)->abi_version==2,"native ABI2 Essentials");
 for(auto node:{1u,2u}){ws_event on{sizeof(on),0,node,WS_NOTE_ON,0,69,1,71,app_engine_generation(e.get())};check(app_engine_enqueue(e.get(),&on)==WS_OK,"host note on");}
 for(auto node:{1u,2u}){ws_event off{sizeof(off),4800,node,WS_NOTE_OFF,0,69,1,71,app_engine_generation(e.get())};check(app_engine_enqueue(e.get(),&off)==WS_OK,"host note off");}
 float out[128],previous=0;double peak=0,tail=0;size_t allocations=0;unsigned crossings=0;
 for(uint32_t frame=0;frame<48000;frame+=128){uint32_t count=std::min(128u,48000-frame);int result;
  {allocation_watch::Scope watch;result=app_engine_render(e.get(),out,count);allocations+=watch.allocations();}
  check(result==WS_OK,"render host-note graph");
  for(uint32_t i=0;i<count;++i){check(std::isfinite(out[i]),"finite callback output");peak=std::max(peak,double(std::abs(out[i])));if(frame+i>=24000)tail=std::max(tail,double(std::abs(out[i])));
   float oscillator=engine.output_sample(1,0,0,i);if(frame+i<4800&&previous<0&&oscillator>=0)++crossings;previous=oscillator;
  }
 }
 check(peak>0&&tail==0,"ADSR host note reaches envelope and releases");
 check(crossings>=42&&crossings<=45,"host A4 note reaches VCO");
 check(allocations==0,"host callback no allocation");
}
int main(int argc,char**argv){try{check(argc==2,"ABI1 fixture library argument");check(app_engine_create(nullptr)==nullptr,"null configuration refused");shim(argv[1]);host_notes();std::cout<<"PASS app native catalogue, host notes, ABI1 IDs/library lifetime and allocation-free callbacks\n";return 0;}
catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
