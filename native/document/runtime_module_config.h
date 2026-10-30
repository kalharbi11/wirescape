// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../modules2/midi_cc_to_cv/config.h"
#include "../modules2/osc_in/config.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>
namespace wirescape::document {
// Quiescent JSON-to-module adapter. The JSON shape, not struct padding, is saved.
inline std::vector<uint8_t> runtime_module_config(const std::string&type,const nlohmann::json&value){
 auto require=[](bool ok,const char*s){if(!ok)throw std::runtime_error(s);};
 require(value.is_object(),"Module config must be an object");if(value.empty())return {};
 if(type=="midi.in"){
  for(auto it=value.begin();it!=value.end();++it)require((it.key()=="endpoint"||it.key()=="system_name")&&it.value().is_string(),"Invalid MIDI input endpoint configuration");
  if(value.contains("endpoint"))require(!value.at("endpoint").get<std::string>().empty(),"Empty MIDI endpoint");return {};
 }
 if(type=="monitor"){
  require(value.size()==1&&value.contains("prefix")&&value["prefix"].is_string()&&value["prefix"].get<std::string>().size()<=128,"Monitor config accepts only a short prefix");return {};
 }
 if(type=="osc.in"){
  wirescape::osc::Configuration config;
  for(auto it=value.begin();it!=value.end();++it){auto&v=it.value();auto key=it.key();
   if(key=="port"){require(v.is_number_integer(),"OSC port must be integer");auto n=v.get<int64_t>();require(n>=1&&n<=65535,"OSC port outside 1..65535");config.port=static_cast<uint32_t>(n);}
   else if(key=="lan"){require(v.is_boolean(),"OSC lan must be boolean");config.lan=v.get<bool>();}
   else if(key=="rows"){
    require(v.is_array()&&v.size()<=8,"OSC accepts at most eight rows");size_t i=0;
    for(const auto&row:v){require(row.is_object(),"OSC row must be object");auto&r=config.rows[i++];
     for(auto field=row.begin();field!=row.end();++field){auto name=field.key();auto&data=field.value();
      if(name=="pattern"){require(data.is_string(),"OSC pattern must be string");auto text=data.get<std::string>();require(!text.empty()&&text[0]=='/'&&text.size()<sizeof(r.pattern)&&text.find(char(0))==std::string::npos,"Invalid OSC pattern length");std::memset(r.pattern,0,sizeof(r.pattern));std::memcpy(r.pattern,text.data(),text.size());}
      else if(name=="argument"){require(data.is_number_integer(),"OSC argument must be integer");auto n=data.get<int64_t>();require(n>=0&&n<32,"OSC argument outside 0..31");r.argument=static_cast<uint32_t>(n);}
      else if(name=="kind"){require(data.is_string(),"OSC row kind must be cv/gate/trigger");auto k=data.get<std::string>();require(k=="cv"||k=="gate"||k=="trigger","Unknown OSC row kind");r.kind=k=="cv"?0:k=="gate"?1:2;}
      else if(name=="stale"){require(data.is_string()&&(data=="hold"||data=="zero"),"OSC stale must be hold/zero");r.stale=data=="zero"?1:0;}
      else{require(data.is_number(),"OSC row range/timing must be numeric");float n=data.get<float>();require(std::isfinite(n),"Nonfinite OSC row setting");
       if(name=="input_min")r.input_min=n;else if(name=="input_max")r.input_max=n;else if(name=="output_min")r.output_min=n;else if(name=="output_max")r.output_max=n;else if(name=="smoothing_ms")r.smoothing_ms=n;else if(name=="timeout_ms")r.timeout_ms=n;else require(false,"Unknown OSC row setting");
      }
     }
    }
   }else require(false,"Unknown OSC setting (host pointers are not document data)");
  }
  require(wirescape::osc::configuration(nullptr,0,config),"Invalid OSC row configuration");std::vector<uint8_t>bytes(sizeof(config));std::memcpy(bytes.data(),&config,sizeof(config));return bytes;
 }
 require(type=="midi.cc_to_cv","Module config needs a module-specific adapter");
 require(value.size()==1&&value.contains("rows")&&value.at("rows").is_array()&&value.at("rows").size()<=8,"CC configuration requires at most eight rows");
 wirescape::midi::CcConfiguration config;size_t index=0;
 for(const auto&row:value.at("rows")){
  require(row.is_object(),"CC row must be an object");auto&r=config.rows[index++];
  for(auto it=row.begin();it!=row.end();++it){const auto& key=it.key();auto&v=it.value();
   if(key=="channel"||key=="controller"){
    require(v.is_number_integer(),"CC channel/controller must be integers");auto n=v.get<int64_t>();require(n>=0&&n<=(key=="channel"?16:127),"CC channel/controller outside range");
    if(key=="channel")r.channel=static_cast<uint8_t>(n);else r.controller=static_cast<uint8_t>(n);
   }else if(key=="fourteen_bit"||key=="bipolar"){
    require(v.is_boolean(),"CC toggles must be boolean");if(key=="fourteen_bit")r.fourteen_bit=v.get<bool>();else r.bipolar=v.get<bool>();
   }else if(key=="smoothing_ms"){
    require(v.is_number(),"CC smoothing must be numeric");double n=v.get<double>();require(std::isfinite(n)&&n>=0&&n<=2000,"CC smoothing outside range");r.smoothing_ms=static_cast<float>(n);
   }else require(false,"Unknown CC row setting");
  }
 }
 require(wirescape::midi::cc_configuration(nullptr,0,config),"Invalid CC row combination");
 std::vector<uint8_t>bytes(sizeof(config));std::memcpy(bytes.data(),&config,sizeof(config));return bytes;
}
}
