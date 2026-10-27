// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace ws2ports {
inline bool write_double(ws::Writer w,double x){uint64_t bits;static_assert(sizeof(bits)==sizeof(x),"binary64");std::memcpy(&bits,&x,8);return w.u32(uint32_t(bits))&&w.u32(uint32_t(bits>>32));}
inline bool read_double(ws::Reader r,double&x){uint32_t lo,hi;if(!r.u32(lo)||!r.u32(hi))return false;uint64_t bits=uint64_t(lo)|(uint64_t(hi)<<32);std::memcpy(&x,&bits,8);return std::isfinite(x);}
// Preserve ABI1's double defaults, accumulator and sample-count ramp exactly.
struct PortRamp {
 double current,target,step=0;uint32_t duration=240,remaining=0;bool rendered=false;float last;
 explicit PortRamp(double initial=0):current(initial),target(initial),last(static_cast<float>(initial)){}
 void reset(){current=target;remaining=0;rendered=false;}
 void prepare(double rate){duration=std::max(1u,static_cast<uint32_t>(rate*.005));reset();}
 void update(float value){if(value==last)return;last=value;if(double(value)==target)return;target=value;if(!rendered){current=value;remaining=0;return;}remaining=duration;step=(target-current)/remaining;}
 double next(){rendered=true;if(remaining&&! --remaining)current=target;else if(remaining)current+=step;return current;}
 bool write(ws::Writer w)const{return write_double(w,current)&&write_double(w,target)&&write_double(w,step)&&w.u32(duration)&&w.u32(remaining)&&w.u32(rendered?1:0)&&w.f32(last);}
 bool read(ws::Reader r,double low,double high){uint32_t flag;auto v=*this;if(!read_double(r,v.current)||!read_double(r,v.target)||!read_double(r,v.step)||!r.u32(v.duration)||!r.u32(v.remaining)||!r.u32(flag)||!r.f32(v.last)||!v.duration||v.duration>1920||v.remaining>v.duration||flag>1||!std::isfinite(v.last)||v.target<low||v.target>high||v.last<low||v.last>high)return false;v.rendered=flag!=0;*this=v;return true;}
};
constexpr uint32_t port_flags=WS2_MODULATABLE|WS2_AUTOMATABLE|WS2_PER_VOICE|WS2_AUDIO_RATE_OK;
}
