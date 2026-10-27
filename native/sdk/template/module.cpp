// SPDX-License-Identifier: MIT
#include "wirescape_sdk.h"
struct Gain {
 static constexpr ws::Info info{"example.gain2","Example Gain",1};
 static constexpr std::array inputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
 static constexpr std::array params{ws::param("gain","Gain",0,2,1)};
 float*scratch=nullptr;uint32_t stride=0;
 static ws2_memory memory_requirements(const ws2_config&c){return {sizeof(ws2_memory),2*sizeof(float)*c.max_block_frames,0,32};}
 bool prepare(const ws::Setup&s){stride=s.max_block();scratch=static_cast<float*>(s.alloc(2*sizeof(float)*stride));return scratch!=nullptr;}
 void reset() {}
 void process(ws::Block&b) {
  b.out(0).set_channels(b.channels());
  bool constant=b.in(0).constant()&&b.param(0).constant(); b.out(0).mark_constant(constant);
  const uint32_t count=constant?1:b.frames();
  for(uint32_t c=0;c<b.channels();++c){
   const float*audio=b.in(0).row(c,scratch,count),*gain=b.param(0).row(c,scratch+stride,count);
   for(uint32_t i=0;i<count;++i)b.out(0).ch(c)[i]=audio[i]*gain[i];
  }
 }
};
WS_REGISTER_MODULE(Gain)
