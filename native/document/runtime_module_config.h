// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../modules2/groove/config.h"
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
 if (type == "sequencing.groove") {
     using namespace groove;
     require(value.size() == 3 && value.contains("version") && value.contains("bpm") && value.contains("tracks"),
             "Groove S0 requires version, bpm and tracks");
     require(value.at("version").is_number_integer() && value.at("version").get<int64_t>() == 1,
             "Unsupported Groove configuration version");
     require(value.at("bpm").is_number() && std::isfinite(value.at("bpm").get<double>()) &&
                 value.at("bpm").get<double>() >= 20 && value.at("bpm").get<double>() <= 300,
             "Groove BPM outside range");
     const auto &tracks = value.at("tracks");
     require(tracks.is_array() && tracks.size() == 1 && tracks[0].is_object() && tracks[0].size() == 1 &&
                 tracks[0].contains("steps"),
             "Groove S0 requires one track with steps");
     const auto &steps = tracks[0].at("steps");
     require(steps.is_array() && steps.size() >= 1 && steps.size() <= 8, "Groove S0 requires 1..8 steps");
     Config config;
     config.bpm = static_cast<float>(value.at("bpm").get<double>());
     config.count = static_cast<uint32_t>(steps.size());
     for (size_t i = 0; i < steps.size(); ++i) {
         const auto &s = steps[i];
         require(s.is_object() && s.contains("kind") && s.at("kind").is_string(), "Invalid Groove step");
         const auto kind = s.at("kind").get<std::string>();
         auto &row = config.steps[i];
         if (kind == "note") {
             require(s.size() == 3 && s.contains("key") && s.contains("velocity") && s.at("key").is_number_integer() &&
                         s.at("velocity").is_number(),
                     "Invalid Groove note");
             const auto key = s.at("key").get<int64_t>();
             const auto velocity = s.at("velocity").get<double>();
             require(key >= 0 && key <= 127 && std::isfinite(velocity) && velocity >= 0 && velocity <= 1,
                     "Groove note outside range");
             row.kind = Note;
             row.key = static_cast<int32_t>(key);
             row.velocity = static_cast<float>(velocity);
         } else {
             require(s.size() == 1, "Unknown Groove rest/tie setting");
             require(kind == "rest" || kind == "tie", "Unknown Groove step kind");
             row.kind = kind == "tie" ? Tie : Rest;
         }
     }
     Config validated;
     require(decode(reinterpret_cast<const uint8_t *>(&config), sizeof(config), validated),
             "Invalid Groove configuration");
     std::vector<uint8_t> bytes(sizeof(config));
     std::memcpy(bytes.data(), &config, sizeof(config));
     return bytes;
 }
 if(type=="midi.in"){
  for(auto it=value.begin();it!=value.end();++it)require((it.key()=="endpoint"||it.key()=="system_name")&&it.value().is_string(),"Invalid MIDI input endpoint configuration");
  if(value.contains("endpoint"))require(!value.at("endpoint").get<std::string>().empty(),"Empty MIDI endpoint");return {};
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
   }else if(key=="min"||key=="max"){
    require(v.is_number(),"CC range endpoints must be numbers");double n=v.get<double>();
    require(std::isfinite(n),"CC range endpoints must be finite");
    if(key=="min"){r.min=n;r.has_min=1;}else{r.max=n;r.has_max=1;}
   }else require(false,"Unknown CC row setting");
  }
 }
 require(wirescape::midi::cc_configuration(nullptr,0,config),"Invalid CC row combination");
 std::vector<uint8_t>bytes(sizeof(config));std::memcpy(bytes.data(),&config,sizeof(config));return bytes;
}
}
