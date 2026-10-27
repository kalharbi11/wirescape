// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../port_state.h"
namespace ws2ports {
struct ScaleOffset {
 static constexpr ws::Info info{"essentials.scale_offset","Scale/Offset",2};
 static constexpr std::array inputs{ws::port("control","Control",WS2_CV_BI)};static constexpr std::array outputs{ws::port("control","Control",WS2_CV_BI)};
 static constexpr std::array params{ws::param("scale","Scale",-2,2,1,"",port_flags,WS2_LINEAR,0),ws::param("offset","Offset",-1,1,0,"",port_flags,WS2_LINEAR,1)};
 struct Voice{PortRamp scale{1},offset{0};};std::array<Voice,16>voices{};
 void prepare(const ws::Setup&s){for(auto&v:voices){v.scale.prepare(s.sample_rate());v.offset.prepare(s.sample_rate());}}
 void reset(){for(auto&v:voices){v.scale.reset();v.offset.reset();}}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(false);for(uint32_t c=0;c<b.channels();++c){auto&v=voices[c];for(uint32_t i=0;i<b.frames();++i){v.scale.update(b.param(0).value(c,i));v.offset.update(b.param(1).value(c,i));b.out(0).ch(c)[i]=static_cast<float>(b.in(0).value(c,i)*v.scale.next()+v.offset.next());}}}
 int save_state(ws::Writer w){if(!w.u32(1))return WS2_CAPACITY;for(auto&v:voices)if(!v.scale.write(w)||!v.offset.write(w))return WS2_CAPACITY;return WS2_OK;}
 int load_state(ws::Reader r){uint32_t version;if(!r.u32(version))return WS2_INVALID;if(version!=1)return WS2_UNSUPPORTED;auto staged=voices;for(auto&v:staged)if(!v.scale.read(r,-2,2)||!v.offset.read(r,-1,1))return WS2_INVALID;voices=staged;return WS2_OK;}
};
}
