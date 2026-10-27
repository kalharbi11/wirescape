// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
namespace ws2ports {
struct Vca {
    static constexpr ws::Info info{"essentials.vca","VCA",2};
    static constexpr std::array inputs{ws::port("audio","Audio",WS2_AUDIO),ws::port("gain_cv","Gain CV",WS2_CV_UNI,WS2_POLY,1)};
    static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
    static constexpr std::array params{ws::param("gain","Gain",0,2,1,"",WS2_MODULATABLE|WS2_PER_VOICE|WS2_AUDIO_RATE_OK|WS2_AUTOMATABLE)};
    float*scratch=nullptr;uint32_t stride=0;
    static ws2_memory memory_requirements(const ws2_config&c){return {sizeof(ws2_memory),3*sizeof(float)*c.max_block_frames,0,32};}
    bool prepare(const ws::Setup&s){stride=s.max_block();scratch=static_cast<float*>(s.alloc(3*sizeof(float)*stride));return scratch!=nullptr;}
    void reset(){}
    void process(ws::Block&b){
        bool constant=b.in(0).constant()&&b.in(1).constant()&&b.param(0).constant();
        b.out(0).set_channels(b.channels());b.out(0).mark_constant(constant);
        const uint32_t count=constant?1:b.frames();
        for(uint32_t c=0;c<b.channels();++c){
            const float*audio=b.in(0).row(c,scratch,count);
            float*out=b.out(0).ch(c);uint32_t i=0;
            if(b.in(1).constant()&&b.param(0).constant()){
                const float cv=b.in(1).value(c),gain=b.param(0).value(c);
                const ws::simd::f4 cv4(cv),gain4(gain);
                for(;i+4<=count;i+=4)(ws::simd::f4::load(audio+i)*cv4*gain4).store(out+i);
                for(;i<count;++i)out[i]=audio[i]*cv*gain;
                continue;
            }
            const float*cv=b.in(1).row(c,scratch+stride,count),*gain=b.param(0).row(c,scratch+2*stride,count);
            for(;i+4<=count;i+=4)(ws::simd::f4::load(audio+i)*ws::simd::f4::load(cv+i)*ws::simd::f4::load(gain+i)).store(out+i);
            for(;i<count;++i)out[i]=audio[i]*cv[i]*gain[i];
        }
    }
};
}
