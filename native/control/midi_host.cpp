// SPDX-License-Identifier: GPL-3.0-only
#include "midi_host.h"
#include "midi_release.h"
#include "midi_service.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
namespace wirescape::midi {
Host::Host(bool authorized,std::unique_ptr<Backend>fake):backend_(std::move(fake)),authorized_(authorized){if(!backend_&&authorized_)backend_=real_backend(true);}
Host::~Host(){stop();}
std::vector<Endpoint>Host::endpoints(){if(service_)throw std::runtime_error("Stop playback before refreshing MIDI endpoints");return backend_?backend_->endpoints():std::vector<Endpoint>{};}
void Host::select(Endpoint selection,bool enable){if(service_)throw std::runtime_error("Stop playback before changing MIDI endpoints");auto match=[&](const Endpoint&s){return s.name==selection.name&&s.system_name==selection.system_name;};enabled_.erase(std::remove_if(enabled_.begin(),enabled_.end(),match),enabled_.end());if(enable)enabled_.push_back(std::move(selection));}
void Host::restore(const std::string&document){
 if(service_)throw std::runtime_error("Stop playback before restoring MIDI endpoints");
 auto j=nlohmann::json::parse(document);std::vector<Endpoint> staged;
 if(j.contains("devices")){
  if(!j.at("devices").is_array())throw std::runtime_error("Invalid devices array");
  for(auto&d:j.at("devices")){
   if(!d.is_object()||!d.contains("kind")||!d["kind"].is_string()||d["kind"]!="midi")continue;
   if(d.contains("enabled")&&!d["enabled"].is_boolean())throw std::runtime_error("Invalid MIDI enabled flag");
   if(!d.value("enabled",false))continue;
   if(!d.contains("name")||!d["name"].is_string()||d["name"].get<std::string>().empty()||(d.contains("system_name")&&!d["system_name"].is_string()))throw std::runtime_error("Invalid MIDI endpoint identity");
   Endpoint e{d.at("name").get<std::string>(),d.value("system_name",std::string{})};
   if(std::none_of(staged.begin(),staged.end(),[&](const Endpoint&s){return s.name==e.name&&s.system_name==e.system_name;}))staged.push_back(std::move(e));
   if(staged.size()>16)throw std::runtime_error("At most 16 MIDI endpoints may be enabled");
  }
 }enabled_=std::move(staged);
}

int Host::deliver(void*opaque,uint32_t node,const ws2_event&e,uint64_t ns)noexcept{auto&h=*static_cast<Host*>(opaque);auto&engine=app_engine_native(h.engine_);int result=engine.enqueue(1,node,WS2_DIRECT_PORT,e,ns,wirescape::engine2::Timing::Timestamped,engine.meter().generation);if(result!=WS2_OK&&(e.type==WS2_NOTE_CHOKE||wirescape::engine2::midi_release(e)))engine.panic();return result;}
bool Host::start(AppEngine*engine,const std::string&document,std::string&error){stop();try{
 if(enabled_.empty())return true;if(!authorized_||!backend_)throw std::runtime_error("MIDI endpoints require --confirm-midi");
 if(!engine)throw std::runtime_error("No MIDI render engine");
 auto j=nlohmann::json::parse(document);engine_=engine;service_=std::make_unique<Service>(*backend_,Sink{this,deliver});
 for(auto&s:enabled_)if(!service_->enable({s.name,s.system_name}))throw std::runtime_error("Too many enabled MIDI endpoints");
 for(auto&m:j.at("modules")){auto type=m.value("module",m.value("type",std::string{}));if(type!="midi.in")continue;auto c=m.value("config",nlohmann::json::object());auto name=c.value("endpoint",std::string("all_enabled"));Route r;r.node=m.at("id").get<uint32_t>();r.all_enabled=name=="all_enabled";r.endpoint={name,c.value("system_name",std::string{})};if(!service_->subscribe(r))throw std::runtime_error("Too many MIDI routes");}
 if(!service_->start())throw std::runtime_error("MIDI service did not start");error.clear();return true;
 }catch(const std::exception&e){error=e.what();stop();return false;}}
void Host::stop(){if(service_){service_->stop();last_statistics_=diagnostics();service_.reset();}engine_=nullptr;}
std::string Host::diagnostics()const{if(!service_&&!last_statistics_.empty())return last_statistics_;auto c=counters();return nlohmann::json{{"received",c.received},{"dropped",c.dropped},{"malformed",c.malformed},{"oversize",c.oversize},{"ring_full",c.ring_full},{"disconnects",c.disconnects},{"send_fail",c.send_fail}}.dump();}
Counters Host::counters()const noexcept{return service_?service_->counters():Counters{};}
}
