// SPDX-License-Identifier: GPL-3.0-only
#include "osc_host.h"
#include <nlohmann/json.hpp>
namespace wirescape::osc {
Host::~Host(){stop();}
bool Host::needs_lan(const std::string&text){auto j=nlohmann::json::parse(text);for(const auto&m:j.at("modules"))if(m.value("module",m.value("type",std::string{}))=="osc.in"&&m.value("config",nlohmann::json::object()).value("lan",false))return true;return false;}
int Host::deliver(void*ctx,uint32_t node,const ws2_event&e,uint64_t ns)noexcept{auto&host=*static_cast<Host*>(ctx);auto&engine=app_engine_native(host.engine_);return engine.enqueue(2,node,WS2_DIRECT_PORT,e,ns,wirescape::engine2::Timing::Timestamped,engine.meter().generation);}
bool Host::start(AppEngine*engine,const std::string&text,uint16_t override_port,std::string&error){stop();try{
 auto j=nlohmann::json::parse(text);if(!authorized_&&needs_lan(text))throw std::runtime_error("LAN OSC requires --confirm-lan or app confirmation");bool routed=false;service_=std::make_unique<Service>(Sink{this,deliver},authorized_);engine_=engine;
 for(const auto&m:j.at("modules"))if(m.value("module",m.value("type",std::string{}))=="osc.in"){
  auto c=m.value("config",nlohmann::json::object());auto port=c.value("port",9000);if(port<1||port>65535)throw std::runtime_error("OSC port outside 1..65535");
  if(!service_->add({m.at("id").get<uint32_t>(),override_port?override_port:static_cast<uint16_t>(port),c.value("lan",false)}))throw std::runtime_error(service_->error());routed=true;
 }
 if(!routed){stop();return true;}if(!service_->start())throw std::runtime_error(service_->error());error.clear();return true;
 }catch(const std::exception&e){error=e.what();stop();return false;}}
void Host::stop(){if(service_){service_->stop();last_statistics_=diagnostics();service_.reset();}engine_=nullptr;}
std::string Host::diagnostics()const{if(!service_&&!last_statistics_.empty())return last_statistics_;auto c=service_?service_->counters():Counters{};return nlohmann::json{{"received",c.received},{"dropped",c.dropped},{"malformed",c.malformed},{"truncated",c.truncated},{"sockets",c.sockets}}.dump();}
}
