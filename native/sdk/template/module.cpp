// SPDX-License-Identifier: MIT
#include "wirescape_sdk.h"
struct Gain {
 static constexpr ws::Info info{"example.gain2","Example Gain",1};
 static constexpr std::array inputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array params{ws::param("gain","Gain",0,2,1)};
 void prepare(const ws::Setup&) {} void reset() {}
 void process(ws::Block&b) {
  b.out(0).set_channels(b.channels());
  bool constant=b.in(0).constant()&&b.param(0).constant(); b.out(0).mark_constant(constant);
  for(uint32_t c=0;c<b.channels();++c)
   for(uint32_t i=0;i<(constant?1u:b.frames());++i) b.out(0).ch(c)[i]=b.in(0).ch(c)[i]*b.param(0).ch(c)[i];
 }
};
WS_REGISTER_MODULE(Gain)
