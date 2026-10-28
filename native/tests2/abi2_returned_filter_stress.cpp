// SPDX-License-Identifier: GPL-3.0-only
// Execute the existing stress functions unchanged through an ABI2 C-callback bridge.
#define main abi1_stress_original_main
#include "../tests/returned_filter_stress.cpp"
#undef main
#include "module_loader.h"
#include "wirescape_sdk.h"
static const ws2_descriptor*stress_descriptor=nullptr;
struct Adapter {
 alignas(64)std::array<uint8_t,65536>arena{};size_t used=0;void*instance=nullptr;double rate;float values[3]{};
 static void*WS2_CALL allocate(void*ctx,size_t n,size_t alignment,uint32_t){auto&s=*static_cast<Adapter*>(ctx);s.used=(s.used+alignment-1)&~(alignment-1);if(n>s.arena.size()-s.used)return nullptr;void*p=s.arena.data()+s.used;s.used+=n;return p;}
 explicit Adapter(double sr):rate(sr){for(int i=0;i<3;++i)values[i]=stress_descriptor->params[i].initial;}
 ws2_host host(){return {sizeof(ws2_host),this,allocate};}
 static void*WS_CALL create(double rate){return new Adapter(rate);}
 static void WS_CALL destroy(void*p){auto*s=static_cast<Adapter*>(p);if(s->instance)stress_descriptor->destroy(s->instance);delete s;}
 static int WS_CALL prepare(void*p,const ws_module_config*){auto&s=*static_cast<Adapter*>(p);auto h=s.host();s.instance=stress_descriptor->create(&h);ws2_config cfg{sizeof(cfg),s.rate,512,1,1,nullptr,0};return s.instance&&stress_descriptor->prepare(s.instance,&cfg,&h)==WS2_OK?0:-1;}
 static void WS_CALL reset(void*p){stress_descriptor->reset(static_cast<Adapter*>(p)->instance);}
 static void WS_CALL event(void*p,const ws_module_event*e){if(e->type==WS_PARAMETER&&e->parameter_id<3)static_cast<Adapter*>(p)->values[e->parameter_id]=e->value;}
 static void WS_CALL process(void*p,const float*const*in,float*const*out,const ws_process_context*c){
  auto&s=*static_cast<Adapter*>(p);const float*input[]={in?in[0]:nullptr};ws2_signal signal{sizeof(signal),input,input[0]?1u:0u,input[0]?WS2_CONNECTED:WS2_CONSTANT};
  const float*data[3]={s.values,s.values+1,s.values+2};ws2_signal parameters[3];for(int i=0;i<3;++i)parameters[i]={sizeof(ws2_signal),data+i,1,WS2_CONSTANT};
  float*output[]={out[0]};ws2_signal_out result{sizeof(result),output,1,0,1};ws2_event_list events{sizeof(events),0,nullptr};
  ws2_process context{sizeof(context),c->frames,c->frame_position,1,s.rate,1,1,1,3,&signal,&result,parameters,&events,nullptr,{sizeof(ws2_event_list),0,nullptr},nullptr};
  require(stress_descriptor->process(s.instance,&context)==WS2_OK,"ABI2 ladder process failed");
 }
};
int main(int argc,char**argv){try{require(argc==2,"expected module directory");ModuleLoader loader(argv[1]);stress_descriptor=loader.abi2("ladder");auto bridge=*loader.abi1("ladder");bridge.create=Adapter::create;bridge.destroy=Adapter::destroy;bridge.prepare=Adapter::prepare;bridge.reset=Adapter::reset;bridge.event=Adapter::event;bridge.process=Adapter::process;for(double rate:{44100.,48000.,96000.}){impulse_cases(&bridge,false,rate);modulation_case(&bridge,false,rate);}std::puts("PASS unchanged returned_filter_stress cases against ABI2 ladder");return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL ABI2 returned filter stress: %s\n",e.what());return 1;}}
