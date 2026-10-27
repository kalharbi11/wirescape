// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
namespace ws2ports {
struct Lfo {
    static constexpr ws::Info info{"essentials.lfo","LFO",2};
    static constexpr std::array<ws2_port,0>inputs{};
    static constexpr std::array outputs{ws::port("control","Control",WS2_CV_BI)};
    static constexpr uint32_t flags=WS2_MODULATABLE|WS2_PER_VOICE|WS2_AUDIO_RATE_OK|WS2_AUTOMATABLE;
    static constexpr std::array params{
        ws::param("rate","Rate",.01f,20,1,"Hz",flags,WS2_LINEAR,0),
        ws::param("depth","Depth",0,1,1,"",flags,WS2_LINEAR,1),
        ws::param("offset","Offset",-1,1,0,"",flags,WS2_LINEAR,2),
        ws::param("sync","Tempo sync",0,1,0,"",WS2_STEPPED,WS2_LINEAR,3,4,1),
        ws::param("beats","Beats/cycle",.125f,16,1,"beats",WS2_AUTOMATABLE,WS2_LOG,4)};
    std::array<double,16>phase{}; // Double phase avoids long-term low-rate drift.
    void prepare(const ws::Setup&){}void reset(){phase.fill(0);}
    void process(ws::Block&b){
        constexpr double tau=6.283185307179586476925286766559;
        b.out(0).set_channels(b.channels());b.out(0).mark_constant(false);
        for(uint32_t group=0;group<b.channels();group+=4)for(uint32_t c=group;c<std::min(group+4,b.channels());++c){
            auto*out=b.out(0).ch(c);
            for(uint32_t i=0;i<b.frames();++i){
                if(b.param(3).value(c,i)>=.5f&&b.transport()){
                    double position=b.transport()->beat+((b.transport()->flags&WS2_PLAYING)?i*b.transport()->bpm/(60.*b.sample_rate()):0);
                    double cycles=position/b.param(4).value(c,i);phase[c]=tau*(cycles-std::floor(cycles));
                }
                out[i]=static_cast<float>(std::sin(phase[c])*b.param(1).value(c,i)+b.param(2).value(c,i));
                if(!(b.param(3).value(c,i)>=.5f&&b.transport())){phase[c]+=tau*b.param(0).value(c,i)/b.sample_rate();if(phase[c]>=tau)phase[c]=std::fmod(phase[c],tau);}
            }
        }
    }
};
}
