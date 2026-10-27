// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../port_state.h"
namespace ws2ports {
struct Mixer {
 static constexpr ws::Info info{"essentials.mixer","Mixer",2};
 static constexpr std::array inputs{ws::port("a","A",WS2_AUDIO),ws::port("b","B",WS2_AUDIO)};
 static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array params{ws::param("level_a","Level A",0,2,.5f,"",port_flags,WS2_LINEAR,0),ws::param("level_b","Level B",0,2,.5f,"",port_flags,WS2_LINEAR,1)};
 struct Voice{PortRamp a{.5},b{.5};};std::array<Voice,16>voices{};
 void prepare(const ws::Setup&s){for(auto&v:voices){v.a.prepare(s.sample_rate());v.b.prepare(s.sample_rate());}}
 void reset(){for(auto&v:voices){v.a.reset();v.b.reset();}}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(false);for(uint32_t c=0;c<b.channels();++c){auto&v=voices[c];for(uint32_t i=0;i<b.frames();++i){v.a.update(b.param(0).value(c,i));v.b.update(b.param(1).value(c,i));b.out(0).ch(c)[i]=static_cast<float>(b.in(0).value(c,i)*v.a.next()+b.in(1).value(c,i)*v.b.next());}}}
 int save_state(ws::Writer w){if(!w.u32(1))return WS2_CAPACITY;for(auto&v:voices)if(!v.a.write(w)||!v.b.write(w))return WS2_CAPACITY;return WS2_OK;}
 int load_state(ws::Reader r){uint32_t version;if(!r.u32(version))return WS2_INVALID;if(version!=1)return WS2_UNSUPPORTED;auto staged=voices;for(auto&v:staged)if(!v.a.read(r,0,2)||!v.b.read(r,0,2))return WS2_INVALID;voices=staged;return WS2_OK;}
};
}
