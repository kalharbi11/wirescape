// SPDX-License-Identifier: MIT
#include "wirescape_sdk.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"SDK check line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
struct Mini {
 static constexpr ws::Info info{"test.sdk","SDK test",1};
 static constexpr std::array inputs{ws::port("in","In")};
 static constexpr std::array outputs{ws::port("out","Out")};
 static constexpr std::array params{ws::param("gain","Gain",0,2,1)};
 void prepare(const ws::Setup&){} void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant(b.in(0).constant()&&b.param(0).constant());for(uint32_t c=0;c<b.channels();++c)for(uint32_t i=0;i<(b.in(0).constant()&&b.param(0).constant()?1u:b.frames());++i)b.out(0).ch(c)[i]=b.in(0).ch(c)[i]*b.param(0).ch(c)[i];}
};
int main(){
 return 1; // Temporary CI negative control; revert after evidence.
 for(double sr:{44100.,48000.,96000.}){ws::LinearRamp r(0);r.prepare(sr);CHECK(r.next()==0);r.set(1);uint32_t n=static_cast<uint32_t>(sr*.005);for(uint32_t i=1;i<=n;++i)CHECK(std::abs(r.next()-double(i)/n)<1e-12);CHECK(r.constant());r.set(.2);r.reset();CHECK(r.next()==.2);}
 ws::Schmitt gate;CHECK(!gate.process(.5f));CHECK(gate.process(.51f));CHECK(gate.process(.25f));CHECK(!gate.process(.24f));
 ws::Trigger trig;trig.prepare(44100);trig.fire();int high=0;for(int i=0;i<100;++i)high+=trig.next()>0;CHECK(high==45);
 ws::Random a,b,c;a.seed(123,0);b.seed(123,0);c.seed(123,1);bool different=false;for(int i=0;i<500;++i){auto x=a.next();CHECK(x==b.next());different|=x!=c.next();}CHECK(different);
 alignas(16) float x[]={1,2,3,4},y[]={4,3,2,1},z[4];(ws::simd::f4::load(x)*ws::simd::f4::load(y)+ws::simd::f4(2)).store(z);for(int i=0;i<4;++i)CHECK(z[i]==x[i]*y[i]+2);
 double maximum=0;for(int i=-10000;i<=10000;++i){float t=float(i)/40000;maximum=std::max(maximum,std::abs(double(ws::fastmath::tan_small(t))-std::tan(double(t))));}CHECK(maximum<6e-8);
 auto log=ws::param("f","F",20,20000,1000,"Hz",0,WS2_LOG);CHECK(std::abs(ws::to_plain(log,.5)-std::sqrt(20.*20000))<1e-8);CHECK(std::abs(ws::to_normal(log,1000)-ws::to_normal(log,ws::to_plain(log,ws::to_normal(log,1000))))<1e-12);
 struct Pool {alignas(64) unsigned char arena[4096];size_t used=0;} pool;
 ws2_host host{sizeof(host),&pool,[](void*p,size_t n,size_t align,uint32_t)->void*{auto&v=*static_cast<Pool*>(p);v.used=(v.used+align-1)&~(align-1);if(v.used+n>4096)return nullptr;void*r=v.arena+v.used;v.used+=n;return r;}};
 auto*d=ws::Module<Mini>::descriptor();void*instance=d->create(&host);CHECK(instance);
 ws2_config cfg{sizeof(cfg),48000,32,16,123,nullptr,0};CHECK(d->prepare(instance,&cfg,&host)==WS2_OK);
 alignas(32) float storage[16][32]{};float*outputs[16];for(int j=0;j<16;++j)outputs[j]=storage[j];
 alignas(32) float input[32]={.25f},gain[32]={2};const float*ip[]={input};const float*gp[]={gain};
 ws2_signal in{sizeof(in),ip,1,WS2_CONSTANT|WS2_CONNECTED},parameter{sizeof(parameter),gp,1,WS2_CONSTANT};
 ws2_signal_out out{sizeof(out),outputs,1,0,16};ws2_process ctx{};ctx.struct_size=sizeof(ctx);ctx.frames=17;ctx.sample_rate=48000;ctx.channels=4;ctx.input_count=1;ctx.output_count=1;ctx.param_count=1;ctx.inputs=&in;ctx.params=&parameter;ctx.outputs=&out;
 auto allocated=pool.used;CHECK(d->process(instance,&ctx)==WS2_OK);CHECK(pool.used==allocated);CHECK(out.channels==4&&(out.flags&WS2_CONSTANT));for(int j=0;j<4;++j)CHECK(storage[j][0]==.5f);
 input[0]=std::nanf("");CHECK(d->process(instance,&ctx)==WS2_INVALID);CHECK(storage[0][0]==0);input[0]=.25f;
 ctx.frames=33;CHECK(d->process(instance,&ctx)==WS2_INVALID);ctx.frames=17;ctx.struct_size=0;CHECK(d->process(instance,&ctx)==WS2_INVALID);ctx.struct_size=sizeof(ctx);
 d->reset(instance);d->destroy(instance);CHECK(pool.used==allocated);
 std::puts("PASS SDK ramp/gate/trigger/PRNG/SIMD/taper/fastmath");
}
