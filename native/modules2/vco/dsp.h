// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
namespace ws2ports {
struct Vco {
    static constexpr ws::Info info{"essentials.vco","VCO",2};
    static constexpr std::array inputs{
        ws::port("pitch","Pitch",WS2_PITCH),ws::port("fm","FM Hz",WS2_AUDIO),
        ws::port("notes","Notes",WS2_EV_NOTES,WS2_POLY,0,nullptr,WS2_EVENT)};
    static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};
    static constexpr std::array params{ws::param("frequency","Frequency",.01f,20000,220,"Hz",WS2_MODULATABLE|WS2_PER_VOICE|WS2_AUDIO_RATE_OK|WS2_AUTOMATABLE)};
    // Double phase preserves accumulated precision and ABI1 reference math.
    std::array<double,16>phase{};std::array<float,16>note_frequency{};std::array<bool,16>note_active{};
    void prepare(const ws::Setup&){}void reset(){phase.fill(0);note_frequency.fill(0);note_active.fill(false);}
    void process(ws::Block&b){
        constexpr double tau=6.283185307179586476925286766559;
        b.out(0).set_channels(b.channels());b.out(0).mark_constant(false);
        b.split([&](uint32_t first,uint32_t end){
            for(uint32_t group=0;group<b.channels();group+=4)for(uint32_t c=group;c<std::min(group+4,b.channels());++c){
                auto pitch=b.in(0).ch(c),fm=b.in(1).ch(c),frequency=b.param(0).ch(c);auto*out=b.out(0).ch(c);
                for(uint32_t i=first;i<end;++i){
                    out[i]=static_cast<float>(std::sin(phase[c]));
                    double hz=b.in(0).connected()?261.6255653005986*std::pow(2.,pitch[i])*(frequency[i]/220.):note_active[c]?note_frequency[c]:frequency[i];
                    hz=std::clamp(hz+fm[i],0.,b.sample_rate()*.49);
                    phase[c]+=tau*hz/b.sample_rate();if(phase[c]>=tau)phase[c]-=tau;
                }
            }
        },[&](const ws2_event&e){uint32_t c=e.channel<0?0:static_cast<uint32_t>(e.channel);if(c>=b.channels())return;
            if(e.type==WS2_NOTE_ON){note_frequency[c]=static_cast<float>(440*std::pow(2.,(e.key-69.)/12.));note_active[c]=true;}
            else if(e.type==WS2_NOTE_CHOKE){phase[c]=0;note_active[c]=false;}
        });
    }
};
}
