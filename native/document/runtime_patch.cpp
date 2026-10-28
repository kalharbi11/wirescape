// SPDX-License-Identifier: GPL-3.0-only
#include "runtime_patch.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
namespace wirescape::document {
namespace {
using Json=nlohmann::json;
void need(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
uint32_t id(const Json&v){need(v.is_number_integer(),"Expected integer ID");auto n=v.get<int64_t>();need(n>0&&n<=UINT32_MAX,"ID outside range");return static_cast<uint32_t>(n);}
std::vector<uint8_t> state_bytes(const Json&s){
 need(s.at("encoding")=="base64"&&s.at("data").is_string(),"Invalid state envelope");
 need(s.at("version").is_number_integer()&&s.at("version").get<int64_t>()>=0,"Invalid state version");
 const auto encoded=s.at("data").get<std::string>();need(encoded.size()%4==0,"Invalid base64 length");std::vector<uint8_t> out;
 const std::string alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(size_t i=0;i<encoded.size();i+=4){uint32_t word=0;int padding=0;
  for(int j=0;j<4;++j){char c=encoded[i+j];if(c=='='){need(j>=2&&i+4==encoded.size(),"Invalid base64 padding");++padding;word<<=6;}
   else{need(!padding,"Invalid base64 padding");auto n=alphabet.find(c);need(n!=std::string::npos,"Invalid base64 character");word=(word<<6)|uint32_t(n);}}
  need(padding<=2,"Invalid base64 padding");out.push_back(uint8_t(word>>16));if(padding<2)out.push_back(uint8_t(word>>8));if(!padding)out.push_back(uint8_t(word));
 }return out;
}
}
AppEngine* load_runtime_patch(const std::string&bytes,AppEngine*catalog,const ws_engine_config&config,std::string&error){
 try{
  need(bytes.size()<=RuntimePatchLimit,"Patch exceeds 2 MiB limit");std::vector<std::set<std::string>> keys;
  auto callback=[&](int depth,Json::parse_event_t event,Json&value){need(depth<=64,"JSON nesting exceeds 64");
   if(event==Json::parse_event_t::object_start)keys.emplace_back();
   if(event==Json::parse_event_t::key)need(keys.back().insert(value.get<std::string>()).second,"Duplicate JSON key");
   if(event==Json::parse_event_t::object_end)keys.pop_back();return true;};
  const auto patch=Json::parse(bytes,callback);need(patch.is_object()&&patch.at("format")=="wirescape.native.patch","Not a native patch");
  need(patch.at("version").is_number_integer(),"Invalid patch version");int version=patch.at("version").get<int>();need(version==1||version==2,"Unsupported patch version");
  const auto&modules=patch.at("modules");const auto&cables=patch.at("connections");need(modules.is_array()&&modules.size()<=64&&cables.is_array()&&cables.size()<=128,"Invalid graph size/types");
  auto native=AppEngineConfig(config); // Internal config, never an ABI extension.
  if(version==2){const auto&settings=patch.at("settings");need(settings.is_object(),"Invalid settings");
   if(settings.contains("sample_rate"))need(settings["sample_rate"].is_number()&&std::isfinite(settings["sample_rate"].get<double>())&&settings["sample_rate"].get<double>()>0,"Invalid saved sample rate");
   if(settings.contains("max_block_frames"))need(id(settings["max_block_frames"])<=4096,"Invalid saved block limit");
   if(settings.contains("timing_mode"))need(settings["timing_mode"]=="immediate"||settings["timing_mode"]=="timestamped","Invalid timing mode");
   if(settings.contains("max_channels")){native.max_channels=id(settings.at("max_channels"));need(native.max_channels<=16,"Invalid channel limit");}
   if(settings.contains("feedback_block")){native.feedback_block=id(settings.at("feedback_block"));need(native.feedback_block<=config.max_block_frames,"Invalid feedback block");}
   if(settings.contains("seed")){need(settings.at("seed").is_number_unsigned(),"Invalid seed");native.seed=settings.at("seed").get<uint64_t>();}
  }
  std::unique_ptr<AppEngine,decltype(&app_engine_destroy)>engine(app_engine_create_config(&config,&native),app_engine_destroy);need(bool(engine),"Cannot create engine2");
  for(uint32_t i=0;i<app_engine_module_count(catalog);++i)need(app_engine_register_module(engine.get(),app_engine_module(catalog,i))==WS_OK,"Cannot register module");
  std::map<uint32_t,const ws_module_descriptor*> nodes;
  for(const auto&m:modules){need(m.is_object(),"Invalid module");const uint32_t node=id(m.at("id"));need(!nodes.count(node),"Duplicate module ID");
   std::string type=m.at(version==1?"type":"module").get<std::string>();need(!m.value("inactive",false),"Inactive module; saved data preserved");
   if(version==2){need(m.at("version").is_number_integer()&&m.at("version").get<int64_t>()>0,"Invalid module version");if(m.contains("config"))need(m["config"].is_object()&&m["config"].empty(),"Module config needs a module-specific adapter");}
   need(app_engine_add_node(engine.get(),node,type.c_str())==WS_OK,"Module unavailable or prepare failed");
   for(uint32_t i=0;i<app_engine_module_count(engine.get());++i){auto*d=app_engine_module(engine.get(),i);if(type==d->id)nodes[node]=d;}
  }
  auto port=[&](uint32_t node,const std::string&name,bool output){need(nodes.count(node),"Missing cable/output module");auto*d=nodes.at(node);auto*p=output?d->outputs:d->inputs;uint32_t n=output?d->output_count:d->input_count;for(uint32_t i=0;i<n;++i)if(name==p[i].id)return i;throw std::runtime_error("Unknown port");};
  auto&graph=app_engine_native(engine.get());
  for(const auto&c:cables){need(c.is_object(),"Invalid cable");std::string kind=c.at("kind").get<std::string>();
   uint32_t from=id(version==1?c.at("fromNode"):c.at("from").at("module")),to=id(version==1?c.at("toNode"):c.at("to").at("module"));
   std::string out=version==1?c.at("fromPort").get<std::string>():c.at("from").at("port").get<std::string>();
   std::string in=version==1?c.at("toPort").get<std::string>():c.at("to").at("port").get<std::string>();
   uint32_t a=port(from,out,true);bool ok=false;
   if(kind=="modulation"){need(c.at("amount").is_number(),"Invalid modulation amount");float amount=c.at("amount").get<float>();need(std::isfinite(amount)&&std::abs(amount)<=1,"Invalid modulation amount");auto*d=graph.descriptor(to);if(d)for(uint32_t i=0;i<d->param_count;++i)if(in==d->params[i].id)ok=graph.modulate(from,a,to,i,amount);}
   else{need(kind=="signal"||kind=="event"||(version==1&&(kind=="audio"||kind=="control")),"Invalid connection kind");uint32_t b=port(to,in,false);auto*ad=graph.descriptor(from);auto*bd=graph.descriptor(to);if(ad&&bd)need((kind=="event")==bool(ad->outputs[a].kind==WS2_EVENT&&bd->inputs[b].kind==WS2_EVENT),"Cable kind mismatch");ok=graph.connect(from,a,to,b);}
   need(ok,"Invalid cable graph");
  }
  if(version==1){uint32_t output=id(patch.at("outputNode"));need(graph.output(output,0,0)&&graph.output(output,0,1),"Invalid output");}
  else{need(patch.at("outputs").is_array()&&!patch.at("outputs").empty(),"Missing outputs");for(const auto&o:patch.at("outputs")){uint32_t node=id(o.at("module"));need(o.at("bus_channel").is_number_integer(),"Invalid bus channel");int bus=o.at("bus_channel").get<int>();need(bus>=0&&bus<16&&graph.output(node,port(node,o.at("port").get<std::string>(),true),uint32_t(bus)),"Invalid output bus");}}
  need(graph.prepare(),"Cannot prepare graph");
  for(const auto&m:modules){uint32_t node=id(m.at("id"));auto*d=nodes.at(node);const auto values=m.value(version==1?"values":"parameters",Json::object());need(values.is_object(),"Invalid parameters");
   for(auto it=values.begin();it!=values.end();++it){bool found=false;for(uint32_t p=0;p<d->parameter_count;++p){bool matches=version==1?it.key()==std::to_string(d->parameters[p].id):it.key()==app_engine_parameter_id(engine.get(),d->id,p);if(!matches)continue;need(it.value().is_number(),"Invalid parameter value");float v=it.value().get<float>();need(std::isfinite(v)&&app_engine_initialize_parameter(engine.get(),node,d->parameters[p].id,v)==WS_OK,"Parameter outside range");found=true;break;}need(version==1||found,"Unknown parameter ID");}
   if(version==2&&m.contains("state"))need(graph.load_state(node,state_bytes(m.at("state")))==WS2_OK,"Unsupported module state");
  }
  error.clear();return engine.release();
 }catch(const std::exception&e){error=e.what();return nullptr;}
}
}
