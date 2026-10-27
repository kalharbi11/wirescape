// SPDX-License-Identifier: MIT
// Standalone callback test; compile this file alone (it includes the module).
#include "module.cpp"
#include <cstdio>
#include <cstdlib>
#define REQUIRE(x) do{if(!(x)){std::fprintf(stderr,"template check line %d: %s\n",__LINE__,#x);std::abort();}}while(0)
struct Pool {alignas(64) unsigned char bytes[8192];size_t used=0;};
void* WS2_CALL allocate(void*ctx,size_t bytes,size_t alignment,uint32_t){auto&p=*static_cast<Pool*>(ctx);p.used=(p.used+alignment-1)&~(alignment-1);if(p.used+bytes>sizeof(p.bytes))return nullptr;void*r=p.bytes+p.used;p.used+=bytes;return r;}
int main(){
 Pool pool;ws2_host host{sizeof(host),&pool,allocate};auto*d=ws::Module<Gain>::descriptor();void*state=d->create(&host);REQUIRE(state);
 ws2_config cfg{sizeof(cfg),48000,256,16,42,nullptr,0};REQUIRE(d->prepare(state,&cfg,&host)==WS2_OK);
 alignas(32) float data[16][256]{};float*dst[16];for(int c=0;c<16;++c)dst[c]=data[c];
 alignas(32) float input[256]={.25f},gain[256]={1};const float*ip[]={input};const float*pp[]={gain};
 ws2_signal in{sizeof(in),ip,1,WS2_CONSTANT|WS2_CONNECTED},param{sizeof(param),pp,1,WS2_CONSTANT};
 ws2_signal_out out{sizeof(out),dst,1,0,16};
 ws2_process ctx{};ctx.struct_size=sizeof(ctx);ctx.frames=17;ctx.channels=4;ctx.sample_rate=48000;ctx.input_count=1;ctx.output_count=1;ctx.param_count=1;ctx.inputs=&in;ctx.params=&param;ctx.outputs=&out;
 REQUIRE(d->process(state,&ctx)==WS2_OK);REQUIRE(out.channels==4);REQUIRE(out.flags&WS2_CONSTANT);for(int c=0;c<4;++c)REQUIRE(data[c][0]==.25f);
 ws2_memory memory{sizeof(memory),0,0,0};REQUIRE(d->memory_requirements(&cfg,&memory)==WS2_OK);REQUIRE(memory.fast_bytes>=2*sizeof(float)*cfg.max_block_frames);
 host.alloc=[](void*,size_t,size_t,uint32_t)->void*{return nullptr;};REQUIRE(d->prepare(state,&cfg,&host)==WS2_INVALID);
 d->reset(state);d->destroy(state);std::puts("template callbacks passed");
}
