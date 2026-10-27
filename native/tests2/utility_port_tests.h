// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "dsp_fixture.h"
#include "module_loader.h"
#include "allocation_watch.h"
#include <cstring>
#include <memory>
#include <vector>
static std::vector<uint8_t>port_save(DspFixture&f){std::vector<uint8_t>bytes;ws2_writer w{sizeof(w),&bytes,[](void*ctx,const void*p,uint32_t n){auto&b=*static_cast<std::vector<uint8_t>*>(ctx);auto*first=static_cast<const uint8_t*>(p);b.insert(b.end(),first,first+n);return int(WS2_OK);}};VERIFY(f.d->save_state(f.state,&w)==WS2_OK);return bytes;}
static int port_load(DspFixture&f,const std::vector<uint8_t>&bytes){struct Cursor{const std::vector<uint8_t>&b;size_t pos=0;}cursor{bytes};ws2_reader r{sizeof(r),&cursor,[](void*ctx,void*p,uint32_t n){auto&c=*static_cast<Cursor*>(ctx);if(n>c.b.size()-c.pos)return int(WS2_INVALID);std::memcpy(p,c.b.data()+c.pos,n);c.pos+=n;return int(WS2_OK);},uint32_t(bytes.size())};return f.d->load_state(f.state,&r);}
static int utility_port_tests(const char*name,int argc,char**argv){
 VERIFY(argc==2);ModuleLoader loader(argv[1]);auto*old=loader.abi1(name);auto*d=loader.abi2(name);VERIFY(!std::strcmp(d->id,old->id));VERIFY(d->input_count==old->input_count&&d->output_count==old->output_count&&d->param_count==old->parameter_count);
 for(uint32_t p=0;p<d->input_count;++p)VERIFY(!std::strcmp(d->inputs[p].id,old->inputs[p].id));for(uint32_t p=0;p<d->output_count;++p)VERIFY(!std::strcmp(d->outputs[p].id,old->outputs[p].id));
 for(uint32_t p=0;p<d->param_count;++p){VERIFY(d->params[p].legacy_index==old->parameters[p].id);VERIFY(d->params[p].initial==old->parameters[p].initial);VERIFY(d->params[p].minimum==old->parameters[p].minimum&&d->params[p].maximum==old->parameters[p].maximum);VERIFY(d->params[p].flags&WS2_MODULATABLE);}
 for(double rate:{44100.,48000.,96000.})for(uint32_t channels:{1u,4u,16u})for(uint32_t frames:{1u,17u,256u})for(int pattern=0;pattern<4;++pattern){
  auto f=std::make_unique<DspFixture>(d,rate,channels);std::vector<void*>states(channels);ws_module_config cfg{sizeof(cfg),256,rate,1};for(auto&state:states){state=old->create(rate);VERIFY(state&&old->prepare(state,&cfg)==0);}
  uint64_t hash=0,reference=0;
  for(int reset=0;reset<2;++reset){hash=UINT64_C(14695981039346656037);
   for(uint32_t pos=0;pos<1024;){uint32_t n=std::min(frames,1024-pos);
    for(uint32_t p=0;p<d->input_count;++p){f->ins[p].channels=pattern==3?0:channels;f->ins[p].flags=pattern==3?WS2_CONSTANT:WS2_CONNECTED;for(uint32_t c=0;c<channels;++c)for(uint32_t i=0;i<n;++i){double t=pos+i;f->input[p][c][i]=pattern==1?(t==0?1.f:0.f):static_cast<float>(std::sin(pattern==2?.0001*t*t:.13*t+.2*c));}}
    {allocation_watch::Scope watch;f->run(n);VERIFY(watch.allocations()==0);}
    for(uint32_t c=0;c<channels;++c){float result[6][256]{};float*op[6];const float*ip[6];for(int p=0;p<6;++p){op[p]=result[p];ip[p]=pattern==3?nullptr:f->input[p][c];}ws_process_context context{sizeof(context),n,pos,1};old->process(states[c],ip,op,&context);
     for(uint32_t p=0;p<d->output_count;++p)for(uint32_t i=0;i<n;++i){float actual=f->output[p][c][(f->outs[p].flags&WS2_CONSTANT)?0:i];VERIFY(std::isfinite(actual));if(c==0||std::strcmp(name,"noise"))VERIFY(actual==result[p][i]);uint32_t bits;std::memcpy(&bits,&actual,4);hash=(hash^bits)*UINT64_C(1099511628211);}
    }pos+=n;
   }
   if(reset)VERIFY(hash==reference);else reference=hash;
   {allocation_watch::Scope watch;f->reset();VERIFY(watch.allocations()==0);}for(auto state:states)old->reset(state);
  }for(auto state:states)old->destroy(state);
 }
 auto f=std::make_unique<DspFixture>(d,48000,16);f->run(17);auto state=port_save(*f);f->run(17);float expected[6][16][17]{};for(uint32_t p=0;p<d->output_count;++p)for(int c=0;c<16;++c)for(int i=0;i<17;++i)expected[p][c][i]=f->output[p][c][(f->outs[p].flags&WS2_CONSTANT)?0:i];VERIFY(port_load(*f,state)==WS2_OK);f->run(17);
 for(uint32_t p=0;p<d->output_count;++p)for(int c=0;c<16;++c)for(int i=0;i<17;++i)VERIFY(expected[p][c][i]==f->output[p][c][(f->outs[p].flags&WS2_CONSTANT)?0:i]);
 auto current=port_save(*f),future=current;future[0]=99;VERIFY(port_load(*f,future)==WS2_UNSUPPORTED);VERIFY(port_save(*f)==current);VERIFY(port_load(*f,{1})==WS2_INVALID);VERIFY(port_save(*f)==current);if(d->param_count){auto corrupt=current;corrupt.back()=255;VERIFY(port_load(*f,corrupt)==WS2_INVALID);VERIFY(port_save(*f)==current);}
 for(uint32_t p=0;p<d->input_count;++p)for(int c=0;c<16;++c)std::fill_n(f->input[p][c],256,1.e30f);f->run(17);
 std::printf("PASS %s: exact ABI1 impulse/sine/sweep/unpatched, rate/block/channel matrix, reset/state/noalloc/extremes\n",name);return 0;
}
