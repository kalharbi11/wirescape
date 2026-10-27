// SPDX-License-Identifier: MIT
#pragma once
#include "wirescape/module2.h"
#include "signals.h"
#include "simd.h"
#include "fastmath.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <type_traits>
#include <utility>

namespace ws {
struct Info {const char *id,*name; uint32_t version;};
constexpr ws2_port port(const char*id,const char*name,uint32_t hint=WS2_ANY,uint32_t poly=WS2_POLY,
                       float unpatched=0,const char*pair=nullptr,uint32_t kind=WS2_SIGNAL) {
    return {sizeof(ws2_port),id,name,kind,hint,poly,pair,unpatched};
}
constexpr ws2_param param(const char*id,const char*name,float low,float high,float initial,
                         const char*unit="",uint32_t flags=WS2_MODULATABLE|WS2_AUTOMATABLE,
                         uint32_t taper=WS2_LINEAR,uint32_t legacy=0,float curve=4,float step=0) {
    return {sizeof(ws2_param),id,name,unit,low,high,initial,legacy,taper,curve,step,flags,0,nullptr,3,nullptr};
}
inline double to_plain(const ws2_param&p,double x) noexcept {
    x=std::clamp(x,0.,1.);
    double y=x;
    if(p.taper==WS2_LOG) y=std::exp(std::log(static_cast<double>(p.minimum))+x*std::log(static_cast<double>(p.maximum)/p.minimum));
    if(p.taper==WS2_EXP&&std::abs(p.curve)>1.e-8) y=std::expm1(p.curve*x)/std::expm1(static_cast<double>(p.curve));
    double value=p.taper==WS2_LOG?y:p.minimum+(p.maximum-p.minimum)*y;
    if(p.step>0) value=p.minimum+std::round((value-p.minimum)/p.step)*p.step;
    return std::clamp(value,static_cast<double>(p.minimum),static_cast<double>(p.maximum));
}
inline double to_normal(const ws2_param&p,double value) noexcept {
    value=std::clamp(value,static_cast<double>(p.minimum),static_cast<double>(p.maximum));
    if(p.maximum==p.minimum)return 0;
    if(p.taper==WS2_LOG)return std::log(value/p.minimum)/std::log(static_cast<double>(p.maximum)/p.minimum);
    double x=(value-p.minimum)/(p.maximum-p.minimum);
    if(p.taper==WS2_EXP&&std::abs(p.curve)>1.e-8)x=std::log1p(x*std::expm1(static_cast<double>(p.curve)))/p.curve;
    return x;
}
inline bool finite_samples(const float*data,uint32_t count) noexcept {
    uint32_t exponent=0;
    for(uint32_t i=0;i<count;++i){uint32_t bits;std::memcpy(&bits,data+i,sizeof(bits));exponent=std::max(exponent,bits&UINT32_C(0x7f800000));}
    if(exponent==UINT32_C(0x7f800000))for(uint32_t i=0;i<count;++i)if(!std::isfinite(data[i]))return false;
    return true;
}
inline void silence_outputs(ws2_process*c,uint32_t max_channels=WS2_MAX_CHANNELS) noexcept {
            if(!c||c->struct_size<sizeof(ws2_process)||!c->outputs||c->output_count>WS2_MAX_PORTS)return;
            for(uint32_t j=0;j<c->output_count;++j){auto&o=c->outputs[j];
                if(o.struct_size<sizeof(ws2_signal_out)||!o.data||!o.max_channels||o.max_channels>max_channels)continue;
                bool valid=true;for(uint32_t k=0;k<o.max_channels;++k)valid&=o.data[k]!=nullptr;if(!valid)continue;
                o.flags=WS2_CONSTANT;o.channels=std::clamp(c->channels,1u,o.max_channels);for(uint32_t k=0;k<o.channels;++k)o.data[k][0]=0;
            }
}
struct Setup {
    const ws2_config& config; const ws2_host& host;
    double sample_rate()const{return config.sample_rate;}
    uint32_t max_channels()const{return config.max_channels;}
    uint32_t max_block()const{return config.max_block_frames;}
    void*alloc(size_t bytes,size_t alignment=32,uint32_t hint=WS2_FAST)const {
        return host.alloc?host.alloc(host.ctx,bytes,alignment,hint):nullptr;
    }
};
struct Channel {
    const float *p=nullptr; bool is_constant=true;
    float operator[](uint32_t i)const noexcept{return p?p[is_constant?0:i]:0;}
};
class Signal {
    const ws2_signal*s_;
public:
    explicit Signal(const ws2_signal*s):s_(s){}
    uint32_t channels()const noexcept{return s_?s_->channels:0;}
    bool constant()const noexcept{return !s_||(s_->flags&WS2_CONSTANT);}
    bool connected()const noexcept{return s_&&(s_->flags&WS2_CONNECTED);}
    Channel ch(uint32_t c=0)const noexcept {
        if(!s_||!s_->channels||!s_->data)return {};
        if(s_->channels==1)c=0;
        return {c<s_->channels?s_->data[c]:nullptr,constant()};
    }
    float value(uint32_t c=0,uint32_t i=0)const noexcept{return ch(c)[i];}
    const float*row(uint32_t c,float*scratch,uint32_t frames)const noexcept {
        auto channel=ch(c);if(channel.p&&!channel.is_constant)return channel.p;
        std::fill_n(scratch,frames,channel[0]);return scratch;
    }
};
class Output {
    ws2_signal_out*s_;
public:
    explicit Output(ws2_signal_out*s):s_(s){}
    bool set_channels(uint32_t n)noexcept{if(!s_||n>s_->max_channels)return false;s_->channels=n;return true;}
    void mark_constant(bool value=true)noexcept{if(s_)s_->flags=value?WS2_CONSTANT:0;}
    float*ch(uint32_t c=0)const noexcept{return s_&&c<s_->max_channels&&s_->data?s_->data[c]:nullptr;}
    uint32_t channels()const noexcept{return s_?s_->channels:0;}
};
class Events {
    const ws2_event_list*list_;
public:
    explicit Events(const ws2_event_list*l):list_(l){}
    const ws2_event*begin()const{return list_?list_->items:nullptr;}
    const ws2_event*end()const{return list_&&list_->count?list_->items+list_->count:begin();}
    uint32_t size()const{return list_?list_->count:0;}
};
class Block {
    ws2_process&c_;
public:
    explicit Block(ws2_process&c):c_(c){}
    uint32_t frames()const{return c_.frames;}
    uint32_t channels()const{return c_.channels;}
    double sample_rate()const{return c_.sample_rate;}
    uint64_t frame_position()const{return c_.frame_position;}
    Signal in(uint32_t i)const{return Signal(i<c_.input_count?c_.inputs+i:nullptr);}
    Signal param(uint32_t i)const{return Signal(i<c_.param_count?c_.params+i:nullptr);}
    Output out(uint32_t i)const{return Output(i<c_.output_count?c_.outputs+i:nullptr);}
    Events events()const{return Events(&c_.events);}
    Events events_in(uint32_t port)const{return Events(port<c_.input_count&&c_.event_inputs?c_.event_inputs+port:nullptr);}
    bool emit(uint32_t port,const ws2_event&e)const {
        return port<c_.output_count&&c_.event_outputs&&c_.event_outputs[port].push&&
            c_.event_outputs[port].push(c_.event_outputs[port].ctx,port,&e)==WS2_OK;
    }
    const ws2_transport*transport()const{return c_.transport;}
    template<class Range,class Event>void split(Range range,Event event)const {
        uint32_t cursor=0;
        for(const auto&e:events()) {uint32_t at=std::min(e.offset,c_.frames);if(at>cursor)range(cursor,at);event(e);cursor=at;}
        if(cursor<c_.frames)range(cursor,c_.frames);
    }
};
class Writer {
    const ws2_writer*w_;
public:
    explicit Writer(const ws2_writer*w):w_(w){}
    bool bytes(const void*p,uint32_t n)const{return w_&&w_->write&&w_->write(w_->ctx,p,n)==WS2_OK;}
    template<class T>bool value(const T&x)const{static_assert(std::is_trivially_copyable<T>::value,"state value");return bytes(&x,sizeof(x));}
    bool u32(uint32_t value)const{uint8_t data[4];for(int i=0;i<4;++i)data[i]=static_cast<uint8_t>(value>>(i*8));return bytes(data,4);}
    bool f32(float value)const{uint32_t bits;static_assert(sizeof(float)==4,"float32");std::memcpy(&bits,&value,4);return u32(bits);}
};
class Reader {
    const ws2_reader*r_;
public:
    explicit Reader(const ws2_reader*r):r_(r){}
    bool bytes(void*p,uint32_t n)const{return r_&&r_->read&&r_->read(r_->ctx,p,n)==WS2_OK;}
    template<class T>bool value(T&x)const{static_assert(std::is_trivially_copyable<T>::value,"state value");return bytes(&x,sizeof(x));}
    bool u32(uint32_t&value)const{uint8_t data[4];if(!bytes(data,4))return false;value=0;for(int i=0;i<4;++i)value|=uint32_t(data[i])<<(i*8);return true;}
    bool f32(float&value)const{uint32_t bits;if(!u32(bits))return false;std::memcpy(&value,&bits,4);return true;}
};
template<class T,class=void>struct HasQuery:std::false_type{};
template<class T>struct HasQuery<T,std::void_t<decltype(std::declval<T>().query_ports(std::declval<const ws2_config&>(),std::declval<ws2_ports&>()))>>:std::true_type{};
template<class T,class=void>struct HasState:std::false_type{};
template<class T>struct HasState<T,std::void_t<decltype(std::declval<T>().save_state(std::declval<Writer>())),decltype(std::declval<T>().load_state(std::declval<Reader>()))>>:std::true_type{};
template<class T,class=void>struct HasMemory:std::false_type{};
template<class T>struct HasMemory<T,std::void_t<decltype(T::memory_requirements(std::declval<const ws2_config&>()))>>:std::true_type{};
template<class T,class=void>struct HasLatency:std::false_type{};
template<class T>struct HasLatency<T,std::void_t<decltype(std::declval<T>().latency_frames())>>:std::true_type{};

template<class T>class Module {
    struct Instance {T dsp;ws2_config config{};uint32_t inputs=0,outputs=0;};
    template<class F>static int guarded(F f)noexcept {
#if defined(__cpp_exceptions)||defined(_CPPUNWIND)
        try{return f();}catch(...){return WS2_EXCEPTION;}
#else
        return f();
#endif
    }
    static void*WS2_CALL create(const ws2_host*h) {
        if(!h||h->struct_size<sizeof(ws2_host)||!h->alloc)return nullptr;
        void*p=h->alloc(h->ctx,sizeof(Instance),alignof(Instance),WS2_FAST);
        if(!p)return nullptr;
        Instance*instance=nullptr;int result=guarded([&](){instance=new(p)Instance{};return WS2_OK;});
        return result==WS2_OK?instance:nullptr;
    }
    static void WS2_CALL destroy(void*p){if(p)guarded([&](){static_cast<Instance*>(p)->~Instance();return WS2_OK;});}
    static int WS2_CALL query(void*p,const ws2_config*c,ws2_ports*ports) {
        if(!p||!c||c->struct_size<sizeof(ws2_config)||!ports||ports->struct_size<sizeof(ws2_ports))return WS2_INVALID;
        return guarded([&](){
            if constexpr(HasQuery<T>::value)return static_cast<Instance*>(p)->dsp.query_ports(*c,*ports);
            else {*ports={sizeof(ws2_ports),static_cast<uint32_t>(T::inputs.size()),static_cast<uint32_t>(T::outputs.size()),T::inputs.data(),T::outputs.data()};return WS2_OK;}
        });
    }
    static int WS2_CALL memory(const ws2_config*c,ws2_memory*m) {
        if(!c||!m||c->struct_size<sizeof(ws2_config)||m->struct_size<sizeof(ws2_memory))return WS2_INVALID;
        return guarded([&](){*m={sizeof(ws2_memory),sizeof(Instance),0,32};
            if constexpr(HasMemory<T>::value){auto extra=T::memory_requirements(*c);m->fast_bytes+=extra.fast_bytes;m->large_bytes+=extra.large_bytes;}
            return int(WS2_OK);});
    }
    static int WS2_CALL prepare(void*p,const ws2_config*c,const ws2_host*h) {
        if(!p||!c||!h||c->struct_size<sizeof(ws2_config)||h->struct_size<sizeof(ws2_host)||
           !std::isfinite(c->sample_rate)||c->sample_rate<1||c->sample_rate>384000||!c->max_block_frames||
           !c->max_channels||c->max_channels>WS2_MAX_CHANNELS||T::params.size()>WS2_MAX_PARAMS)return WS2_INVALID;
        for(const auto&v:T::params)if(v.struct_size<sizeof(ws2_param)||!v.id||!std::isfinite(v.minimum)||!std::isfinite(v.maximum)||
            v.maximum<v.minimum||!std::isfinite(v.initial)||v.initial<v.minimum||v.initial>v.maximum||
            !std::isfinite(v.curve)||!std::isfinite(v.step)||v.step<0||v.taper>WS2_EXP||
            (v.taper==WS2_LOG&&v.minimum<=0))return WS2_INVALID;
        auto&x=*static_cast<Instance*>(p);x.config=*c;
        return guarded([&](){ws2_ports ports{sizeof(ports),0,0,nullptr,nullptr};int r=query(p,c,&ports);if(r!=WS2_OK)return r;
            if(ports.input_count>WS2_MAX_PORTS||ports.output_count>WS2_MAX_PORTS)return int(WS2_INVALID);
            x.inputs=ports.input_count;x.outputs=ports.output_count;
            if constexpr(std::is_same_v<decltype(x.dsp.prepare(Setup{*c,*h})),bool>){if(!x.dsp.prepare(Setup{*c,*h}))return int(WS2_INVALID);}
            else x.dsp.prepare(Setup{*c,*h});
            x.dsp.reset();return int(WS2_OK);});
    }
    static void WS2_CALL reset(void*p){if(p)guarded([&](){static_cast<Instance*>(p)->dsp.reset();return WS2_OK;});}
    static int WS2_CALL process(void*p,ws2_process*c) {
        if(!p||!c||c->struct_size<sizeof(ws2_process)){silence_outputs(c);return WS2_INVALID;}
        auto&x=*static_cast<Instance*>(p);
        if(!c->frames)return WS2_OK;
        auto silence=[&](){silence_outputs(c,x.config.max_channels);};
        auto invalid=[&](){silence();return int(WS2_INVALID);};
        if(c->frames>x.config.max_block_frames||!c->channels||c->channels>x.config.max_channels||
           c->input_count!=x.inputs||c->output_count!=x.outputs||c->param_count!=T::params.size()||
           c->sample_rate!=x.config.sample_rate||
           (c->input_count&&!c->inputs)||(c->output_count&&!c->outputs)||(c->param_count&&!c->params))return invalid();
        for(uint32_t j=0;j<c->output_count;++j) {
            const auto&o=c->outputs[j];
            if(o.struct_size<sizeof(ws2_signal_out)||o.max_channels!=x.config.max_channels||!o.data)return invalid();
            for(uint32_t k=0;k<o.max_channels;++k)if(!o.data[k])return invalid();
        }
        auto valid=[&](const ws2_signal&s){
            if(s.struct_size<sizeof(ws2_signal)||s.channels>x.config.max_channels||(s.channels&&!s.data))return false;
            for(uint32_t k=0;k<s.channels;++k)if(!s.data[k])return false;
            return true;
        };
        for(uint32_t i=0;i<c->input_count;++i)if(!valid(c->inputs[i])){return invalid();}
        for(uint32_t i=0;i<c->param_count;++i)if(!valid(c->params[i])){return invalid();}
        if(c->events.count&&(!c->events.items||c->events.struct_size<sizeof(ws2_event_list)))return invalid();
        uint32_t previous=0;
        for(uint32_t i=0;i<c->events.count;++i){const auto&e=c->events.items[i];if(e.struct_size<sizeof(ws2_event)||e.offset>=c->frames||(i&&e.offset<previous)||!std::isfinite(e.value)||!std::isfinite(e.value2)||e.bytes>WS2_MAX_PAYLOAD||(e.bytes&&!e.data)||e.ump_words>4)return invalid();previous=e.offset;}
        int result=guarded([&](){Block block(*c);x.dsp.process(block);return WS2_OK;});
        if(result!=WS2_OK){silence();return result;}
        for(uint32_t j=0;j<c->output_count;++j){const auto&o=c->outputs[j];
            if(o.channels>o.max_channels){return invalid();}
            for(uint32_t k=0;k<o.channels;++k)if(!finite_samples(o.data[k],(o.flags&WS2_CONSTANT)?1u:c->frames)){return invalid();}
        }
        return WS2_OK;
    }
    static int WS2_CALL save(void*p,const ws2_writer*w) {
        if(!p||!w||w->struct_size<sizeof(ws2_writer))return WS2_INVALID;
        if constexpr(HasState<T>::value)return guarded([&](){return static_cast<Instance*>(p)->dsp.save_state(Writer(w));});
        else return WS2_UNSUPPORTED;
    }
    static int WS2_CALL load(void*p,const ws2_reader*r) {
        if(!p||!r||r->struct_size<sizeof(ws2_reader))return WS2_INVALID;
        if constexpr(HasState<T>::value)return guarded([&](){return static_cast<Instance*>(p)->dsp.load_state(Reader(r));});
        else return WS2_UNSUPPORTED;
    }
    static uint32_t WS2_CALL latency(void*p) {uint32_t frames=0;if constexpr(HasLatency<T>::value){if(p)guarded([&](){frames=static_cast<Instance*>(p)->dsp.latency_frames();return int(WS2_OK);});}return frames;}
public:
    static const ws2_descriptor*descriptor() {
        static const ws2_descriptor d={sizeof(ws2_descriptor),WS_ABI2_VERSION,T::info.id,T::info.name,T::info.version,
            static_cast<uint32_t>(T::inputs.size()),static_cast<uint32_t>(T::outputs.size()),static_cast<uint32_t>(T::params.size()),
            T::inputs.data(),T::outputs.data(),T::params.data(),create,destroy,query,memory,prepare,reset,process,save,load,latency};
        return &d;
    }
};
}
#define WS_REGISTER_MODULE(T) \
extern "C" WS2_EXPORT uint32_t WS2_CALL WS_MODULE2_VERSION_NAME(void){return WS_ABI2_VERSION;} \
extern "C" WS2_EXPORT const ws2_descriptor*WS2_CALL WS_MODULE2_ENTRY_NAME(void){return ws::Module<T>::descriptor();}
