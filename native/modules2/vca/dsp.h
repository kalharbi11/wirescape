// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
namespace ws2ports {
struct Vca {
    static constexpr ws::Info info{"essentials.vca","VCA",2};
    static constexpr std::array inputs{ws::port("audio","Audio",WS2_AUDIO),ws::port("gain_cv","Gain CV",WS2_CV_UNI,WS2_POLY,1)};
    static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
    static constexpr std::array params{ws::param("gain","Gain",0,2,1,"",WS2_MODULATABLE|WS2_PER_VOICE|WS2_AUDIO_RATE_OK|WS2_AUTOMATABLE)};
    void prepare(const ws::Setup&){}void reset(){}
    void process(ws::Block&b){
        bool constant=b.in(0).constant()&&b.in(1).constant()&&b.param(0).constant();
        b.out(0).set_channels(b.channels());b.out(0).mark_constant(constant);
        for(uint32_t i=0;i<(constant?1u:b.frames());++i)for(uint32_t first=0;first<b.channels();first+=4){
            alignas(16)float audio[4]{},cv[4]{},gain[4]{},result[4]{};
            for(uint32_t lane=0;lane<4&&first+lane<b.channels();++lane){auto c=first+lane;audio[lane]=b.in(0).value(c,i);cv[lane]=b.in(1).value(c,i);gain[lane]=b.param(0).value(c,i);}
            (ws::simd::f4::load(audio)*ws::simd::f4::load(cv)*ws::simd::f4::load(gain)).store(result);
            for(uint32_t lane=0;lane<4&&first+lane<b.channels();++lane)b.out(0).ch(first+lane)[i]=result[lane];
        }
    }
};
}
