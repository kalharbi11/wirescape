// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../port_state.h"
namespace ws2ports {
struct Output {
 static constexpr ws::Info info{"essentials.output","Output",2};
 static constexpr std::array inputs{ws::port("audio", "Audio", WS2_AUDIO, WS2_SUM)};
 static constexpr std::array outputs{ws::port("audio", "Audio", WS2_AUDIO)};
 static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(b.in(0).constant());for(uint32_t c=0;c<b.channels();++c)for(uint32_t i=0;i<(b.in(0).constant()?1u:b.frames());++i)b.out(0).ch(c)[i]=b.in(0).value(c,i);}
 int save_state(ws::Writer w){return w.u32(1)?WS2_OK:WS2_CAPACITY;}
 int load_state(ws::Reader r){uint32_t version;if(!r.u32(version))return WS2_INVALID;return version==1?WS2_OK:WS2_UNSUPPORTED;}
};
}
