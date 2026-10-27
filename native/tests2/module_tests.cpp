// SPDX-License-Identifier: GPL-3.0-only
#include "dsp_fixture.h"
#include <memory>
#include <cstring>
#include <limits>
#ifndef WS2_PORTABLE_TEST
#include "module_loader.h"
#include "allocation_watch.h"
#endif
static const ws2_descriptor*descriptors[]={ws::Module<ws2ports::Vco>::descriptor(),ws::Module<ws2ports::Lfo>::descriptor(),ws::Module<ws2ports::Vcf>::descriptor(),ws::Module<ws2ports::Adsr>::descriptor(),ws::Module<ws2ports::Vca>::descriptor(),ws::Module<ws2ports::Mixer>::descriptor(),ws::Module<ws2ports::Noise>::descriptor(),ws::Module<ws2ports::Output>::descriptor(),ws::Module<ws2ports::ScaleOffset>::descriptor(),ws::Module<ws2ports::Slew>::descriptor()};
static const char*names[]={"vco","lfo","vcf","adsr","vca","mixer","noise","output","scale_offset","slew"};
static void matrix(){
 for(auto*d:descriptors)for(double rate:{44100.,48000.,96000.})for(uint32_t count:{1u,4u,16u})for(uint32_t block:{1u,17u,256u}){
  auto a=std::make_unique<DspFixture>(d,rate,count),b=std::make_unique<DspFixture>(d,rate,count);
  uint64_t reference=0;for(int pass=0;pass<2;++pass){uint64_t hash=UINT64_C(14695981039346656037);
   for(int n=0;n<3;++n){a->run(block);b->run(block);for(uint32_t p=0;p<d->output_count;++p)for(uint32_t c=0;c<count;++c)for(uint32_t i=0;i<block;++i){VERIFY(std::isfinite(a->output[p][c][i]));VERIFY(a->output[p][c][i]==b->output[p][c][i]);uint32_t bits;std::memcpy(&bits,&a->output[p][c][i],4);hash=(hash^bits)*UINT64_C(1099511628211);}}
   if(pass==0)reference=hash;else VERIFY(reference==hash);
   a->reset();b->reset();
  }
 }
 // Scalar reference is the same multiply order as f4, including tail lanes.
 auto a=std::make_unique<DspFixture>(descriptors[4],48000,16);a->run();
 for(uint32_t c=0;c<16;++c)for(uint32_t i=0;i<256;++i)VERIFY(std::abs(a->output[0][c][i]-a->input[0][c][i]*a->input[1][c][i])<=1e-6f);
 // Direct DSP counter proves constant-cutoff caching, independent of the wrapper.
 ws2ports::Vcf v;v.reset();auto f=std::make_unique<DspFixture>(descriptors[2],48000,4);ws::Block block(f->process);v.process(block);VERIFY(v.coefficient_recomputes==4);v.process(block);VERIFY(v.coefficient_recomputes==4);
 f->parameter[0][0][0]=2000;v.process(block);VERIFY(v.coefficient_recomputes==8);
 // Nullable transport, play/stop and locate behavior for synced LFO.
 auto l=std::make_unique<DspFixture>(descriptors[1]);l->parameter[3][0][0]=1;l->run(17);VERIFY(l->output[0][0][1]!=0);
 ws2_transport t{sizeof(t),0,120,.25,0,4,4,0,0};l->process.transport=&t;l->run(17);for(int i=0;i<17;++i)VERIFY(std::abs(l->output[0][0][i]-1)<1e-6);
 t.beat=.5;l->run(17);VERIFY(std::abs(l->output[0][0][0])<1e-6);t.flags=WS2_PLAYING;l->run(17);VERIFY(l->output[0][0][1]<0);
 auto guard=std::make_unique<DspFixture>(descriptors[4]);guard->process.output_count=0;VERIFY(guard->d->process(guard->state,&guard->process)==WS2_INVALID);guard->process.output_count=1;
 guard->input[0][0][0]=std::numeric_limits<float>::quiet_NaN();VERIFY(guard->d->process(guard->state,&guard->process)==WS2_INVALID);VERIFY(guard->outs[0].flags&WS2_CONSTANT);VERIFY(guard->output[0][0][0]==0);
 std::printf("PASS %zu DSP matrix cases, reset/determinism, scalar VCA, VCF counter, nullable/stop/locate transport\n",std::size(descriptors)*27);
}
static void vca_exact(){
 for(double rate:{44100.,48000.,96000.})for(uint32_t channels:{1u,4u,16u})for(uint32_t frames:{1u,17u,256u})for(uint32_t mask=0;mask<8;++mask){
  auto f=std::make_unique<DspFixture>(descriptors[4],rate,channels);f->ins[0].flags=WS2_CONNECTED|((mask&1)?WS2_CONSTANT:0);f->ins[1].flags=WS2_CONNECTED|((mask&2)?WS2_CONSTANT:0);f->pars[0].flags=(mask&4)?WS2_CONSTANT:0;
  for(uint32_t c=0;c<channels;++c)for(uint32_t i=0;i<frames;++i)f->parameter[0][c][i]=.25f+.001f*i;
  f->run(frames);for(uint32_t c=0;c<channels;++c)for(uint32_t i=0;i<((f->outs[0].flags&WS2_CONSTANT)?1:frames);++i){
   float expected=f->input[0][c][(mask&1)?0:i]*f->input[1][c][(mask&2)?0:i]*f->parameter[0][0][(mask&4)?0:i];VERIFY(f->output[0][c][i]==expected);
  }
 }
 std::puts("PASS exact VCA scalar equality across rate/channel/frame/constant matrix");
}
#ifndef WS2_PORTABLE_TEST
static void equivalence(const char*directory,const char*report){
 ModuleLoader loader(directory);FILE*file=std::fopen(report,"w");VERIFY(file);std::fprintf(file,"{\"modules\":[");
 for(uint32_t m=0;m<std::size(descriptors);++m){auto*old=loader.abi1(names[m]);double worst=0;
  for(double rate:{44100.,48000.,96000.})for(uint32_t block:{1u,17u,256u}){
   auto f=std::make_unique<DspFixture>(descriptors[m],rate,1);void*state=old->create(rate);VERIFY(state);ws_module_config cfg{sizeof(cfg),256,rate,1};VERIFY(old->prepare(state,&cfg)==0);
   // ABI1 VCO has no pitch/FM inputs; match its unmodulated frequency path.
   if(m==0){f->ins[0].flags=WS2_CONSTANT;f->ins[1].flags=WS2_CONSTANT;f->input[0][0][0]=f->input[1][0][0]=0;}
   // Event gating equivalence, with no connected gate cable.
   ws2_event on{};on.struct_size=sizeof(on);on.type=WS2_NOTE_ON;on.key=60;on.note_id=1;on.source_id=1;on.channel=0;on.value=1;
   if(m==3){f->ins[0].flags=WS2_CONSTANT;f->input[0][0][0]=0;f->process.events={sizeof(ws2_event_list),1,&on};ws_module_event e{WS_NOTE_ON,0,60,1,1};old->event(state,&e);}
   alignas(32)float out[6][256]{};float*op[6];const float*ip[6];for(int p=0;p<6;++p){op[p]=out[p];ip[p]=f->input[p][0];}
   for(uint32_t pos=0;pos<2048;){uint32_t n=std::min(block,2048-pos);ws_process_context context{sizeof(context),n,pos,1};old->process(state,ip,op,&context);f->run(n);
    for(uint32_t p=0;p<old->output_count;++p)for(uint32_t i=0;i<n;++i)worst=std::max(worst,double(std::abs(out[p][i]-f->output[p][0][(f->outs[p].flags&WS2_CONSTANT)?0:i])));
    f->process.events.count=0;pos+=n;
   }old->destroy(state);
  }
  std::printf("equivalence %s max_abs_error %.9g\n",names[m],worst);std::fprintf(file,"%s{\"name\":\"%s\",\"max_abs_error\":%.12g,\"passed\":%s}",m?",":"",names[m],worst,worst<=1e-6?"true":"false");VERIFY(m<5?worst<=1e-6:worst==0);
 }std::fputs("]}\n",file);std::fclose(file);
}
static void noalloc(){for(auto*d:descriptors){auto f=std::make_unique<DspFixture>(d,48000,16);allocation_watch::Scope watch;for(int i=0;i<16;++i){f->run(17);f->reset();}VERIFY(watch.allocations()==0);}std::puts("PASS zero new/new[]/aligned-new in static SDK/DSP process/reset");}
#endif
int main(int argc,char**argv){matrix();vca_exact();
#ifndef WS2_PORTABLE_TEST
 noalloc();if(argc==3)equivalence(argv[1],argv[2]);
#endif
}
