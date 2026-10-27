// SPDX-License-Identifier: GPL-3.0-only
#include "benchmark_graph.h"
#include "../../tests2/module_loader.h"
#include "wirescape/engine.h"
#include <string>
int main(int argc,char**argv){
 VERIFY(argc==4);const std::string mode=argv[2];char*end=nullptr;auto blocks=std::strtoul(argv[3],&end,10);VERIFY(*argv[3]&&!*end&&blocks>0&&blocks<=1000000);
 float buffer[256]{};float*out[]={buffer};
 if(mode=="abi1_mono"){
  ModuleLoader loader(argv[1]);ws_engine_config c{sizeof(c),48000,256,32,1024};
  std::unique_ptr<ws_engine,decltype(&ws_engine_destroy)>old(ws_engine_create(&c),ws_engine_destroy);VERIFY(old);
  const char*names[]={"vco","vcf","vca"};for(uint32_t i=0;i<3;++i){auto*d=loader.abi1(names[i]);VERIFY(ws_engine_register_module(old.get(),d)==0);VERIFY(ws_engine_add_node(old.get(),i+1,d->id)==0);}
  VERIFY(ws_engine_connect(old.get(),1,0,2,0)==0);VERIFY(ws_engine_connect(old.get(),2,1,3,0)==0);VERIFY(ws_engine_prepare(old.get(),3,0)==0);
  for(unsigned long i=0;i<blocks;++i)VERIFY(ws_engine_render(old.get(),buffer,256)==0);
 }else{
  VERIFY(mode=="abi2_mono"||mode=="abi2_modulated"||mode=="abi2_poly8");
  auto graph=make(mode=="abi2_poly8"?8:1,mode=="abi2_modulated");
  for(unsigned long i=0;i<blocks;++i)VERIFY(graph->render(out,1,256)==0);
 }
}
