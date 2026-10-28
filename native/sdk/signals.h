// SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace ws {
// Independently written sample-counted ramp; doubles retain the ABI1
// accumulator precision. Its public samples are still float32 signals.
class LinearRamp {
    double present_, destination_, increment_=0;
    uint32_t length_=240, left_=0;
    bool started_=false;
public:
    explicit LinearRamp(double value=0):present_(value),destination_(value){}
    void prepare(double rate) noexcept { length_=std::max(1u,static_cast<uint32_t>(rate*.005)); reset(); }
    void reset() noexcept { present_=destination_; left_=0; started_=false; }
    void jump(double value) noexcept { present_=destination_=value; left_=0; }
    void set(double value) noexcept {
        if(value==destination_) return;
        destination_=value;
        if(!started_) {present_=value;left_=0;return;}
        left_=length_; increment_=(destination_-present_)/length_;
    }
    double next() noexcept {
        started_=true;
        if(left_) { --left_; if(left_) present_+=increment_; else present_=destination_; }
        return present_;
    }
    bool constant() const noexcept {return left_==0;}
    double value() const noexcept {return present_;}
};
class Schmitt {
    bool high_=false;
public:
    bool process(float value) noexcept { if(value>.5f)high_=true; else if(value<.25f)high_=false; return high_; }
    void reset() noexcept {high_=false;}
};
class Trigger {
    uint32_t length_=48,left_=0;
public:
    void prepare(double rate) noexcept {length_=std::max(1u,static_cast<uint32_t>(std::ceil(rate*.001)));left_=0;}
    void fire() noexcept {left_=length_;}
    float next() noexcept {if(!left_)return 0;--left_;return 1;}
    void reset() noexcept {left_=0;}
};
class Random {
    uint64_t state_=0,sequence_=1;
public:
    void seed(uint64_t instance,uint32_t channel=0) noexcept {
        state_=0;sequence_=(static_cast<uint64_t>(channel)<<1)|1;next();state_+=instance;next();
    }
    uint32_t next() noexcept {
        uint64_t old=state_;state_=old*6364136223846793005ULL+sequence_;
        uint32_t x=static_cast<uint32_t>(((old>>18)^old)>>27),r=static_cast<uint32_t>(old>>59);
        return (x>>r)|(x<<((-r)&31));
    }
    float bipolar() noexcept {return static_cast<float>(next()>>8)*(1.f/8388608.f)-1.f;}
    // Additive state accessor so modules can persist the generator. `sequence` is always odd
    // for a seeded generator; restore() trusts its input, so validate before calling it.
    struct State {uint64_t state, sequence;};
    State save() const noexcept {return {state_,sequence_};}
    void restore(const State& s) noexcept {state_=s.state;sequence_=s.sequence;}
};
}
