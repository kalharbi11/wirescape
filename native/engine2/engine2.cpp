// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "midi_release.h"
#include "wirescape_sdk.h"
#include "wirescape/module.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <tuple>
#include <utility>
#if defined(__SSE2__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace wirescape::engine2 {
namespace {
bool release(const ws2_event&e){return e.type==WS2_NOTE_OFF||e.type==WS2_NOTE_CHOKE||midi_release(e);}
struct AlignedDelete {void operator()(float*p)const{::operator delete[](p,std::align_val_t(32));}};
struct Buffer {
    std::unique_ptr<float[],AlignedDelete>memory;
    std::array<float*,WS2_MAX_CHANNELS>data{};
    std::array<const float*,WS2_MAX_CHANNELS>read{};
    uint32_t channels=1,flags=WS2_CONSTANT,stride=0;
    void prepare(uint32_t count,uint32_t frames) {
        stride=(frames+7u)&~7u;
        memory.reset(static_cast<float*>(::operator new[](sizeof(float)*count*stride,std::align_val_t(32))));
        std::fill_n(memory.get(),count*stride,0.f);
        for(uint32_t c=0;c<count;++c){data[c]=memory.get()+c*stride;read[c]=data[c];}
    }
};
struct Packet {
    uint32_t node=0,port=WS2_DIRECT_PORT,producer=0;Timing mode=Timing::Frames;
    uint64_t when=0,generation=1,order=0,mapped_time_ns=0;
    ws2_event event{};std::array<uint8_t,WS2_MAX_PAYLOAD>payload{};
    void fix(){event.data=event.bytes?payload.data():nullptr;}
};
// One producer/one consumer per ring. The registry never changes while running.
struct Ring {
    std::vector<Packet>slots;std::atomic<uint32_t>head{0},tail{0};
    explicit Ring(uint32_t capacity):slots(capacity+1){}
    bool push(const Packet&p)noexcept {
        auto h=head.load(std::memory_order_relaxed),next=(h+1)%static_cast<uint32_t>(slots.size());
        if(next==tail.load(std::memory_order_acquire))return false;
        slots[h]=p;slots[h].fix();head.store(next,std::memory_order_release);return true;
    }
    bool pop(Packet&p)noexcept {
        auto t=tail.load(std::memory_order_relaxed);
        if(t==head.load(std::memory_order_acquire))return false;
        p=slots[t];p.fix();tail.store((t+1)%static_cast<uint32_t>(slots.size()),std::memory_order_release);return true;
    }
    bool peek(Packet&p)const noexcept {
        auto t=tail.load(std::memory_order_relaxed);
        if(t==head.load(std::memory_order_acquire))return false;
        p=slots[t];p.fix();return true;
    }
    void clear()noexcept{tail.store(head.load(std::memory_order_acquire),std::memory_order_release);}
};
struct Producer {
    Ring normal,critical;uint64_t serial=0;
    explicit Producer(uint32_t capacity):normal(capacity),critical(capacity){}
};
struct Item {ws2_event event{};uint64_t frame=0,order=0;};
bool ordered(const Item&a,const Item&b){return std::tie(a.frame,a.order)<std::tie(b.frame,b.order);}
struct Allocation {void*p;size_t alignment,bytes;uint32_t hint;};
struct Parameter {
    ws2_param info{};ws::LinearRamp base;
    std::array<float,WS2_MAX_CHANNELS>offset{};
    bool requested=false;
    double log_min=0,log_ratio=0,exp_curve=0;
    explicit Parameter(ws2_param p):info(p),base(ws::to_normal(p,p.initial)) {
        if(p.taper==WS2_LOG){log_min=std::log(static_cast<double>(p.minimum));log_ratio=std::log(static_cast<double>(p.maximum)/p.minimum);}
        if(p.taper==WS2_EXP)exp_curve=std::expm1(static_cast<double>(p.curve));
    }
    double plain(double x)const noexcept {
        const auto&p=info;x=std::clamp(x,0.,1.);double y=x;
        if(p.taper==WS2_LOG)y=std::exp(log_min+x*log_ratio);
        if(p.taper==WS2_EXP&&std::abs(p.curve)>1.e-8)y=std::expm1(p.curve*x)/exp_curve;
        double value=p.taper==WS2_LOG?y:p.minimum+(p.maximum-p.minimum)*y;
        if(p.step>0)value=p.minimum+std::round((value-p.minimum)/p.step)*p.step;
        return std::clamp(value,static_cast<double>(p.minimum),static_cast<double>(p.maximum));
    }
};
struct Edge;
struct Node {
    uint32_t id=0,index=0,component=0;const ws2_descriptor*d=nullptr;
    const ws_module_descriptor*legacy=nullptr;void*instance=nullptr;
    std::vector<ws2_port>inputs,outputs;
    std::vector<ws2_param>parameters;std::vector<std::string>legacy_ids;
    std::vector<Edge*>incoming,signal_edges;
    std::vector<std::vector<Edge*>>modulation,outgoing;
    std::vector<Parameter>state;std::vector<uint8_t>config,preserved;
    std::vector<Allocation>allocations;
    std::vector<Buffer>in,local_out,global_out,param;
    std::vector<double>modulation_sum;
    std::vector<ws2_signal>in_view,param_view;std::vector<ws2_signal_out>out_view;
    std::vector<std::vector<ws2_event>>port_events;
    std::vector<ws2_event_list>event_view;std::vector<ws2_event_out>event_outputs;
    std::vector<ws2_event>events;std::vector<Item>pending;
    std::vector<const float*>legacy_in;std::vector<float*>legacy_out;
    int forced_channels=-1;uint32_t channels=1,latency=0;
    bool loop=false,touched=false;size_t requested_fast=0,requested_large=0;
    void*emit_context=nullptr;
    ~Node(){clear_instance();}
    void clear_instance() {
        if(instance){if(legacy)legacy->destroy(instance);else if(d)d->destroy(instance);instance=nullptr;}
        for(auto&a:allocations)::operator delete(a.p,std::align_val_t(a.alignment));
        allocations.clear();requested_fast=requested_large=0;
    }
    static void*WS2_CALL allocate(void*ctx,size_t bytes,size_t alignment,uint32_t hint) {
        auto&n=*static_cast<Node*>(ctx);
        if(n.allocations.size()==n.allocations.capacity()||!alignment||(alignment&(alignment-1))||bytes>128*1024*1024)return nullptr;
        alignment=std::max(alignment,size_t(32));bytes=std::max(bytes,size_t(1));
        void*p=::operator new(bytes,std::align_val_t(alignment),std::nothrow);
        if(p){n.allocations.push_back({p,alignment,bytes,hint});if(hint==WS2_LARGE)n.requested_large+=bytes;else n.requested_fast+=bytes;}
        return p;
    }
    ws2_host host(){return {sizeof(ws2_host),this,allocate};}
};
struct Edge {
    uint32_t from,output,to,input;int kind;float amount;
    Node*source=nullptr,*destination=nullptr;bool back=false,removed=false;
    Buffer delay;std::vector<uint32_t>delay_channels;uint32_t head=0;
};
struct Component {std::vector<Node*>nodes;bool loop=false;};
struct Bus {uint32_t node,port,channel;Node*source=nullptr;};
}

struct Engine::Impl {
    Config cfg;std::string error;
    std::vector<std::unique_ptr<Node>>nodes;std::vector<Edge>edges;std::vector<Component>components;
    std::vector<std::pair<uint32_t,Node*>>node_table;
    std::array<uint32_t,32>bus_latency{};
    std::vector<Bus>buses;std::array<std::unique_ptr<Producer>,32>producers;
    uint32_t producer_count=0,dropped_connections=0;
    std::unique_ptr<Producer>egress;
    std::vector<Packet>future;std::vector<uint8_t>arena;
    bool prepared=false;std::atomic_flag rendering=ATOMIC_FLAG_INIT;
    std::atomic<uint64_t>position{0},generation{1},applied{1},event_count{0},dropped{0},coalesced{0},ring_full{0},stale{0},late{0},emergencies{0},arena_full{0},faults{0};
    std::atomic<bool>egress_emergency{false};
    uint64_t anchor_frame=0,anchor_ns=0,sequence=0,block_start=0;
    double frames_per_ns=0,beat=0;
    std::atomic<uint32_t>arena_used{0};
    uint32_t block_frames=0,ordinary_used=0,critical_used=0,feedback_count=0,latency=0;
    size_t setup_bytes=0;
    Node*processing=nullptr;uint32_t processing_start=0,processing_frames=0;
    explicit Impl(Config c):cfg(c),anchor_ns(c.epoch_ns),frames_per_ns(c.sample_rate/1.e9) {
        nodes.reserve(cfg.max_nodes);future.reserve(cfg.future_capacity);
    }
    Node*find(uint32_t id)const {
        if(prepared){auto it=std::lower_bound(node_table.begin(),node_table.end(),id,[](const auto&entry,uint32_t key){return entry.first<key;});return it!=node_table.end()&&it->first==id?it->second:nullptr;}
        for(auto&n:nodes)if(n->id==id)return n.get();return nullptr;
    }
    bool fail(const char*s){error=s;return false;}
    uint64_t map_time(uint64_t ns)const noexcept {
        const bool before=ns<anchor_ns;const uint64_t delta=before?anchor_ns-ns:ns-anchor_ns;
        long double distance;
        if(frames_per_ns==cfg.sample_rate/1.e9&&cfg.sample_rate==std::floor(cfg.sample_rate)){
            const uint64_t rate=static_cast<uint64_t>(cfg.sample_rate),seconds=delta/UINT64_C(1000000000);
            if(seconds>UINT64_MAX/rate)return before?0:UINT64_MAX;
            distance=static_cast<long double>(seconds*rate)+(delta%UINT64_C(1000000000))*rate/UINT64_C(1000000000);
        }else distance=std::floor(static_cast<long double>(delta)*frames_per_ns);
        if(distance>static_cast<long double>(UINT64_MAX/2))return before?0:UINT64_MAX;
        const uint64_t d=static_cast<uint64_t>(distance);
        if(before)return d>anchor_frame?0:anchor_frame-d;
        return d>UINT64_MAX-anchor_frame?UINT64_MAX:anchor_frame+d;
    }
    uint64_t map_frame(uint64_t frame)const noexcept {
        const long double t=static_cast<long double>(anchor_ns)+(static_cast<long double>(frame)-anchor_frame)/frames_per_ns;
        return t<0?0:t>UINT64_MAX?UINT64_MAX:static_cast<uint64_t>(t);
    }
    void emergency()noexcept {
        auto g=generation.load(std::memory_order_acquire);
        if(g==applied.load(std::memory_order_acquire)&&generation.compare_exchange_strong(g,g+1,std::memory_order_acq_rel))++emergencies;
        egress_emergency.store(true,std::memory_order_release);
    }
    void reset_processors()noexcept {
        for(auto&n:nodes){
            if(n->legacy)n->legacy->reset(n->instance);else n->d->reset(n->instance);
            for(auto&p:n->state){p.base.reset();p.offset.fill(0);}n->pending.clear();
        }
        for(auto&e:edges)if(e.back){e.head=0;std::fill(e.delay_channels.begin(),e.delay_channels.end(),0);if(e.delay.memory)std::fill_n(e.delay.memory.get(),cfg.max_channels*e.delay.stride,0.f);}
        future.clear();
    }
    bool topology();
    bool finish_prepare();
    bool setup(Node&);
    bool admit(Node&,uint32_t,const ws2_event&,uint64_t,uint64_t);
    void collect();
    bool route(uint32_t,const ws2_event&);
    void process_node(Node&,uint32_t,uint32_t);
    static int WS2_CALL emit(void*ctx,uint32_t port,const ws2_event*event){
        if(!event||event->struct_size<sizeof(ws2_event))return WS2_INVALID;
        return static_cast<Impl*>(ctx)->route(port,*event)?WS2_OK:WS2_CAPACITY;
    }
};

bool Engine::Impl::setup(Node&n) {
    n.clear_instance();n.allocations.reserve(64);
    ws2_config config{sizeof(ws2_config),cfg.sample_rate,cfg.max_block,cfg.max_channels,cfg.seed+n.id,n.config.data(),static_cast<uint32_t>(n.config.size())};
    if(n.legacy){
        n.instance=n.legacy->create(cfg.sample_rate);
        ws_module_config old{sizeof(old),cfg.max_block,cfg.sample_rate,generation.load()};
        if(!n.instance||n.legacy->prepare(n.instance,&old))return fail("ABI1 shim prepare failed");
        for(auto&p:n.state)if(p.requested){ws_module_event e{WS_PARAMETER,p.info.legacy_index,static_cast<float>(ws::to_plain(p.info,p.base.value())),0,0};n.legacy->event(n.instance,&e);}
    }else{
        auto host=n.host();n.instance=n.d->create(&host);
        if(!n.instance)return fail("ABI2 create failed");
        ws2_ports ports{sizeof(ports),0,0,nullptr,nullptr};
        if(n.d->query_ports(n.instance,&config,&ports)!=WS2_OK||ports.input_count>WS2_MAX_PORTS||ports.output_count>WS2_MAX_PORTS||
           (ports.input_count&&!ports.inputs)||(ports.output_count&&!ports.outputs))return fail("ABI2 query_ports failed");
        n.inputs.clear();n.outputs.clear();
        if(ports.input_count)n.inputs.assign(ports.inputs,ports.inputs+ports.input_count);
        if(ports.output_count)n.outputs.assign(ports.outputs,ports.outputs+ports.output_count);
        if(n.d->prepare(n.instance,&config,&host)!=WS2_OK)return fail("ABI2 prepare failed");
        ws2_memory memory{sizeof(memory),0,0,0};
        if(n.d->memory_requirements(&config,&memory)!=WS2_OK||memory.fast_bytes<n.requested_fast||memory.large_bytes<n.requested_large)return fail("Module memory report understates requested storage");
        n.latency=n.d->latency_frames(n.instance);
    }
    auto check_ports=[&](const std::vector<ws2_port>&ports){
        for(size_t i=0;i<ports.size();++i){const auto&p=ports[i];if(p.struct_size<sizeof(ws2_port)||!p.id||!p.name||
            (p.kind!=WS2_SIGNAL&&p.kind!=WS2_EVENT)||p.poly>WS2_FIRST||!std::isfinite(p.unpatched_value))return false;
            for(size_t j=0;j<i;++j)if(!std::strcmp(ports[j].id,p.id))return false;}
        return true;
    };
    if(!check_ports(n.inputs)||!check_ports(n.outputs))return fail("Invalid/duplicate port descriptor");
    n.in.resize(n.inputs.size());n.local_out.resize(n.outputs.size());n.global_out.resize(n.outputs.size());n.param.resize(n.parameters.size());
    for(auto*buffers:{&n.in,&n.local_out,&n.global_out,&n.param})for(auto&b:*buffers)b.prepare(cfg.max_channels,cfg.max_block);
    n.modulation_sum.resize(size_t(cfg.max_channels)*cfg.max_block);
    n.in_view.resize(n.in.size());n.out_view.resize(n.local_out.size());n.param_view.resize(n.param.size());
    n.port_events.resize(n.inputs.size());n.event_view.resize(n.inputs.size());n.event_outputs.resize(n.outputs.size());
    for(auto&events:n.port_events)events.reserve(cfg.event_budget);
    n.events.reserve(cfg.event_budget);n.pending.reserve(cfg.event_budget);
    n.legacy_in.resize(n.inputs.size());n.legacy_out.resize(n.outputs.size());
    for(auto&p:n.state){p.base.prepare(cfg.sample_rate);p.offset.fill(0);}
    for(uint32_t i=0;i<n.outputs.size();++i)n.event_outputs[i]={sizeof(ws2_event_out),this,i,emit};
    setup_bytes+=n.requested_fast+n.requested_large;
    return true;
}

bool Engine::Impl::finish_prepare() {
    if(!topology())return false;
    for(auto&bus:buses){auto*n=find(bus.node);if(!n||bus.port>=n->outputs.size())return fail("Output no longer exists");bus.source=n;}
    arena.resize(cfg.message_arena_bytes);egress=std::make_unique<Producer>(cfg.ring_capacity);
    prepared=true;applied.store(generation.load());return true;
}

bool Engine::Impl::topology() {
    std::sort(nodes.begin(),nodes.end(),[](const auto&a,const auto&b){return a->id<b->id;});
    for(uint32_t i=0;i<nodes.size();++i)nodes[i]->index=i;
    std::sort(edges.begin(),edges.end(),[](const Edge&a,const Edge&b){return std::tie(a.from,a.output,a.to,a.input,a.kind,a.amount)<std::tie(b.from,b.output,b.to,b.input,b.kind,b.amount);});
    for(auto&e:edges){e.source=find(e.from);e.destination=find(e.to);e.back=false;
        if(e.removed)continue;
        if(!e.source||!e.destination||e.output>=e.source->outputs.size()||
           (e.kind==2?e.input>=e.destination->parameters.size():e.input>=e.destination->inputs.size()))return fail("Unavailable cable endpoint");
        if(e.kind==2){if(e.source->outputs[e.output].kind!=WS2_SIGNAL||!(e.destination->parameters[e.input].flags&WS2_MODULATABLE))return fail("Invalid modulation target");}
        else if(e.source->outputs[e.output].kind!=e.destination->inputs[e.input].kind)return fail("Signal/event kind mismatch");
    }
    node_table.clear();
    for(auto&n:nodes){node_table.emplace_back(n->id,n.get());n->incoming.assign(n->inputs.size(),nullptr);n->signal_edges.clear();n->modulation.assign(n->parameters.size(),{});n->outgoing.assign(n->outputs.size(),{});}
    for(auto&e:edges)if(!e.removed){
        if(e.kind==0)e.destination->incoming[e.input]=&e;
        if(e.kind==2)e.destination->modulation[e.input].push_back(&e);
        if(e.kind==1)e.source->outgoing[e.output].push_back(&e);else e.destination->signal_edges.push_back(&e);
    }
    const uint32_t count=static_cast<uint32_t>(nodes.size());
    std::vector<int>index(count,-1),low(count),stack;std::vector<bool>on(count,false);int next=0;components.clear();
    std::function<void(uint32_t)>visit=[&](uint32_t v){
        index[v]=low[v]=next++;stack.push_back(v);on[v]=true;
        for(auto&e:edges)if(!e.removed&&e.source->index==v){uint32_t w=e.destination->index;if(index[w]<0){visit(w);low[v]=std::min(low[v],low[w]);}else if(on[w])low[v]=std::min(low[v],index[w]);}
        if(low[v]==index[v]){Component component;for(;;){auto w=static_cast<uint32_t>(stack.back());stack.pop_back();on[w]=false;nodes[w]->component=static_cast<uint32_t>(components.size());component.nodes.push_back(nodes[w].get());if(w==v)break;}components.push_back(std::move(component));}
    };
    for(uint32_t i=0;i<count;++i)if(index[i]<0)visit(i);
    feedback_count=0;
    for(auto&component:components){
        std::sort(component.nodes.begin(),component.nodes.end(),[](auto*a,auto*b){return a->id<b->id;});
        component.loop=component.nodes.size()>1;
        std::vector<uint8_t>colour(count,0);
        std::function<void(Node*)>dfs=[&](Node*n){colour[n->index]=1;for(auto&e:edges)if(!e.removed&&e.source==n&&e.destination->component==n->component){auto c=colour[e.destination->index];if(c==1){e.back=true;component.loop=true;++feedback_count;}else if(c==0)dfs(e.destination);}colour[n->index]=2;};
        for(auto*n:component.nodes)if(!colour[n->index])dfs(n);
        std::vector<uint32_t>degree(count,0);for(auto&e:edges)if(!e.removed&&!e.back&&e.source->component==e.destination->component&&e.source->component==component.nodes.front()->component)++degree[e.destination->index];
        std::vector<Node*>sorted;while(sorted.size()<component.nodes.size()){
            Node*chosen=nullptr;for(auto*n:component.nodes)if(degree[n->index]==0&&std::find(sorted.begin(),sorted.end(),n)==sorted.end()){chosen=n;break;}
            if(!chosen)return fail("Feedback back-edge removal did not produce DAG");sorted.push_back(chosen);
            for(auto&e:edges)if(!e.removed&&!e.back&&e.source==chosen&&e.destination->component==chosen->component)--degree[e.destination->index];
        }component.nodes=std::move(sorted);
    }
    std::vector<uint32_t>degree(components.size(),0);for(auto&e:edges)if(!e.removed&&e.source->component!=e.destination->component)++degree[e.destination->component];
    std::vector<Component>ordered_components;std::vector<bool>used(components.size(),false);
    while(ordered_components.size()<components.size()){
        uint32_t chosen=UINT32_MAX,best=UINT32_MAX;
        for(uint32_t i=0;i<components.size();++i)if(!used[i]&&!degree[i]){uint32_t id=UINT32_MAX;for(auto*n:components[i].nodes)id=std::min(id,n->id);if(id<best){best=id;chosen=i;}}
        if(chosen==UINT32_MAX)return fail("Invalid component graph");used[chosen]=true;
        for(auto&e:edges)if(!e.removed&&e.source->component==chosen&&e.destination->component!=chosen)--degree[e.destination->component];
        ordered_components.push_back(std::move(components[chosen]));
    }components=std::move(ordered_components);
    for(auto&e:edges)if(e.back&&e.kind!=1){e.delay.prepare(cfg.max_channels,cfg.feedback_block);e.delay_channels.assign(cfg.feedback_block,0);e.head=0;}
    for(auto&component:components)for(auto*n:component.nodes)n->loop=component.loop;
    std::vector<uint32_t>distance(nodes.size(),0);bus_latency.fill(0);latency=0;
    for(auto&component:components)for(auto*n:component.nodes){
        uint32_t upstream=0;
        for(auto&e:edges)if(!e.removed&&!e.back&&e.kind==0&&e.destination==n)upstream=std::max(upstream,distance[e.source->index]);
        distance[n->index]=static_cast<uint32_t>(std::min<uint64_t>(UINT32_MAX,uint64_t(upstream)+n->latency));
    }
    for(auto&bus:buses){auto*n=find(bus.node);if(n){bus_latency[bus.channel]=std::max(bus_latency[bus.channel],distance[n->index]);latency=std::max(latency,bus_latency[bus.channel]);}}
    return true;
}

bool Engine::Impl::admit(Node&n,uint32_t port,const ws2_event&source,uint64_t frame,uint64_t order) {
    auto e=source;e.port=port;
    const bool critical=release(e);
    bool full=n.pending.size()>=cfg.event_budget||ordinary_used+critical_used>=cfg.event_budget||(!critical&&ordinary_used>=cfg.event_budget-cfg.release_reserve);
    if(full){
        if(e.type==WS2_PARAM)for(auto&old:n.pending)if(old.event.type==WS2_PARAM&&old.event.index==e.index&&old.event.source_id==e.source_id&&old.event.channel==e.channel){old={e,frame,order};++coalesced;return true;}
        if(critical)emergency();else ++dropped;
        return false;
    }
    if(e.bytes){
        if(e.bytes>WS2_MAX_PAYLOAD||!e.data||e.bytes>arena.size()-arena_used){++arena_full;++dropped;return false;}
        std::memcpy(arena.data()+arena_used,e.data,e.bytes);e.data=arena.data()+arena_used;arena_used+=e.bytes;
    }
    n.pending.push_back({e,frame,order});if(critical)++critical_used;else ++ordinary_used;++event_count;return true;
}

void Engine::Impl::collect() {
    for(auto&n:nodes)n->pending.clear();
    Packet p;
    const uint64_t gen=generation.load(std::memory_order_acquire);
    for(uint32_t producer=0;producer<producer_count;++producer){
        auto&source=*producers[producer];
        for(Ring*ring:{&source.critical,&source.normal}){
            // Snapshot-like bounded drain. A busy producer cannot extend this indefinitely.
            for(uint32_t work=0;work<cfg.ring_capacity&&ring->pop(p);++work){
                if(p.generation!=gen){++stale;continue;}
                if(p.mode==Timing::Immediate)p.when=block_start;
                else if(p.mode==Timing::Timestamped){uint64_t f=map_time(p.when);p.when=f>UINT64_MAX-cfg.max_block?UINT64_MAX:f+cfg.max_block;}
                if(future.size()==future.capacity()){if(release(p.event))emergency();else ++dropped;continue;}
                future.push_back(p);
            }
        }
    }
    size_t keep=0;
    for(size_t i=0;i<future.size();++i){
        auto&packet=future[i];packet.fix();
        if(packet.generation!=gen){++stale;continue;}
        if(packet.when>=block_start+block_frames){if(keep!=i)future[keep]=packet;++keep;continue;}
        if(packet.when<block_start){packet.when=block_start;++late;}
        Node*n=find(packet.node);
        if(n)admit(*n,packet.port,packet.event,packet.when,packet.order);
    }future.resize(keep);
}

bool Engine::Impl::route(uint32_t port,const ws2_event&source) {
    if(!processing||port>=processing->outputs.size()||processing->outputs[port].kind!=WS2_EVENT||source.offset>=processing_frames||
       !ws::valid_event(source))return false;
    if(ordinary_used+critical_used>=cfg.event_budget||(!release(source)&&ordinary_used>=cfg.event_budget-cfg.release_reserve)){
        if(release(source))emergency();else ++dropped;return false;
    }
    if(release(source))++critical_used;else ++ordinary_used;++event_count;
    const uint64_t frame=block_start+processing_start+source.offset;
    bool routed=false,okay=true;
    for(auto*edge:processing->outgoing[port]){auto&e=*edge;
        routed=true;const uint64_t destination_frame=frame+(e.back?cfg.feedback_block:0);
        if(destination_frame<block_start+block_frames)okay=admit(*e.destination,e.input,source,destination_frame,(UINT64_C(1)<<63)|sequence++)&&okay;
        else {
            if(future.size()==future.capacity()){if(release(source))emergency();else ++dropped;okay=false;continue;}
            Packet p;p.node=e.to;p.port=e.input;p.event=source;p.when=destination_frame;p.generation=generation.load();p.order=(UINT64_C(1)<<63)|sequence++;
            if(source.bytes)std::memcpy(p.payload.data(),source.data,source.bytes);p.fix();future.push_back(p);
        }
    }
    // Every module output is also available to a fake/external dispatcher. Track C
    // will choose endpoint sinks; this headless slice does not perform I/O.
    Packet out;out.node=processing->id;out.port=port;out.when=frame;out.order=sequence++;out.mapped_time_ns=map_frame(frame);out.generation=generation.load();out.event=source;
    if(source.bytes)std::memcpy(out.payload.data(),source.data,source.bytes);out.fix();
    if(!(release(source)?egress->critical:egress->normal).push(out)){
        if(release(source))emergency();else ++dropped;okay=false;
    }
    return routed?okay:okay;
}

void Engine::Impl::process_node(Node&n,uint32_t start,uint32_t frames) {
    const bool direct=!n.loop&&!n.legacy;
    n.events.clear();for(auto&v:n.port_events)v.clear();
    std::sort(n.pending.begin(),n.pending.end(),ordered);
    for(const auto&item:n.pending)if(item.frame>=block_start+start&&item.frame<block_start+start+frames){
        auto event=item.event;event.offset=static_cast<uint32_t>(item.frame-block_start-start);n.events.push_back(event);
        if(event.port<n.port_events.size())n.port_events[event.port].push_back(event);
    }
    auto raw_channels=[&](const Edge&e){
        if(!e.back)return e.source->global_out[e.output].channels;
        uint32_t count=0;for(uint32_t i=0;i<frames;++i)count=std::max(count,e.delay_channels[(e.head+i)%cfg.feedback_block]);return count;
    };
    auto value=[&](const Edge&e,uint32_t channel,uint32_t i){
        if(e.back){auto pos=(e.head+i)%cfg.feedback_block;auto count=e.delay_channels[pos];if(count==1)channel=0;return channel<count?e.delay.data[channel][pos]:0.f;}
        const auto&buffer=e.source->global_out[e.output];if(buffer.channels==1)channel=0;
        return channel<buffer.channels?buffer.data[channel][(buffer.flags&WS2_CONSTANT)?0:start+i]:0.f;
    };
    uint32_t channels=n.forced_channels>0?static_cast<uint32_t>(n.forced_channels):1;
    if(!n.legacy)for(const auto*edge:n.signal_edges){const auto&e=*edge;if(e.kind==2?(n.parameters[e.input].flags&WS2_PER_VOICE)!=0:n.inputs[e.input].poly==WS2_POLY)channels=std::max(channels,raw_channels(e));}
    channels=std::min(channels,cfg.max_channels);n.channels=channels;
    for(uint32_t j=0;j<n.inputs.size();++j){
        const auto&port=n.inputs[j];auto&buffer=n.in[j];const Edge*source=nullptr;
        if(port.kind==WS2_SIGNAL)source=n.incoming[j];
        uint32_t source_channels=source?raw_channels(*source):0;
        bool connected=source_channels>0;
        bool constant=!connected||(!source->back&&(source->source->global_out[source->output].flags&WS2_CONSTANT));
        buffer.channels=port.kind==WS2_EVENT?0:port.poly==WS2_POLY?channels:1;
        buffer.flags=(constant?WS2_CONSTANT:0)|(connected?WS2_CONNECTED:0);
        const bool alias=direct&&connected&&!source->back&&(source_channels==1||(port.poly==WS2_POLY&&source_channels==buffer.channels));
        for(uint32_t c=0;c<buffer.channels;++c)buffer.read[c]=alias?source->source->global_out[source->output].data[source_channels==1?0:c]:buffer.data[c];
        if(!alias)for(uint32_t c=0;c<buffer.channels;++c)for(uint32_t i=0;i<(constant?1u:frames);++i){
            float sample=port.unpatched_value;
            if(connected){
                if(port.poly==WS2_SUM){sample=0;for(uint32_t k=0;k<source_channels;++k)sample+=value(*source,k,i);}
                else sample=value(*source,port.poly==WS2_FIRST?0:c,i);
            }
            buffer.data[c][i]=sample;
        }
        n.in_view[j]={sizeof(ws2_signal),buffer.read.data(),buffer.channels,buffer.flags};
        n.event_view[j]={sizeof(ws2_event_list),static_cast<uint32_t>(n.port_events[j].size()),n.port_events[j].data()};
    }
    if(!n.legacy)for(uint32_t j=0;j<n.parameters.size();++j){
        auto&p=n.state[j];auto&buffer=n.param[j];bool poly=false,constant=p.base.constant();
        for(uint32_t c=1;c<channels;++c)poly|=p.offset[c]!=p.offset[0];
        for(const auto*edge:n.modulation[j]){const auto&e=*edge;poly|=raw_channels(e)>1;constant&=!e.back&&(e.source->global_out[e.output].flags&WS2_CONSTANT);}
        for(const auto&e:n.events)if((e.type==WS2_PARAM||e.type==WS2_PARAM_MOD)&&e.index==j){constant=false;if(e.type==WS2_PARAM_MOD&&e.channel>=0)poly=true;}
        buffer.channels=(poly&&(p.info.flags&WS2_PER_VOICE))?channels:1;buffer.flags=constant?WS2_CONSTANT:0;
        uint32_t cursor=0;const uint32_t length=constant?1:frames;
        const bool modulated=!n.modulation[j].empty();
        if(modulated)for(uint32_t c=0;c<buffer.channels;++c){
            double*sum=n.modulation_sum.data()+size_t(c)*cfg.max_block;std::fill_n(sum,length,0.);
            for(const auto*edge:n.modulation[j])for(uint32_t i=0;i<length;++i)sum[i]+=edge->amount*value(*edge,c,i);
        }
        bool actually_constant=true;
        for(uint32_t i=0;i<length;++i){
            while(cursor<n.events.size()&&n.events[cursor].offset<=i){const auto&e=n.events[cursor++];
                if(e.index==j&&e.type==WS2_PARAM){double target=ws::to_normal(p.info,e.value);if(p.info.flags&WS2_STEPPED)p.base.jump(target);else p.base.set(target);p.requested=true;}
                if(e.index==j&&e.type==WS2_PARAM_MOD){if(e.channel<0)p.offset.fill(e.value);else if(static_cast<uint32_t>(e.channel)<cfg.max_channels)p.offset[e.channel]=e.value;}
            }
            const double base=p.base.next();
            for(uint32_t c=0;c<buffer.channels;++c){
                double effective=base+p.offset[c];
                if(modulated)effective+=n.modulation_sum[size_t(c)*cfg.max_block+i];
                buffer.data[c][i]=static_cast<float>(p.plain(effective));
                if(i&&buffer.data[c][i]!=buffer.data[c][0])actually_constant=false;
            }
        }
        if(actually_constant)buffer.flags|=WS2_CONSTANT;
        n.param_view[j]={sizeof(ws2_signal),buffer.read.data(),buffer.channels,buffer.flags};
    }
    for(uint32_t j=0;j<n.outputs.size();++j)n.out_view[j]={sizeof(ws2_signal_out),(direct?n.global_out[j]:n.local_out[j]).data.data(),channels,0,cfg.max_channels};
    processing=&n;processing_start=start;processing_frames=frames;
    if(n.forced_channels==0){for(auto&o:n.out_view){o.channels=0;o.flags=WS2_CONSTANT;}}
    else if(n.legacy){
        uint32_t cursor=0;
        auto span=[&](uint32_t end){if(end<=cursor)return;
            for(uint32_t j=0;j<n.in.size();++j){auto&b=n.in[j];if(b.flags&WS2_CONSTANT)std::fill_n(b.data[0],frames,b.data[0][0]);n.legacy_in[j]=b.data[0]+cursor;}
            for(uint32_t j=0;j<n.local_out.size();++j)n.legacy_out[j]=n.local_out[j].data[0]+cursor;
            ws_process_context c{sizeof(c),end-cursor,block_start+start+cursor,generation.load()};
            n.legacy->process(n.instance,n.legacy_in.data(),n.legacy_out.data(),&c);cursor=end;
        };
        for(const auto&e:n.events){span(e.offset);
            ws_module_event old{};old.source_id=e.source_id;old.note_id=static_cast<uint32_t>(e.note_id);old.parameter_id=e.index;old.value=e.value;
            if(e.type==WS2_PARAM){old.type=WS_PARAMETER;if(e.index<n.state.size()){auto&v=n.state[e.index];old.parameter_id=v.info.legacy_index;v.base.jump(ws::to_normal(v.info,e.value));v.requested=true;}}
            else if(e.type==WS2_NOTE_ON){old.type=WS_NOTE_ON;old.value=static_cast<float>(e.key);}
            else if(e.type==WS2_NOTE_OFF||e.type==WS2_NOTE_CHOKE)old.type=WS_NOTE_OFF;
            else continue;
            n.legacy->event(n.instance,&old);
        }span(frames);
        bool finite=true;
        for(auto&o:n.out_view){o.channels=1;o.flags=0;finite&=ws::finite_samples(o.data[0],frames);}
        if(!finite){++faults;for(auto&o:n.out_view){o.flags=WS2_CONSTANT;o.data[0][0]=0;}}
    }else{
        ws2_transport transport{sizeof(ws2_transport),cfg.playing?WS2_PLAYING:0u,cfg.bpm,
            beat+(cfg.playing?start*cfg.bpm/(60.*cfg.sample_rate):0),0,4,4,0,0};
        transport.bar_start=std::floor(transport.beat/4)*4;
        ws2_process context{sizeof(ws2_process),frames,block_start+start,generation.load(),cfg.sample_rate,channels,
            static_cast<uint32_t>(n.inputs.size()),static_cast<uint32_t>(n.outputs.size()),static_cast<uint32_t>(n.parameters.size()),
            n.in_view.data(),n.out_view.data(),n.param_view.data(),n.event_view.data(),n.event_outputs.data(),
            {sizeof(ws2_event_list),static_cast<uint32_t>(n.events.size()),n.events.data()},&transport};
        if(n.d->process(n.instance,&context)!=WS2_OK){++faults;for(auto&o:n.out_view){o.channels=channels;o.flags=WS2_CONSTANT;for(uint32_t c=0;c<channels;++c)o.data[c][0]=0;}}
    }
    processing=nullptr;
    for(uint32_t j=0;j<n.outputs.size();++j){
        auto&o=n.out_view[j];auto&global=n.global_out[j];
        if(direct){global.channels=o.channels;global.flags=o.flags;continue;}
        if(!n.touched){global.channels=o.channels;global.flags=o.flags;}
        else{
            if(global.channels!=o.channels)global.flags=0;
            if(!(o.flags&WS2_CONSTANT))global.flags=0;
            if(global.flags&WS2_CONSTANT)for(uint32_t c=0;c<o.channels;++c)if(global.data[c][0]!=o.data[c][0])global.flags=0;
            global.channels=std::max(global.channels,o.channels);
        }
        for(uint32_t c=0;c<global.channels;++c)for(uint32_t i=0;i<frames;++i)global.data[c][start+i]=c<o.channels?o.data[c][(o.flags&WS2_CONSTANT)?0:i]:0;
    }n.touched=true;
}

Engine::Engine(Config config):p_(std::make_unique<Impl>(config)){}
Engine::~Engine()=default;
bool Engine::add_node(uint32_t id,const ws2_descriptor*d,const std::vector<uint8_t>&config){
    auto&p=*p_;if(!p.cfg.max_channels||p.cfg.max_channels>16||!p.cfg.max_block||p.cfg.max_block>4096)return p.fail("Invalid setup bounds");
    if(p.prepared||!id||p.find(id)||p.nodes.size()>=p.cfg.max_nodes||!d||d->struct_size<sizeof(ws2_descriptor)||d->abi_version!=2||!d->id||!d->name||
        !d->create||!d->destroy||!d->query_ports||!d->memory_requirements||!d->prepare||!d->reset||!d->process||!d->save_state||!d->load_state||!d->latency_frames||
        d->input_count>WS2_MAX_PORTS||d->output_count>WS2_MAX_PORTS||d->param_count>WS2_MAX_PARAMS||(d->input_count&&!d->inputs)||(d->output_count&&!d->outputs)||(d->param_count&&!d->params))return p.fail("Invalid ABI2 node");
    auto n=std::make_unique<Node>();n->id=id;n->d=d;n->config=config;
    if(d->input_count)n->inputs.assign(d->inputs,d->inputs+d->input_count);if(d->output_count)n->outputs.assign(d->outputs,d->outputs+d->output_count);
    if(d->param_count)n->parameters.assign(d->params,d->params+d->param_count);
    for(const auto&info:n->parameters)n->state.emplace_back(info);
    // Resolve configuration-dependent ports quiescently before cable authoring.
    if(!p.setup(*n))return false;
    p.nodes.push_back(std::move(n));return true;
}
bool Engine::add_abi1(uint32_t id,const ws_module_descriptor*d){
    auto&p=*p_;if(p.prepared||!id||p.find(id)||p.nodes.size()>=p.cfg.max_nodes||!d||d->struct_size<sizeof(ws_module_descriptor)||d->abi_version!=WS_ABI_VERSION||!d->create||!d->prepare||!d->process||!d->reset||!d->destroy||!d->event)return p.fail("Invalid ABI1 node");
    auto n=std::make_unique<Node>();n->id=id;n->legacy=d;n->legacy_ids.resize(d->parameter_count);
    for(uint32_t i=0;i<d->input_count;++i){auto&v=d->inputs[i];n->inputs.push_back(ws::port(v.id,v.name,v.kind==WS_AUDIO?WS2_AUDIO:WS2_ANY,WS2_FIRST,!std::strcmp(v.id,"gain_cv")?1.f:0.f));}
    for(uint32_t i=0;i<d->output_count;++i){auto&v=d->outputs[i];n->outputs.push_back(ws::port(v.id,v.name,v.kind==WS_AUDIO?WS2_AUDIO:WS2_ANY));}
    for(uint32_t i=0;i<d->parameter_count;++i){const auto&v=d->parameters[i];n->legacy_ids[i]=std::to_string(v.id);n->parameters.push_back(ws::param(n->legacy_ids[i].c_str(),v.name,v.minimum,v.maximum,v.initial,v.unit,WS2_STEPPED,WS2_LINEAR,v.id));n->state.emplace_back(n->parameters.back());}
    p.nodes.push_back(std::move(n));return true;
}
bool Engine::connect(uint32_t from,uint32_t output,uint32_t to,uint32_t input){
    auto&p=*p_;auto*a=p.find(from),*b=p.find(to);if(p.prepared||!a||!b||output>=a->outputs.size()||input>=b->inputs.size())return p.fail("Invalid connection");
    auto kind=a->outputs[output].kind;if(kind!=b->inputs[input].kind)return p.fail("Signal/event kind mismatch");
    if(kind==WS2_SIGNAL)for(auto&e:p.edges)if(e.kind==0&&e.to==to&&e.input==input)return p.fail("Signal input already connected");
    p.edges.push_back({from,output,to,input,kind==WS2_EVENT?1:0,0});return true;
}
bool Engine::modulate(uint32_t from,uint32_t output,uint32_t to,uint32_t param,float amount){
    auto&p=*p_;auto*a=p.find(from),*b=p.find(to);if(p.prepared||!a||!b||output>=a->outputs.size()||param>=b->parameters.size()||a->outputs[output].kind!=WS2_SIGNAL||!(b->parameters[param].flags&WS2_MODULATABLE)||!std::isfinite(amount)||amount< -1||amount>1)return p.fail("Invalid modulation cable");
    p.edges.push_back({from,output,to,param,2,amount});return true;
}
bool Engine::output(uint32_t node,uint32_t port,uint32_t channel){
    auto&p=*p_;auto*n=p.find(node);if(p.prepared||!n||port>=n->outputs.size()||n->outputs[port].kind!=WS2_SIGNAL||channel>=32)return p.fail("Invalid output bus");
    p.buses.push_back({node,port,channel});return true;
}
int Engine::register_producer(){auto&p=*p_;if(p.prepared||p.producer_count==32||!p.cfg.ring_capacity||p.cfg.ring_capacity>65536)return -1;uint32_t i=p.producer_count++;p.producers[i]=std::make_unique<Producer>(p.cfg.ring_capacity);return static_cast<int>(i);}
bool Engine::prepare(){
    auto&p=*p_;if(p.prepared)return p.fail("Prepare is quiescent and single-use; reconfigure uses staged replacement");
    if(!std::isfinite(p.cfg.sample_rate)||p.cfg.sample_rate<1000||p.cfg.sample_rate>384000||!p.cfg.max_channels||p.cfg.max_channels>16||!p.cfg.max_block||p.cfg.max_block>4096||!p.cfg.max_nodes||p.cfg.max_nodes>1024||
       p.cfg.event_budget<2||p.cfg.event_budget>65536||!p.cfg.release_reserve||p.cfg.release_reserve>=p.cfg.event_budget||!p.cfg.ring_capacity||p.cfg.ring_capacity>65536||!p.cfg.future_capacity||p.cfg.future_capacity>65536||
       !p.cfg.feedback_block||p.cfg.feedback_block>p.cfg.max_block||p.cfg.message_arena_bytes>64*1024*1024)return p.fail("Invalid engine2 configuration");
    p.setup_bytes=0;
    for(auto&n:p.nodes)if(!p.setup(*n))return false;
    return p.finish_prepare();
}
bool Engine::set_parameter(uint32_t node,uint32_t index,float plain){auto&p=*p_;auto*n=p.find(node);if(!n||index>=n->parameters.size()||!std::isfinite(plain))return false;auto&v=n->state[index];if(plain<v.info.minimum||plain>v.info.maximum)return false;
    if(p.prepared&&p.position.load()!=0)return p.fail("Use PARAM ingress for live changes");
    v.base.set(ws::to_normal(v.info,plain));v.requested=true;
    if(n->legacy&&n->instance){ws_module_event e{WS_PARAMETER,v.info.legacy_index,plain,0,0};n->legacy->event(n->instance,&e);}return true;
}
bool Engine::set_channels(uint32_t node,uint32_t channels){auto*n=p_->find(node);if(!n||channels>p_->cfg.max_channels||(n->legacy&&channels!=1))return false;n->forced_channels=static_cast<int>(channels);return true;}

int Engine::enqueue(uint32_t producer,uint32_t node,uint32_t port,const ws2_event&event,uint64_t time,Timing mode,uint64_t wanted_generation)noexcept {
    auto&p=*p_;if(!p.prepared||producer>=p.producer_count)return WS2_STATE;
    const auto gen=p.generation.load(std::memory_order_acquire);
    if(gen!=p.applied.load(std::memory_order_acquire)||(wanted_generation&&wanted_generation!=gen))return WS2_STATE;
    auto*n=p.find(node);
    if(!n||!ws::valid_event(event))return WS2_INVALID;
    if(port!=WS2_DIRECT_PORT&&(port>=n->inputs.size()||n->inputs[port].kind!=WS2_EVENT))return WS2_INVALID;
    if(event.type==WS2_PARAM||event.type==WS2_PARAM_MOD){
        if(event.index>=n->parameters.size())return WS2_INVALID;
        if(event.type==WS2_PARAM&&(event.value<n->parameters[event.index].minimum||event.value>n->parameters[event.index].maximum))return WS2_INVALID;
    }
    auto&source=*p.producers[producer];
    Packet packet;packet.node=node;packet.port=port;packet.when=time;packet.mode=mode;packet.producer=producer;packet.generation=gen;
    packet.order=(static_cast<uint64_t>(producer)<<48)|(source.serial++&UINT64_C(0x0000ffffffffffff));packet.event=event;
    if(event.bytes)std::memcpy(packet.payload.data(),event.data,event.bytes);packet.fix();
    if(!(release(event)?source.critical:source.normal).push(packet)){
        ++p.ring_full;if(release(event))p.emergency();else ++p.dropped;return WS2_CAPACITY;
    }return WS2_OK;
}

bool Engine::publish_clock(uint64_t frame,uint64_t ns){
    auto&p=*p_;if(frame<p.anchor_frame||ns<p.anchor_ns)return false;
    if(frame>p.anchor_frame&&ns>p.anchor_ns){
        double measured=static_cast<double>(frame-p.anchor_frame)/static_cast<double>(ns-p.anchor_ns);
        double nominal=p.cfg.sample_rate/1.e9;
        measured=std::clamp(measured,nominal*.999,nominal*1.001);
        p.frames_per_ns+=(measured-p.frames_per_ns)/16.;
    }p.anchor_frame=frame;p.anchor_ns=ns;return true;
}

int Engine::render(float*const*outputs,uint32_t output_channels,uint32_t frames,uint64_t callback_time_ns)noexcept {
    auto&p=*p_;if(!p.prepared||!frames||frames>p.cfg.max_block||output_channels>32||(output_channels&&!outputs))return WS2_INVALID;
    for(uint32_t c=0;c<output_channels;++c)if(!outputs[c])return WS2_INVALID;
    if(p.rendering.test_and_set(std::memory_order_acquire))return WS2_STATE;
    struct Guard {std::atomic_flag&flag;
#if defined(__SSE2__) || defined(_M_X64)
        uint32_t fp;
        explicit Guard(std::atomic_flag&f):flag(f),fp(_mm_getcsr()){_mm_setcsr(fp|0x8040);}
        ~Guard(){_mm_setcsr(fp);flag.clear(std::memory_order_release);}
#elif defined(__aarch64__) && !defined(_MSC_VER)
        uint64_t fp;
        explicit Guard(std::atomic_flag&f):flag(f){asm volatile("mrs %0, fpcr":"=r"(fp));uint64_t value=fp|(UINT64_C(1)<<24);asm volatile("msr fpcr, %0"::"r"(value));}
        ~Guard(){asm volatile("msr fpcr, %0"::"r"(fp));flag.clear(std::memory_order_release);}
#else
        explicit Guard(std::atomic_flag&f):flag(f){}~Guard(){flag.clear(std::memory_order_release);}
#endif
    }guard(p.rendering);
    p.block_start=p.position.load(std::memory_order_relaxed);p.block_frames=frames;
    if(callback_time_ns!=UINT64_MAX)publish_clock(p.block_start,callback_time_ns);
    if(p.generation.load()!=p.applied.load()){p.reset_processors();p.applied.store(p.generation.load(),std::memory_order_release);}
    p.arena_used.store(0);p.ordinary_used=p.critical_used=0;
    for(auto&n:p.nodes)n->touched=false;
    p.collect();
    if(p.generation.load()!=p.applied.load()){p.reset_processors();p.applied.store(p.generation.load(),std::memory_order_release);}
    for(auto&component:p.components){
        uint32_t chunk=component.loop?p.cfg.feedback_block:frames;
        for(uint32_t start=0;start<frames;start+=chunk){uint32_t length=std::min(chunk,frames-start);
            for(auto*n:component.nodes)p.process_node(*n,start,length);
            if(component.loop)for(auto&e:p.edges)if(e.back&&e.kind!=1&&e.source->component==component.nodes.front()->component){
                auto&source=e.source->global_out[e.output];
                for(uint32_t i=0;i<length;++i){auto at=(e.head+i)%p.cfg.feedback_block;e.delay_channels[at]=source.channels;
                    for(uint32_t c=0;c<p.cfg.max_channels;++c)e.delay.data[c][at]=c<source.channels?source.data[c][start+i]:0;}
                e.head=(e.head+length)%p.cfg.feedback_block;
            }
        }
    }
    for(uint32_t c=0;c<output_channels;++c)std::fill_n(outputs[c],frames,0.f);
    bool bus_written[32]{};
    for(auto&bus:p.buses)if(bus.channel<output_channels){const auto&buffer=bus.source->global_out[bus.port];
        for(uint32_t c=0;c<buffer.channels;++c){
            for(uint32_t i=0;i<frames;++i){const float sample=buffer.data[c][(buffer.flags&WS2_CONSTANT)?0:i];
                if(!bus_written[bus.channel])outputs[bus.channel][i]=sample;else outputs[bus.channel][i]+=sample;}
            bus_written[bus.channel]=true;
        }
    }
    if(p.cfg.playing)p.beat+=frames*p.cfg.bpm/(60.*p.cfg.sample_rate);
    p.position.store(p.block_start+frames,std::memory_order_release);
    if(p.generation.load()!=p.applied.load()){p.reset_processors();p.applied.store(p.generation.load(),std::memory_order_release);}
    return WS2_OK;
}

bool Engine::pop_egress(Outgoing&out)noexcept {
    auto&p=*p_;if(!p.egress)return false;
    if(p.egress_emergency.exchange(false,std::memory_order_acq_rel)){
        out={};out.generation=p.generation.load();out.frame=p.position.load();out.time_ns=0;
        out.event.struct_size=sizeof(ws2_event);out.event.type=WS2_NOTE_CHOKE;out.event.note_id=-1;out.event.channel=-1;out.event.source_id=UINT32_MAX;out.event.flags=1;return true;
    }
    Packet packet,normal,critical;
    for(uint32_t work=0;work<2*p.cfg.ring_capacity;++work){
        bool a=p.egress->normal.peek(normal),b=p.egress->critical.peek(critical);
        if(!a&&!b)return false;
        Ring&ring=(!a||(b&&std::tie(critical.when,critical.order)<std::tie(normal.when,normal.order)))?p.egress->critical:p.egress->normal;
        if(!ring.pop(packet))continue;
        if(packet.generation!=p.generation.load()){++p.stale;continue;}
        out.node=packet.node;out.port=packet.port;out.frame=packet.when;out.time_ns=packet.mapped_time_ns;out.generation=packet.generation;out.event=packet.event;
        if(out.event.bytes)std::memcpy(out.payload,packet.payload.data(),out.event.bytes);out.event.data=out.event.bytes?out.payload:nullptr;return true;
    }return false;
}
void Engine::panic()noexcept {p_->generation.fetch_add(1,std::memory_order_acq_rel);p_->egress_emergency.store(true,std::memory_order_release);}
void Engine::reset(){auto&p=*p_;for(uint32_t i=0;i<p.producer_count;++i){p.producers[i]->normal.clear();p.producers[i]->critical.clear();}if(p.egress){p.egress->normal.clear();p.egress->critical.clear();}p.reset_processors();p.position.store(0);p.beat=0;p.anchor_frame=0;p.anchor_ns=p.cfg.epoch_ns;p.frames_per_ns=p.cfg.sample_rate/1.e9;p.generation.fetch_add(1);p.applied.store(p.generation.load());}
void Engine::transport(double bpm,bool playing,double beat){if(std::isfinite(bpm)&&bpm>0&&std::isfinite(beat)){p_->cfg.bpm=bpm;p_->cfg.playing=playing;p_->beat=beat;}}
Meter Engine::meter()const noexcept {
    auto&p=*p_;return {p.position.load(),p.generation.load(),p.applied.load(),p.event_count.load(),p.dropped.load(),p.coalesced.load(),p.ring_full.load(),p.stale.load(),p.late.load(),p.emergencies.load(),p.arena_full.load(),p.faults.load(),p.arena_used.load(),p.feedback_count,p.latency,p.setup_bytes};
}
const std::string&Engine::error()const{return p_->error;}
const ws2_descriptor*Engine::descriptor(uint32_t node)const {auto*n=p_->find(node);return n?n->d:nullptr;}
uint32_t Engine::output_channels(uint32_t node,uint32_t port)const{auto*n=p_->find(node);return n&&port<n->global_out.size()?n->global_out[port].channels:0;}
bool Engine::output_constant(uint32_t node,uint32_t port)const{auto*n=p_->find(node);return n&&port<n->global_out.size()&&(n->global_out[port].flags&WS2_CONSTANT);}
float Engine::output_sample(uint32_t node,uint32_t port,uint32_t channel,uint32_t frame)const{auto*n=p_->find(node);if(!n||port>=n->global_out.size())return 0;auto&b=n->global_out[port];return channel<b.channels&&frame<p_->cfg.max_block?b.data[channel][(b.flags&WS2_CONSTANT)?0:frame]:0;}
uint32_t Engine::feedback_edges()const{return p_->feedback_count;}

uint32_t Engine::latency_frames(uint32_t bus)const{return bus<p_->bus_latency.size()?p_->bus_latency[bus]:0;}
std::vector<FeedbackLoop>Engine::feedback_loops()const{
    std::vector<FeedbackLoop>result;
    for(auto&component:p_->components)if(component.loop){FeedbackLoop loop;loop.delay_frames=p_->cfg.feedback_block;for(auto*n:component.nodes)loop.nodes.push_back(n->id);std::sort(loop.nodes.begin(),loop.nodes.end());result.push_back(std::move(loop));}
    return result;
}

std::vector<uint8_t>Engine::save_state(uint32_t node){
    auto*n=p_->find(node);if(!n||n->legacy)return {};
    if(!n->preserved.empty())return n->preserved;
    std::vector<uint8_t>bytes;ws2_writer writer{sizeof(writer),&bytes,[](void*ctx,const void*data,uint32_t count){auto&out=*static_cast<std::vector<uint8_t>*>(ctx);if(count>1024*1024-out.size())return int(WS2_CAPACITY);auto*p=static_cast<const uint8_t*>(data);out.insert(out.end(),p,p+count);return int(WS2_OK);}};
    return n->d->save_state(n->instance,&writer)==WS2_OK?bytes:std::vector<uint8_t>{};
}
int Engine::load_state(uint32_t node,const std::vector<uint8_t>&bytes){
    auto*n=p_->find(node);if(!n||n->legacy||bytes.size()>1024*1024)return WS2_INVALID;
    struct Context{const std::vector<uint8_t>&bytes;size_t cursor=0;}context{bytes};
    ws2_reader reader{sizeof(reader),&context,[](void*ctx,void*data,uint32_t count){auto&c=*static_cast<Context*>(ctx);if(count>c.bytes.size()-c.cursor)return int(WS2_INVALID);std::memcpy(data,c.bytes.data()+c.cursor,count);c.cursor+=count;return int(WS2_OK);},static_cast<uint32_t>(bytes.size())};
    int result=n->d->load_state(n->instance,&reader);
    if(result==WS2_UNSUPPORTED)n->preserved=bytes;else if(result==WS2_OK)n->preserved.clear();return result;
}
const std::vector<uint8_t>&Engine::preserved_state(uint32_t node)const {auto*n=p_->find(node);if(n)return n->preserved;static const std::vector<uint8_t>empty;return empty;}
uint32_t Engine::dropped_connections()const{return p_->dropped_connections;}
bool Engine::reconfigure(uint32_t node,const std::vector<uint8_t>&config){
    auto&old=*p_;auto*changed=old.find(node);if(!old.prepared||!changed)return false;
    if(config==changed->config){old.dropped_connections=0;return true;}
    Engine next(old.cfg);auto&fresh=*next.p_;
    // Stage metadata for retained nodes; only the changed node creates/prepares DSP.
    for(auto&n:old.nodes){
        if(n->id==node){
            if(!(n->legacy?next.add_abi1(n->id,n->legacy):next.add_node(n->id,n->d,config)))return old.fail("Cannot stage reconfiguration");
            auto*target=fresh.find(node);target->config=config;
            target->forced_channels=n->forced_channels;target->state=n->state;
            for(uint32_t i=0;i<target->state.size();++i)target->state[i].info=target->parameters[i];
            if(n->legacy&&!fresh.setup(*target))return old.fail("Staged shim prepare failed");
        }else{
            auto target=std::make_unique<Node>();target->id=n->id;target->d=n->d;target->legacy=n->legacy;
            target->inputs=n->inputs;target->outputs=n->outputs;target->parameters=n->parameters;
            target->forced_channels=n->forced_channels;target->latency=n->latency;
            fresh.nodes.push_back(std::move(target));
        }
    }
    auto index_of=[](const auto&ports,const char*id){for(uint32_t i=0;i<ports.size();++i)if(!std::strcmp(ports[i].id,id))return i;return UINT32_MAX;};
    for(auto&e:old.edges)if(!e.removed){auto*a=fresh.find(e.from),*b=fresh.find(e.to);auto sp=index_of(a->outputs,e.source->outputs[e.output].id);uint32_t dp=UINT32_MAX;
        if(e.kind==2)dp=index_of(b->parameters,e.destination->parameters[e.input].id);else dp=index_of(b->inputs,e.destination->inputs[e.input].id);
        if(sp==UINT32_MAX||dp==UINT32_MAX){++fresh.dropped_connections;continue;}
        if(e.kind==2){if(!next.modulate(e.from,sp,e.to,dp,e.amount))return old.fail("Staged modulation failed");}
        else if(!next.connect(e.from,sp,e.to,dp))return old.fail("Staged connection failed");
    }
    for(auto&bus:old.buses){auto*source=old.find(bus.node),*target=fresh.find(bus.node);uint32_t port=index_of(target->outputs,source->outputs[bus.port].id);if(port==UINT32_MAX){++fresh.dropped_connections;continue;}if(!next.output(bus.node,port,bus.channel))return old.fail("Staged bus failed");}
    for(uint32_t i=0;i<old.producer_count;++i)if(next.register_producer()<0)return old.fail("Staged producer failed");
    if(!fresh.finish_prepare())return old.fail("Staged graph prepare failed");
    if(!changed->legacy){auto bytes=save_state(node);if(!bytes.empty()){int r=next.load_state(node,bytes);if(r!=WS2_OK&&r!=WS2_UNSUPPORTED)return old.fail("Staged state restore failed");}}
    std::vector<Node*>actual;actual.reserve(fresh.nodes.size());
    for(auto&n:fresh.nodes)actual.push_back(n->id==node?n.get():old.find(n->id));
    // Preserve unchanged loop delay lines as well as processor state.
    std::vector<bool>used(old.edges.size(),false);std::vector<std::pair<size_t,size_t>>delays;
    for(size_t i=0;i<fresh.edges.size();++i){auto&e=fresh.edges[i];if(!e.back||e.kind==1||e.from==node||e.to==node)continue;
        for(size_t j=0;j<old.edges.size();++j){auto&v=old.edges[j];if(!used[j]&&!v.removed&&v.back&&std::tie(e.from,e.output,e.to,e.input,e.kind,e.amount)==std::tie(v.from,v.output,v.to,v.input,v.kind,v.amount)){used[j]=true;delays.emplace_back(i,j);break;}}
    }
    // Commit: all potentially failing work/allocations precede these swaps.
    for(size_t i=0;i<fresh.nodes.size();++i)if(fresh.nodes[i]->id!=node){
        auto&retained=*old.nodes[i];auto&layout=*fresh.nodes[i];
        retained.index=layout.index;retained.component=layout.component;retained.loop=layout.loop;
        retained.incoming.swap(layout.incoming);retained.signal_edges.swap(layout.signal_edges);
        retained.modulation.swap(layout.modulation);retained.outgoing.swap(layout.outgoing);
        for(uint32_t j=0;j<retained.event_outputs.size();++j)retained.event_outputs[j]={sizeof(ws2_event_out),&fresh,j,Impl::emit};
        fresh.nodes[i].swap(old.nodes[i]);
    }
    for(auto&e:fresh.edges){e.source=actual[e.source->index];e.destination=actual[e.destination->index];}
    for(auto&component:fresh.components)for(auto*&n:component.nodes)n=actual[n->index];
    for(auto&entry:fresh.node_table)entry.second=actual[entry.second->index];
    for(auto&bus:fresh.buses)bus.source=actual[bus.source->index];
    for(auto pair:delays){auto&e=fresh.edges[pair.first];auto&v=old.edges[pair.second];std::swap(e.delay,v.delay);e.delay_channels.swap(v.delay_channels);std::swap(e.head,v.head);}
    fresh.setup_bytes=0;for(auto&n:fresh.nodes)fresh.setup_bytes+=n->requested_fast+n->requested_large;
    fresh.position.store(old.position.load());fresh.beat=old.beat;fresh.anchor_frame=old.anchor_frame;fresh.anchor_ns=old.anchor_ns;fresh.frames_per_ns=old.frames_per_ns;
    fresh.generation.store(old.generation.load()+1);fresh.applied.store(fresh.generation.load());p_.swap(next.p_);return true;
}
}
