// SPDX-License-Identifier: GPL-3.0-only
#include "app_engine.h"
#include "../modules2/monitor/tap.h"
#include "module_catalog.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <map>
#include <vector>
struct AppEngine {
 struct Entry {
  const ws2_descriptor* native=nullptr;ws_module_descriptor metadata{};
  std::vector<ws_port_descriptor> inputs,outputs;std::vector<ws_parameter_descriptor> params;
 };
 std::vector<AppMonitorTap> monitor_taps;
 std::shared_ptr<ws_engine> legacy{nullptr,ws_engine_destroy};
 std::vector<std::shared_ptr<ws_engine>> library_owners;
 std::deque<Entry> entries;std::map<uint32_t,const Entry*> nodes;
 // Destroy the scheduler before the metadata and libraries its nodes borrow.
 wirescape::engine2::Engine engine;
 std::atomic<float> peak{0};std::atomic<uint64_t> blocks{0};
 std::string error;
 uint32_t egress_budget = 2048;
 explicit AppEngine(const ws_engine_config& c,const wirescape::engine2::Config* override=nullptr):legacy(ws_engine_create(&c),ws_engine_destroy),engine(override?*override:config(c)){
     egress_budget = override ? override->event_budget : c.max_events;
     engine.register_producer();
     engine.register_producer();
     engine.register_producer(); // host keys, MIDI, OSC
     for (auto *d : wirescape::app::builtin_modules()) {
         entries.emplace_back();
         auto &e = entries.back();
         e.native = d;
         for (uint32_t i = 0; i < d->input_count; ++i)
             e.inputs.push_back({d->inputs[i].id, d->inputs[i].name,
                                 static_cast<uint32_t>(d->inputs[i].hint == WS2_AUDIO ? WS_AUDIO : WS_CONTROL)});
         for (uint32_t i = 0; i < d->output_count; ++i)
             e.outputs.push_back({d->outputs[i].id, d->outputs[i].name,
                                  static_cast<uint32_t>(d->outputs[i].hint == WS2_AUDIO ? WS_AUDIO : WS_CONTROL)});
         for (uint32_t i = 0; i < d->param_count; ++i) {
             auto &p = d->params[i];
             e.params.push_back({p.legacy_index, p.name, p.minimum, p.maximum, p.initial, p.unit});
         }
         e.metadata = {sizeof(ws_module_descriptor),
                       WS_ABI_VERSION,
                       d->id,
                       d->name,
                       d->input_count,
                       d->output_count,
                       d->param_count,
                       e.inputs.data(),
                       e.outputs.data(),
                       e.params.data(),
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr,
                       nullptr};
  }
 }
 static wirescape::engine2::Config config(const ws_engine_config& c){wirescape::engine2::Config r;r.sample_rate=c.sample_rate;r.max_block=c.max_block_frames;r.max_nodes=c.max_nodes;r.event_budget=c.max_events;return r;}
 const Entry* find(const char* id)const{for(auto& e:entries)if(std::strcmp(e.metadata.id,id)==0)return &e;return nullptr;}
};
AppEngine* app_engine_create(const ws_engine_config*c){
 if(!c)return nullptr;
 try{return new AppEngine(*c);}catch(...){return nullptr;}
}
wirescape::engine2::Config AppEngineConfig(const ws_engine_config&c){return AppEngine::config(c);}
AppEngine* app_engine_create_config(const ws_engine_config*c,const wirescape::engine2::Config*n){
 if(!c)return nullptr;
 try{return new AppEngine(*c,n);}catch(...){return nullptr;}
}
void app_engine_destroy(AppEngine*e){delete e;}
uint32_t app_engine_abi_version(){return 2;}
const char* app_engine_error(const AppEngine*e){return e->error.empty()?e->engine.error().c_str():e->error.c_str();}
wirescape::engine2::Engine& app_engine_native(AppEngine*e){return e->engine;}
int app_engine_register_module(AppEngine*e,const ws_module_descriptor*d){if(!d)return WS_INVALID;if(e->find(d->id))return WS_OK;e->entries.emplace_back();e->entries.back().metadata=*d;return WS_OK;}
int app_engine_copy_modules(AppEngine*e,const AppEngine*source){
 if(e==source)return WS_OK;
 for(auto&entry:source->entries)if(app_engine_register_module(e,&entry.metadata)!=WS_OK)return WS_INVALID;
 // ABI1 descriptor/callback pointers live in loaded libraries, not in metadata.
 // A replacement graph must retain those libraries after its catalogue dies.
 e->library_owners.push_back(source->legacy);
 e->library_owners.insert(e->library_owners.end(),source->library_owners.begin(),source->library_owners.end());return WS_OK;
}
int app_engine_load_module(AppEngine*e,const char*path){int r=ws_engine_load_module(e->legacy.get(),path);if(r!=WS_OK)return r;for(uint32_t i=0;i<ws_engine_module_count(e->legacy.get());++i)app_engine_register_module(e,ws_engine_module(e->legacy.get(),i));return WS_OK;}
uint32_t app_engine_module_count(const AppEngine*e){return static_cast<uint32_t>(e->entries.size());}
const ws_module_descriptor* app_engine_module(const AppEngine*e,uint32_t i){return i<e->entries.size()?&e->entries[i].metadata:nullptr;}
int app_engine_add_node(AppEngine*e,uint32_t id,const char*type){return app_engine_add_node_config(e,id,type,{});}
int app_engine_add_node_config(AppEngine *e, uint32_t id, const char *type, const std::vector<uint8_t> &config) {
    auto *d = e->find(type);
    if (!d) return WS_NOT_FOUND;
    auto encoded = config;
    std::shared_ptr<wirescape::monitor::Tap> tap;
    if (std::strcmp(type, "monitor") == 0) {
        tap = std::make_shared<wirescape::monitor::Tap>();
        wirescape::monitor::Configuration binding;
        binding.tap = tap.get();
        encoded.resize(sizeof(binding));
        std::memcpy(encoded.data(), &binding, sizeof(binding));
    }
    bool ok = d->native ? e->engine.add_node(id, d->native, encoded)
                        : (encoded.empty() && e->engine.add_abi1(id, &d->metadata));
    if (ok) {
        e->nodes[id] = d;
        if (tap) e->monitor_taps.push_back({id, "", std::move(tap)});
    }
    return ok ? WS_OK : WS_GRAPH;
}
int app_engine_connect(AppEngine*e,uint32_t a,uint32_t ap,uint32_t b,uint32_t bp){return e->engine.connect(a,ap,b,bp)?WS_OK:WS_GRAPH;}
int app_engine_prepare(AppEngine*e,uint32_t n,uint32_t p){return e->engine.output(n,p,0)&&e->engine.prepare()?WS_OK:WS_GRAPH;}
int app_engine_initialize_parameter(AppEngine*e,uint32_t n,uint32_t p,float v){auto it=e->nodes.find(n);if(it==e->nodes.end())return WS_NOT_FOUND;auto*d=it->second;for(uint32_t i=0;i<d->metadata.parameter_count;++i)if(d->metadata.parameters[i].id==p)return e->engine.set_parameter(n,i,v)?WS_OK:WS_INVALID;return WS_INVALID;}
int app_engine_enqueue(AppEngine*e,const ws_event*v){
 if(!e||!v)return WS_INVALID;
 ws2_event x{};x.struct_size=sizeof(x);x.source_id=v->source_id;x.note_id=static_cast<int32_t>(v->note_id);x.channel=0;x.key=static_cast<int32_t>(v->value);x.value=1;
 if(v->type==WS_PARAMETER){
  auto it=e->nodes.find(v->node_id);if(it==e->nodes.end())return WS_NOT_FOUND;
  auto*d=it->second;uint32_t index=0;while(index<d->metadata.parameter_count&&d->metadata.parameters[index].id!=v->parameter_id)++index;
  if(index==d->metadata.parameter_count)return WS_INVALID;x.type=WS2_PARAM;x.index=index;x.value=v->value;
 }
 else if(v->type==WS_GATE){
  // ABI1 gate controls envelopes, never oscillator pitch. Preserve that routing.
  auto it=e->nodes.find(v->node_id);if(it==e->nodes.end())return WS_NOT_FOUND;
  if (it->second->native && std::strcmp(it->second->metadata.id, "essentials.adsr") != 0 &&
      std::strcmp(it->second->metadata.id, "host.keyboard") != 0)
      return WS_OK;
  x.type=v->value>0?WS2_NOTE_ON:WS2_NOTE_OFF;x.key=60;
 }else x.type=v->type==WS_NOTE_ON?WS2_NOTE_ON:WS2_NOTE_OFF;
 int r=e->engine.enqueue(0,v->node_id,WS2_DIRECT_PORT,x,v->frame,wirescape::engine2::Timing::Frames,v->generation);
 return r==WS2_OK?WS_OK:r==WS2_CAPACITY?WS_CAPACITY:WS_INVALID;
}
int app_engine_render_channels(AppEngine *e, float *const *out, uint32_t channels, uint32_t n) {
    int r = e->engine.render(out, channels, n);
    // J has no external MIDI/OSC output dispatcher. Drain its bounded tap queue so
    // unconsumed note releases cannot fill egress and force an emergency reset.
    wirescape::engine2::Outgoing ignored;
    for (uint32_t i = 0; i < e->egress_budget && e->engine.pop_egress(ignored); ++i) {
    }
    if (r != WS2_OK) return WS_CAPACITY;
    float p = 0;
    for (uint32_t c = 0; c < channels; ++c)
        for (uint32_t i = 0; i < n; ++i) p = std::max(p, std::abs(out[c][i]));
    e->peak.store(p);
    e->blocks.fetch_add(1);
    return WS_OK;
}
int app_engine_render(AppEngine*e,float*out,uint32_t n){float* rows[]={out};return app_engine_render_channels(e,rows,1,n);}
int app_engine_reset(AppEngine*e){e->engine.reset();e->peak.store(0);e->blocks.store(0);return WS_OK;}
int app_engine_activate(AppEngine*){return WS_OK;}
int app_engine_deactivate(AppEngine*){return WS_OK;}
int app_engine_panic(AppEngine*e){e->engine.panic();return WS_OK;}
uint64_t app_engine_generation(const AppEngine*e){return e->engine.meter().generation;}
int app_engine_get_meter(const AppEngine*e,ws_meter*m){auto x=e->engine.meter();*m={sizeof(*m),x.frame,e->blocks.load(),x.late,e->peak.load(),0,x.generation,x.dropped,0,0};return WS_OK;}
int app_engine_build_essentials(AppEngine*e,const char*){return app_engine_add_node(e,1,"essentials.vco")||app_engine_add_node(e,2,"essentials.adsr")||app_engine_add_node(e,3,"essentials.vca")||app_engine_add_node(e,4,"essentials.output")||app_engine_connect(e,1,0,3,0)||app_engine_connect(e,2,0,3,1)||app_engine_connect(e,3,0,4,0)||app_engine_prepare(e,4,0)?WS_GRAPH:WS_OK;}

const char* app_engine_parameter_id(const AppEngine*e,const char*type,uint32_t index){for(auto*d:wirescape::app::builtin_modules())if(!std::strcmp(d->id,type))return index<d->param_count?d->params[index].id:"";auto*d=e->find(type);if(!d||index>=d->metadata.parameter_count)return "";thread_local std::string numeric;numeric=std::to_string(d->metadata.parameters[index].id);return numeric.c_str();}

std::vector<AppMonitorTap> app_engine_monitor_taps(AppEngine *e) {
    return e ? e->monitor_taps : std::vector<AppMonitorTap>{};
}
void app_engine_monitor_prefix(AppEngine *e, uint32_t node, const std::string &prefix) {
    for (auto &t : e->monitor_taps)
        if (t.node == node) t.prefix = prefix;
}
