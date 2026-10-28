// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../port_state.h"
namespace ws2ports {
struct Clock {
 static constexpr ws::Info info{"essentials.clock","Clock",2};
 static constexpr std::array inputs{ws::port("reset","Reset",WS2_GATE)};static constexpr std::array outputs{ws::port("clock","Clock",WS2_GATE)};
 static constexpr std::array params{ws::param("bpm","BPM",20,300,120,"BPM",port_flags,WS2_LINEAR,0),ws::param("ticks_per_beat","Ticks per beat",1,24,1,"",port_flags,WS2_LINEAR,1),ws::param("duty","Duty",.01f,.99f,.5f,"",port_flags,WS2_LINEAR,2),ws::param("run","Run",0,1,1,"",port_flags,WS2_LINEAR,3)};
 struct Voice{double phase=0,ticks=1,duty=.5,run=1;PortRamp bpm{120};float last_ticks=1,last_duty=.5f,last_run=1;bool reset_high=false;};std::array<Voice,16>voices{};double rate=48000;
 void prepare(const ws::Setup&s){rate=s.sample_rate();for(auto&v:voices)v.bpm.prepare(rate);reset();}
 void reset(){for(auto&v:voices){v.phase=0;v.reset_high=false;v.bpm.reset();}}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(false);for(uint32_t c=0;c<b.channels();++c){auto&v=voices[c];for(uint32_t i=0;i<b.frames();++i){v.bpm.update(b.param(0).value(c,i));float ticks=b.param(1).value(c,i),duty=b.param(2).value(c,i),run=b.param(3).value(c,i);if(ticks!=v.last_ticks){v.ticks=std::floor(double(ticks)+.5);v.last_ticks=ticks;}if(duty!=v.last_duty){v.duty=duty;v.last_duty=duty;}if(run!=v.last_run){v.run=run;v.last_run=run;}float input=b.in(0).value(c,i);bool high=std::isfinite(input)&&input>=.5f;if(high&&!v.reset_high)v.phase=0;v.reset_high=high;double bpm=v.bpm.next();if(v.run>=.5){b.out(0).ch(c)[i]=v.phase+1.e-12<v.duty?1.f:0.f;v.phase+=bpm*v.ticks/(60.*rate);if(v.phase>=1)v.phase-=std::floor(v.phase);}else b.out(0).ch(c)[i]=0;}}}
 int save_state(ws::Writer w){if(!w.u32(1))return WS2_CAPACITY;for(auto&v:voices){for(double x:{v.phase,v.ticks,v.duty,v.run})if(!write_double(w,x))return WS2_CAPACITY;if(!v.bpm.write(w)||!w.f32(v.last_ticks)||!w.f32(v.last_duty)||!w.f32(v.last_run)||!w.u32(v.reset_high?1:0))return WS2_CAPACITY;}return WS2_OK;}
 int load_state(ws::Reader r){uint32_t version;if(!r.u32(version))return WS2_INVALID;if(version!=1)return WS2_UNSUPPORTED;auto staged=voices;for(auto&v:staged){for(double*x:{&v.phase,&v.ticks,&v.duty,&v.run})if(!read_double(r,*x))return WS2_INVALID;uint32_t high;if(!v.bpm.read(r,20,300)||!r.f32(v.last_ticks)||!r.f32(v.last_duty)||!r.f32(v.last_run)||!r.u32(high)||high>1||v.phase<0||v.phase>=1||v.ticks<1||v.ticks>24||std::floor(v.ticks)!=v.ticks||v.duty<double(.01f)||v.duty>double(.99f)||v.run<0||v.run>1||!std::isfinite(v.last_ticks)||!std::isfinite(v.last_duty)||!std::isfinite(v.last_run)||v.last_ticks<1||v.last_ticks>24||v.last_duty<.01f||v.last_duty>.99f||v.last_run<0||v.last_run>1)return WS2_INVALID;v.reset_high=high!=0;}voices=staged;return WS2_OK;}
};
}
