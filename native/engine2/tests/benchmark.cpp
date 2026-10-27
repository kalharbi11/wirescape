// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "../../tests2/dsp_fixture.h"
#include "../../tests2/module_loader.h"
#include "wirescape/engine.h"
#include <chrono>
#include <memory>
#include <vector>
using namespace wirescape::engine2;
struct Measurement {double mean=0,worst=0,p99=0;};
template<class F>Measurement measure(F call){
 for(int i=0;i<256;++i)VERIFY(call()==0);
 std::vector<double>times(4000);double sum=0;for(auto&t:times){auto start=std::chrono::steady_clock::now();VERIFY(call()==0);t=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count();sum+=t;}
 std::sort(times.begin(),times.end());return {sum/times.size(),times.back(),times[size_t(times.size()*.99)]};
}
static std::unique_ptr<Engine>make(uint32_t voices,bool mod){Config c;c.max_channels=voices;auto e=std::make_unique<Engine>(c);VERIFY(e->add_node(1,ws::Module<ws2ports::Vco>::descriptor()));VERIFY(e->add_node(2,ws::Module<ws2ports::Vcf>::descriptor()));VERIFY(e->add_node(3,ws::Module<ws2ports::Vca>::descriptor()));VERIFY(e->set_channels(1,voices));VERIFY(e->connect(1,0,2,0));VERIFY(e->connect(2,1,3,0));VERIFY(e->output(3,0,0));if(mod){VERIFY(e->add_node(4,ws::Module<ws2ports::Lfo>::descriptor()));VERIFY(e->modulate(4,0,2,0,.2f));}VERIFY(e->prepare());return e;}
int main(int argc,char**argv){VERIFY(argc==3);ModuleLoader loader(argv[1]);ws_engine_config c{sizeof(c),48000,256,32,1024};std::unique_ptr<ws_engine,decltype(&ws_engine_destroy)>old(ws_engine_create(&c),ws_engine_destroy);VERIFY(old);
 const char*names[]={"vco","vcf","vca"};for(uint32_t i=0;i<3;++i){auto*d=loader.abi1(names[i]);VERIFY(ws_engine_register_module(old.get(),d)==0);VERIFY(ws_engine_add_node(old.get(),i+1,d->id)==0);}VERIFY(ws_engine_connect(old.get(),1,0,2,0)==0);VERIFY(ws_engine_connect(old.get(),2,1,3,0)==0);VERIFY(ws_engine_prepare(old.get(),3,0)==0);
 auto mono=make(1,false),poly=make(8,false),modulated=make(1,true);std::vector<std::unique_ptr<Engine>>eight;for(int i=0;i<8;++i)eight.push_back(make(1,false));
 auto fixed=std::make_unique<DspFixture>(ws::Module<ws2ports::Vcf>::descriptor()),varying=std::make_unique<DspFixture>(ws::Module<ws2ports::Vcf>::descriptor());varying->pars[0].flags=0;for(int i=0;i<256;++i)varying->parameter[0][0][i]=1000+500*std::sin(.03f*i);
 float buffer[256]{};float*out[]={buffer};Measurement rows[7];rows[0]=measure([&](){return ws_engine_render(old.get(),buffer,256);});rows[1]=measure([&](){return mono->render(out,1,256);});rows[2]=measure([&](){return poly->render(out,1,256);});rows[3]=measure([&](){for(auto&e:eight)if(e->render(out,1,256))return 1;return 0;});rows[4]=measure([&](){return modulated->render(out,1,256);});rows[5]=measure([&](){fixed->run();return 0;});rows[6]=measure([&](){varying->run();return 0;});
 FILE*f=std::fopen(argv[2],"w");VERIFY(f);const char*labels[]={"abi1_mono","abi2_mono","abi2_poly8","abi2_eight_mono","abi2_modulated","vcf_constant","vcf_audio_rate_cutoff"};std::fputs("{\"sample_rate\":48000,\"frames\":256,\"warmup_blocks\":256,\"measured_blocks\":4000,\"rows\":[",f);
 for(int i=0;i<7;++i)std::fprintf(f,"%s{\"name\":\"%s\",\"mean_ns\":%.3f,\"worst_ns\":%.3f,\"p99_ns\":%.3f}",i?",":"",labels[i],rows[i].mean,rows[i].worst,rows[i].p99);
 std::fprintf(f,"],\"mono_ratio\":%.9g,\"poly8_vs_eight_mono_ratio\":%.9g,\"modulated_ratio\":%.9g}\n",rows[1].mean/rows[0].mean,rows[2].mean/rows[3].mean,rows[4].mean/rows[1].mean);std::fclose(f);
 std::printf("ABI2/ABI1 mono %.3fx; poly8/eight-mono %.3fx; modulated/unmodulated %.3fx\n",rows[1].mean/rows[0].mean,rows[2].mean/rows[3].mean,rows[4].mean/rows[1].mean);
}
