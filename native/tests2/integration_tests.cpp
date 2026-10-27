// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "dsp_fixture.h"
#include "test_modules.h"
#include "module_loader.h"
#include "allocation_watch.h"
#include "wirescape/engine.h"
#include <memory>
#include <vector>
#include <cstring>
using wirescape::engine2::Engine;using wirescape::engine2::Config;
static ws2_event ev(uint32_t type,int key=60,int id=1,float value=1){ws2_event e{};e.struct_size=sizeof(e);e.type=type;e.key=key;e.note_id=id;e.channel=0;e.source_id=1;e.value=value;return e;}
static void u16(FILE*f,uint16_t n){std::fputc(n&255,f);std::fputc(n>>8,f);}static void u32(FILE*f,uint32_t n){u16(f,n&65535);u16(f,n>>16);}
static void wav(const std::string&path,const std::vector<float>&a,uint32_t rate,uint16_t channels=1){
 FILE*f=std::fopen(path.c_str(),"wb");VERIFY(f);std::fwrite("RIFF",1,4,f);u32(f,36+uint32_t(a.size()*4));std::fwrite("WAVEfmt ",1,8,f);u32(f,16);u16(f,3);u16(f,channels);u32(f,rate);u32(f,rate*channels*4);u16(f,channels*4);u16(f,32);std::fwrite("data",1,4,f);u32(f,uint32_t(a.size()*4));VERIFY(std::fwrite(a.data(),4,a.size(),f)==a.size());std::fclose(f);
}
static double pitch(const std::vector<float>&x){double first=-1,last=-1;int crossings=0;for(size_t i=1;i<x.size();++i)if(x[i-1]<0&&x[i]>=0){double at=i-1+double(-x[i-1])/(x[i]-x[i-1]);if(first<0)first=at;last=at;++crossings;}VERIFY(crossings>3);return (crossings-1)*48000./(last-first);}
static void poly(const std::string&dir){
 Config c;c.max_block=256;Engine e(c);VERIFY(e.add_node(1,ws::Module<ws2test::NotesToCv>::descriptor(),{8}));VERIFY(e.add_node(2,ws::Module<ws2ports::Vco>::descriptor()));VERIFY(e.add_node(3,ws::Module<ws2ports::Vcf>::descriptor()));VERIFY(e.add_node(4,ws::Module<ws2ports::Adsr>::descriptor()));VERIFY(e.add_node(5,ws::Module<ws2ports::Vca>::descriptor()));VERIFY(e.add_node(6,ws::Module<ws2ports::Lfo>::descriptor()));
 VERIFY(e.connect(1,0,2,0));VERIFY(e.connect(2,0,3,0));VERIFY(e.connect(1,1,4,0));VERIFY(e.connect(3,1,5,0));VERIFY(e.connect(4,0,5,1));VERIFY(e.modulate(6,0,3,0,.15f));VERIFY(e.output(5,0,0));VERIFY(e.output(5,0,1));VERIFY(e.register_producer()==0);VERIFY(e.prepare());
 const int keys[]={60,64,67};for(int i=0;i<3;++i)VERIFY(e.enqueue(0,1,0,ev(WS2_NOTE_ON,keys[i],i+1),0)==WS2_OK);
 for(int i=0;i<8;++i)VERIFY(e.enqueue(0,1,0,ev(WS2_NOTE_ON,48+i,10+i),12000+i*100)==WS2_OK);
 for(int i=0;i<3;++i)VERIFY(e.enqueue(0,1,0,ev(WS2_NOTE_OFF,keys[i],i+1,0),24000)==WS2_OK);
 for(int i=0;i<8;++i)VERIFY(e.enqueue(0,1,0,ev(WS2_NOTE_OFF,48+i,10+i,0),48000)==WS2_OK);
 std::vector<float>stereo(96000*2),osc[3];for(auto&v:osc)v.reserve(11000);
 float left[256]{},right[256]{};float*out[]={left,right};double peak=0,sum=0,tail=0;uint64_t allocations=0;
 for(uint32_t pos=0;pos<96000;){uint32_t n=std::min(256u,96000-pos);{allocation_watch::Scope watch;VERIFY(e.render(out,2,n)==WS2_OK);allocations+=watch.allocations();}
  VERIFY(e.output_channels(5,0)==8);for(uint32_t i=0;i<n;++i){VERIFY(std::isfinite(left[i]));VERIFY(left[i]==right[i]);stereo[2*(pos+i)]=left[i];stereo[2*(pos+i)+1]=right[i];peak=std::max(peak,double(std::abs(left[i])));sum+=double(left[i])*left[i];if(pos+i>=60000)tail=std::max(tail,double(std::abs(left[i])));
   if(pos+i<11000)for(int v=0;v<3;++v)osc[v].push_back(e.output_sample(2,0,v,i));
   if(pos+i==0){VERIFY(e.output_sample(1,1,0,i)==1);VERIFY(std::abs(e.output_sample(4,0,0,i)-1.f/480)<1e-7);}
   if(pos+i==479)VERIFY(e.output_sample(4,0,0,i)==1);
   if(pos+i>=57599)for(uint32_t v=0;v<8;++v)VERIFY(e.output_sample(4,0,v,i)==0);
  }pos+=n;
 }
 VERIFY(allocations==0);VERIFY(e.meter().faults==0);VERIFY(tail==0);VERIFY(peak>0);double cents[3];for(int v=0;v<3;++v){double hz=pitch(osc[v]),target=440*std::pow(2.,(keys[v]-69.)/12.);cents[v]=1200*std::log2(hz/target);VERIFY(std::abs(cents[v])<=1);}
 wav(dir+"/poly_synth.wav",stereo,48000,2);FILE*f=std::fopen((dir+"/poly_synth.json").c_str(),"w");VERIFY(f);std::fprintf(f,"{\"frames\":96000,\"channels\":2,\"peak\":%.12g,\"rms\":%.12g,\"release_tail_peak\":%.12g,\"allocations\":%llu,\"pitch_cents\":[%.12g,%.12g,%.12g],\"faults\":%llu}\n",peak,std::sqrt(sum/96000),tail,static_cast<unsigned long long>(allocations),cents[0],cents[1],cents[2],static_cast<unsigned long long>(e.meter().faults));std::fclose(f);
 std::puts("PASS offline eight-voice synth: pitch, one-sample envelope boundaries, stealing/release, stereo, zero allocations");
}
static void shim(const char*directory,const std::string&reports){
 ModuleLoader loader(directory);const char*names[]={"noise","clock","function_envelope","slew","vcf","ladder","vca","output"};const ws_module_descriptor*d[8];for(int i=0;i<8;++i)d[i]=loader.abi1(names[i]);
 for(bool scripted:{false,true}){
  ws_engine_config oldcfg{sizeof(oldcfg),48000,256,32,1024};std::unique_ptr<ws_engine,decltype(&ws_engine_destroy)>old(ws_engine_create(&oldcfg),ws_engine_destroy);VERIFY(old);Config c;c.max_block=256;Engine e(c);
  for(uint32_t i=0;i<8;++i){VERIFY(ws_engine_register_module(old.get(),d[i])==WS_OK);VERIFY(ws_engine_add_node(old.get(),i+1,d[i]->id)==WS_OK);VERIFY(e.add_abi1(i+1,d[i]));}
  const uint32_t cables[][4]={{1,0,5,0},{5,1,6,0},{6,0,7,0},{2,0,3,0},{3,0,4,0},{4,0,7,1},{7,0,8,0}};
  for(auto&v:cables){VERIFY(ws_engine_connect(old.get(),v[0],v[1],v[2],v[3])==WS_OK);VERIFY(e.connect(v[0],v[1],v[2],v[3]));}
  VERIFY(e.output(8,0,0));VERIFY(e.register_producer()==0);VERIFY(ws_engine_prepare(old.get(),8,0)==WS_OK);VERIFY(e.prepare());
  struct Init {uint32_t node,param;float value;};const Init initial[]={{1,0,.2f},{2,0,137},{2,1,1},{2,2,.5f},{2,3,1},{3,0,.01f},{3,1,.1f},{3,2,0},{4,0,.02f},{4,1,.02f},{5,0,1800},{5,1,.2f},{6,0,3500},{6,1,.2f},{6,2,.5f},{7,0,1}};
  for(auto&v:initial){VERIFY(ws_engine_initialize_parameter(old.get(),v.node,v.param,v.value)==WS_OK);VERIFY(e.set_parameter(v.node,v.param,v.value));}
  if(scripted){const Init changes[]={{5,0,3000},{7,0,.4f},{2,0,91},{6,0,1500}};uint64_t frame=777;for(auto&v:changes){ws_event o{sizeof(o),frame,v.node,WS_PARAMETER,v.param,v.value,1,1,ws_engine_generation(old.get())};VERIFY(ws_engine_enqueue(old.get(),&o)==WS_OK);auto n=ev(WS2_PARAM,60,1,v.value);n.index=v.param;VERIFY(e.enqueue(0,v.node,WS2_DIRECT_PORT,n,frame)==WS2_OK);frame+=1234;}}
  std::vector<float>baseline(16384),actual(16384);for(uint32_t pos=0;pos<16384;pos+=256){VERIFY(ws_engine_render(old.get(),baseline.data()+pos,256)==WS_OK);float*out[]={actual.data()+pos};VERIFY(e.render(out,1,256)==WS2_OK);}
  double worst=0;for(size_t i=0;i<actual.size();++i)worst=std::max(worst,double(std::abs(actual[i]-baseline[i])));std::printf("shim %s max_abs_error %.12g bit_equal %d\n",scripted?"scripted":"steady",worst,int(std::memcmp(actual.data(),baseline.data(),actual.size()*4)==0));VERIFY(std::memcmp(actual.data(),baseline.data(),actual.size()*4)==0);
  wav(reports+(scripted?"/shim_scripted.wav":"/shim_steady.wav"),baseline,48000);
 }
}
int main(int argc,char**argv){std::setvbuf(stdout,nullptr,_IONBF,0);VERIFY(argc==3);shim(argv[1],argv[2]);poly(argv[2]);}
