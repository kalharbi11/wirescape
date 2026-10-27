// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "wirescape_sdk.h"
namespace ws2ports {
struct Vcf {
    static constexpr ws::Info info{"essentials.vcf","VCF",2};
    static constexpr std::array inputs{ws::port("audio","Audio",WS2_AUDIO)};
    static constexpr std::array outputs{ws::port("low12","Low12",WS2_AUDIO),ws::port("low24","Low24",WS2_AUDIO)};
    static constexpr uint32_t flags=WS2_MODULATABLE|WS2_PER_VOICE|WS2_AUDIO_RATE_OK|WS2_AUTOMATABLE;
    static constexpr std::array params{ws::param("cutoff","Cutoff",20,20000,1000,"Hz",flags,WS2_LOG,0),ws::param("resonance","Resonance",0,.95f,0,"",flags,WS2_LINEAR,1)};
    // High-Q/low-cutoff IIR state remains double for reference equivalence.
    std::array<std::array<double,4>,16>z{};std::array<float,16>previous{};std::array<double,16>coefficient{};
    uint64_t coefficient_recomputes=0;
    void prepare(const ws::Setup&){}void reset(){for(auto&v:z)v.fill(0);previous.fill(-1);coefficient.fill(0);coefficient_recomputes=0;}
    void process(ws::Block&b){
        for(uint32_t j=0;j<2;++j){b.out(j).set_channels(b.channels());b.out(j).mark_constant(false);}
        for(uint32_t group=0;group<b.channels();group+=4)for(uint32_t c=group;c<std::min(group+4,b.channels());++c){
            auto input=b.in(0).ch(c),cutoff=b.param(0).ch(c),resonance=b.param(1).ch(c);
            auto*lo=b.out(0).ch(c);auto*hi=b.out(1).ch(c);
            for(uint32_t i=0;i<b.frames();++i){
                float hz=cutoff[i];
                if(hz!=previous[c]){double g=std::tan(3.14159265358979323846*std::min<double>(hz,.45*b.sample_rate())/b.sample_rate());coefficient[c]=g/(1+g);previous[c]=hz;++coefficient_recomputes;}
                const double a=coefficient[c],x=std::clamp<double>(input[i],-1.e6,1.e6),k=(3.2/.95)*resonance[i];
                double zero=0;for(auto state:z[c])zero=a*zero+(1-a)*state;
                double a2=a*a,a4=a2*a2,feedback=(a4*x+zero)/(1+k*a4),stage=x-k*feedback;
                for(int j=0;j<4;++j){double v=(stage-z[c][j])*a;stage=z[c][j]+v;z[c][j]=stage+v;
                    // Preserve the existing tail rule; FTZ changes alone are not
                    // assumed bit-equivalent for intermediate double state.
                    if(std::abs(z[c][j])<1.e-20)z[c][j]=0;
                    if(j==1)lo[i]=static_cast<float>(stage);
                }hi[i]=static_cast<float>(stage);
            }
        }
    }
};
}
