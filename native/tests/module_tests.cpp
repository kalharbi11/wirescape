#include "wirescape/engine.h"
#include "wirescape/linear_ramp.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
extern "C" const ws_module_descriptor *WS_CALL ws_test_mixer_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_vco_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_vca_entry();
extern "C" const ws_module_descriptor *WS_CALL ws_test_adsr_entry();
static void require(bool v, const char *m) { if (!v) throw std::runtime_error(m); }
using Engine = std::unique_ptr<ws_engine, decltype(&ws_engine_destroy)>;
static void mixer() {
    auto d = ws_test_mixer_entry();
    require(!std::strcmp(d->id, "essentials.mixer") && d->abi_version == WS_ABI_VERSION &&
            d->input_count == 2 && d->output_count == 1 && d->parameter_count == 2, "mixer descriptor");
    void *s = d->create(48000); require(s != nullptr, "mixer create");
    ws_module_config config{sizeof(config), 16, 48000, 1};
    require(d->prepare(s, &config) == 0, "mixer prepare");
    float a[] = {-3, -1, 0, 1, 3}, b[] = {2, -2, 1, -1, 4};
    float guarded[] = {123, 0, 0, 0, 0, 0, 456}; float *out[] = {guarded + 1};
    const float *in[] = {nullptr, nullptr};
    ws_process_context c{sizeof(c), 5, 0, 1};
    auto process = [&] { d->process(s, in, out, &c); require(guarded[0] == 123 && guarded[6] == 456, "output guards"); };
    process(); require(std::all_of(guarded+1, guarded+6, [](float v){return v == 0;}), "missing silence");
    in[0] = a; process(); require(guarded[1] == -1.5f && guarded[5] == 1.5f, "A only negative unclipped");
    in[0] = nullptr; in[1] = b; process(); require(guarded[1] == 1 && guarded[5] == 2, "B only");
    in[0] = a;
    ws_module_event e{WS_PARAMETER, 0, 2, 0, 0}; d->event(s, &e);
    e.parameter_id = 1; e.value = .25f; d->event(s, &e); d->reset(s); process();
    for (int i=0;i<5;++i) require(guarded[i+1] == a[i]*2+b[i]*.25f, "unequal gains no clipping");
    float reference[5]; std::copy_n(guarded+1, 5, reference); d->reset(s); process();
    require(!std::memcmp(reference, guarded+1, sizeof(reference)), "reset retains gains");
    float chunks[5]{}; const float *part[] = {a,b}; float *dest[] = {chunks};
    c.frames = 2; d->process(s,part,dest,&c); part[0]+=2;part[1]+=2;dest[0]+=2;c.frames=3;c.frame_position=2;
    d->process(s,part,dest,&c); require(!std::memcmp(reference,chunks,sizeof(chunks)), "mixer chunk invariance");
    e.parameter_id=0;e.value=0;d->event(s,&e);e.parameter_id=1;d->event(s,&e);d->reset(s);c.frames=5;process();
    require(std::all_of(guarded+1,guarded+6,[](float v){return v==0;}),"zero gains");
    d->destroy(s);
}
static std::vector<float> graph(uint32_t block, const char *directory) {
    ws_engine_config c{sizeof(c),48000,512,8,64}; Engine e(ws_engine_create(&c),ws_engine_destroy);
    for (auto d : {ws_test_vco_entry()}) require(ws_engine_register_module(e.get(),d)==WS_OK,"register");
    // Verify the compiled DLL, its exported descriptor and generic loader as well.
#ifdef _WIN32
    std::string path=std::string(directory)+"/ws_output.dll";
    std::string mixerPath=std::string(directory)+"/ws_mixer.dll";
#else
    std::string path=std::string(directory)+"/libws_output.so";
    std::string mixerPath=std::string(directory)+"/libws_mixer.so";
#endif
    require(ws_engine_load_module(e.get(),path.c_str())==WS_OK,"output DLL");
    require(ws_engine_load_module(e.get(),mixerPath.c_str())==WS_OK,"mixer DLL descriptor");
    require(ws_engine_add_node(e.get(),1,"essentials.vco")==0 && ws_engine_add_node(e.get(),2,"essentials.vco")==0 &&
      ws_engine_add_node(e.get(),3,"essentials.mixer")==0 && ws_engine_add_node(e.get(),4,"essentials.output")==0,"graph nodes");
    require(ws_engine_connect(e.get(),1,0,3,0)==0 && ws_engine_connect(e.get(),2,0,3,1)==0 &&
      ws_engine_connect(e.get(),3,0,4,0)==0 && ws_engine_prepare(e.get(),4,0)==0,"two VCO mixer graph");
    ws_event event{sizeof(event),0,2,WS_PARAMETER,0,330,0,0,ws_engine_generation(e.get())};
    require(ws_engine_enqueue(e.get(),&event)==0,"second pitch");
    std::vector<float> output(4096);for(uint32_t pos=0;pos<output.size();){auto n=std::min<uint32_t>(block,4096-pos);
      require(ws_engine_render(e.get(),output.data()+pos,n)==0,"mixed render");pos+=n;}
    require(std::any_of(output.begin(),output.end(),[](float v){return std::abs(v)>.5f;}),"mixed output");return output;
}
static void smoothing() {
    for (double rate : {44100.,48000.,96000.}) {
        auto d=ws_test_vca_entry(); void *s=d->create(rate);ws_module_config config{sizeof(config),512,rate,1};d->prepare(s,&config);
        float input[512],output[512];std::fill_n(input,512,1.f);const float *in[]={input,nullptr};float *out[]={output};
        ws_process_context c{sizeof(c),1,0,1};d->process(s,in,out,&c);
        ws_module_event event{WS_PARAMETER,0,0,0,0};d->event(s,&event);
        uint32_t frames=static_cast<uint32_t>(rate*.005);c.frames=frames;d->process(s,in,out,&c);
        require(output[0]>.9f && output[0]<1 && output[frames-1]==0,"5 ms gain ramp");
        for(uint32_t i=1;i<frames;++i)require(output[i]<=output[i-1],"monotonic gain ramp");d->destroy(s);
        d=ws_test_vco_entry();s=d->create(rate);d->prepare(s,&config);c.frames=1;d->process(s,nullptr,out,&c);
        event.value=880;d->event(s,&event);c.frames=frames;d->process(s,nullptr,out,&c);
        double phase=6.283185307179586*220/rate;
        for(uint32_t i=0;i<frames;++i) {
            require(std::abs(output[i]-std::sin(phase))<1e-5,"5 ms pitch ramp waveform");
            phase+=6.283185307179586*(220+660.*(i+1)/frames)/rate;
        }
        d->destroy(s);
        d=ws_test_mixer_entry();s=d->create(rate);d->prepare(s,&config);
        const float *mixIn[]={input,nullptr};c.frames=1;d->process(s,mixIn,out,&c);
        event.value=0;event.parameter_id=0;d->event(s,&event);c.frames=frames;d->process(s,mixIn,out,&c);
        require(output[0]>.49f && output[0]<.5f && output[frames-1]==0,"mixer level ramp");d->destroy(s);
        d=ws_test_adsr_entry();s=d->create(rate);d->prepare(s,&config);
        ws_module_event on{WS_GATE,0,1,1,1};d->event(s,&on);
        c.frames=512;for(int block=0;block<30;++block)d->process(s,nullptr,out,&c);
        event.parameter_id=2;event.value=.2f;d->event(s,&event);c.frames=frames;d->process(s,nullptr,out,&c);
        require(output[0]>.69f && std::abs(output[frames-1]-.2f)<1e-6,"sustain level ramp");d->destroy(s);
        auto ramped = [&](uint32_t block) {
            wirescape::LinearRamp ramp{1}; ramp.prepare(rate); std::vector<double> samples(1024);
            for(uint32_t base=0;base<samples.size();base+=block)
                for(uint32_t i=base;i<std::min<uint32_t>(base+block,1024);++i) {
                    if(i==1)ramp.set(0); if(i==50)ramp.set(.8); if(i==60)ramp.set(.8);
                    if(i==700)ramp.reset(); samples[i]=ramp.next();
                }
            require(samples[50]>0 && samples[50]<1 && samples[699]==.8 && samples[700]==.8,"ramp retarget and reset");
            return samples;
        };
        require(ramped(32)==ramped(512),"ramp partition invariance at all rates");
    }
}
int main(int argc,char **argv){try {mixer();smoothing();auto a=graph(32,argc>1?argv[1]:".");auto b=graph(512,argc>1?argv[1]:".");
    require(a==b,"mixed graph partition invariance");std::cout<<"PASS mixer descriptor/process/graph and smoothing\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
