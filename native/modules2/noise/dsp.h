// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../port_state.h"
namespace ws2ports {
struct Noise {
 static constexpr ws::Info info{"essentials.noise","Noise",2};
 static constexpr std::array<ws2_port,0>inputs{};static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array params{ws::param("level","Level",0,1,.2f,"",port_flags,WS2_LINEAR,0)};
 struct Voice{uint32_t rng=0x12345678u;PortRamp level{.2};};std::array<Voice,16>voices{};
 static uint32_t seed(uint32_t c){uint32_t x=0x12345678u^(c*0x9e3779b9u);return x?x:1u;}
 void prepare(const ws::Setup&s){for(auto&v:voices)v.level.prepare(s.sample_rate());reset();}
 void reset(){for(uint32_t c=0;c<16;++c){voices[c].rng=seed(c);voices[c].level.reset();}}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(false);for(uint32_t c=0;c<b.channels();++c){auto&v=voices[c];for(uint32_t i=0;i<b.frames();++i){uint32_t x=v.rng;x^=x<<13;x^=x>>17;x^=x<<5;v.rng=x;double bipolar=static_cast<double>(x>>8)*(1./8388608.)-1.;v.level.update(b.param(0).value(c,i));b.out(0).ch(c)[i]=static_cast<float>(bipolar*v.level.next());}}}
 int save_state(ws::Writer w){if(!w.u32(1))return WS2_CAPACITY;for(auto&v:voices)if(!w.u32(v.rng)||!v.level.write(w))return WS2_CAPACITY;return WS2_OK;}
 int load_state(ws::Reader r){uint32_t version;if(!r.u32(version))return WS2_INVALID;if(version!=1)return WS2_UNSUPPORTED;auto staged=voices;for(auto&v:staged)if(!r.u32(v.rng)||!v.rng||!v.level.read(r,0,1))return WS2_INVALID;voices=staged;return WS2_OK;}
};
}
