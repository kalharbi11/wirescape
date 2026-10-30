// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../modules2/midi_cc_to_cv/config.h"
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
 require(type=="midi.cc_to_cv","Module config needs a module-specific adapter");
 require(value.size()==1&&value.contains("rows")&&value.at("rows").is_array()&&value.at("rows").size()<=8,"CC configuration requires at most eight rows");
 wirescape::midi::CcConfiguration config;size_t index=0;
 for(const auto&row:value.at("rows")){
  require(row.is_object(),"CC row must be an object");auto&r=config.rows[index++];
  for(auto it=row.begin();it!=row.end();++it){auto key=it.key();auto&v=it.value();
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
