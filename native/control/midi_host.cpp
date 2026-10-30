// SPDX-License-Identifier: GPL-3.0-only
#include "midi_host.h"
#include "midi_release.h"
#include "midi_service.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>
namespace wirescape::midi {
Host::Host(bool authorized, std::unique_ptr<Backend> fake)
    : injected_backend_(bool(fake)), backend_(std::move(fake)), authorized_(authorized) {
    if (!backend_ && authorized_) backend_ = real_backend(true);
}
Host::~Host(){stop();}
void Host::authorize(bool enabled) {
    if (service_) throw std::runtime_error("Stop playback before changing MIDI authorization");
    if (enabled && !backend_) backend_ = real_backend(true);
    if (!enabled && !injected_backend_) backend_.reset();
    authorized_ = enabled;
}
std::vector<Endpoint> Host::endpoints() {
    if (service_) throw std::runtime_error("Stop playback before refreshing MIDI endpoints");
    return authorized_ && backend_ ? backend_->endpoints() : std::vector<Endpoint>{};
}
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

int Host::deliver(void *opaque, uint32_t node, const ws2_event &e, uint64_t ns) noexcept {
    auto &h = *static_cast<Host *>(opaque);
    auto event = e;
    if (std::find(h.keyboard_nodes_.begin(), h.keyboard_nodes_.end(), node) != h.keyboard_nodes_.end() &&
        event.type != WS2_NOTE_CHOKE) {
        if (event.type != WS2_MIDI || !event.ump_words) return WS2_OK;
        const auto word = event.ump[0];
        const auto type = word >> 28, status = (word >> 20) & 15;
        // The global keyboard consumes notes; full controller routing stays in MIDI modules.
        if (((word >> 24) & 15) || (type != 2 && type != 4) || (type == 4 && event.ump_words != 2) ||
            (status != 8 && status != 9))
            return WS2_OK;
        const auto velocity = type == 2 ? word & 127 : event.ump[1] >> 16;
        event.type = status == 8 || (type == 2 && !velocity) ? WS2_NOTE_OFF : WS2_NOTE_ON;
        event.channel = int((word >> 16) & 15);
        event.key = int((word >> 8) & 127);
        event.note_id = -1;
        event.value = float(velocity) / float(type == 2 ? 127 : 65535);
        event.ump_words = event.bytes = 0;
        event.data = nullptr;
    }
    auto &engine = app_engine_native(h.engine_);
    int result = engine.enqueue(1, node, WS2_DIRECT_PORT, event, ns, wirescape::engine2::Timing::Timestamped,
                                engine.meter().generation);
    if (result != WS2_OK &&
        (event.type == WS2_NOTE_OFF || event.type == WS2_NOTE_CHOKE || wirescape::engine2::midi_release(event)))
        engine.panic();
    return result;
}
bool Host::start(AppEngine*engine,const std::string&document,std::string&error){stop();try{
 if(enabled_.empty())return true;if(!authorized_||!backend_)throw std::runtime_error("MIDI endpoints require --confirm-midi");
 if(!engine)throw std::runtime_error("No MIDI render engine");
 auto j=nlohmann::json::parse(document);engine_=engine;service_=std::make_unique<Service>(*backend_,Sink{this,deliver});
 for(auto&s:enabled_)if(!service_->enable({s.name,s.system_name}))throw std::runtime_error("Too many enabled MIDI endpoints");
 for (auto &m : j.at("modules")) {
     const auto type = m.value("module", m.value("type", std::string{}));
     if (type == "host.keyboard") {
         const auto node = m.at("id").get<uint32_t>();
         if (!service_->subscribe({node, {}, true})) throw std::runtime_error("Too many MIDI routes");
         keyboard_nodes_.push_back(node);
         continue;
     }
     if (type != "midi.in") continue;
     auto c = m.value("config", nlohmann::json::object());
     auto name = c.value("endpoint", std::string("all_enabled"));
     Route r;
     r.node = m.at("id").get<uint32_t>();
     r.all_enabled = name == "all_enabled";
     r.endpoint = {name, c.value("system_name", std::string{})};
     if (!service_->subscribe(r)) throw std::runtime_error("Too many MIDI routes");
 }
 if(!service_->start())throw std::runtime_error("MIDI service did not start");error.clear();return true;
 }catch(const std::exception&e){error=e.what();stop();return false;}}
void Host::stop() {
    if (service_) {
        service_->stop();
        last_statistics_ = diagnostics();
        service_.reset();
    }
    engine_ = nullptr;
    keyboard_nodes_.clear();
}
std::string Host::diagnostics() const {
    if (!service_ && !last_statistics_.empty()) return last_statistics_;
    auto c = counters();
    auto ports = nlohmann::json::array();
    if (service_)
        for (const auto &s : service_->endpoint_states())
            ports.push_back({{"name", s.saved.name},
                             {"system_name", s.saved.system_name},
                             {"received", s.received},
                             {"online", s.online}});
    return nlohmann::json{{"received", c.received},   {"dropped", c.dropped},
                          {"malformed", c.malformed}, {"oversize", c.oversize},
                          {"ring_full", c.ring_full}, {"disconnects", c.disconnects},
                          {"send_fail", c.send_fail}, {"ports", ports}}
        .dump();
}
Counters Host::counters()const noexcept{return service_?service_->counters():Counters{};}
}
