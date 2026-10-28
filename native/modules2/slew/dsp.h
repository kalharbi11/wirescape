// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../port_state.h"
namespace ws2ports {
struct Slew {
 static constexpr ws::Info info{"essentials.slew","Slew",2};
 static constexpr std::array inputs{ws::port("control","Control",WS2_CV_BI)};static constexpr std::array outputs{ws::port("control","Control",WS2_CV_BI)};
 static constexpr std::array params{ws::param("rise","Rise",0,10,.1f,"s/unit",port_flags,WS2_LINEAR,0),ws::param("fall","Fall",0,10,.1f,"s/unit",port_flags,WS2_LINEAR,1)};
 struct Voice{double level=0,rise=.1,fall=.1;float last_rise=.1f,last_fall=.1f;};std::array<Voice,16>voices{};double rate=48000;
 void prepare(const ws::Setup&s){rate=s.sample_rate();reset();}void reset(){for(auto&v:voices)v.level=0;}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(false);for(uint32_t c=0;c<b.channels();++c){auto&v=voices[c];for(uint32_t i=0;i<b.frames();++i){float rise=b.param(0).value(c,i),fall=b.param(1).value(c,i);if(rise!=v.last_rise){v.rise=rise;v.last_rise=rise;}if(fall!=v.last_fall){v.fall=fall;v.last_fall=fall;}double x=b.in(0).value(c,i);if(std::isfinite(x)){double time=x>=v.level?v.rise:v.fall;if(time==0)v.level=x;else{double step=1./(time*rate);if(x>v.level)v.level+=std::min(step,x-v.level);else v.level-=std::min(step,v.level-x);}}b.out(0).ch(c)[i]=static_cast<float>(v.level);}}}
 int save_state(ws::Writer w){if(!w.u32(1))return WS2_CAPACITY;for(auto&v:voices)if(!write_double(w,v.level)||!write_double(w,v.rise)||!write_double(w,v.fall)||!w.f32(v.last_rise)||!w.f32(v.last_fall))return WS2_CAPACITY;return WS2_OK;}
 int load_state(ws::Reader r){uint32_t version;if(!r.u32(version))return WS2_INVALID;if(version!=1)return WS2_UNSUPPORTED;auto staged=voices;for(auto&v:staged)if(!read_double(r,v.level)||!read_double(r,v.rise)||!read_double(r,v.fall)||!r.f32(v.last_rise)||!r.f32(v.last_fall)||!std::isfinite(v.last_rise)||!std::isfinite(v.last_fall)||v.rise<0||v.rise>10||v.fall<0||v.fall>10||v.last_rise<0||v.last_rise>10||v.last_fall<0||v.last_fall>10)return WS2_INVALID;voices=staged;return WS2_OK;}
};
}
