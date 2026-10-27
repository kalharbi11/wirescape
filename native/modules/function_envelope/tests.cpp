// SPDX-License-Identifier: GPL-3.0-only
#define MODULE_ID "essentials.function_envelope"
#define MODULE_NAME "Function Envelope"
#define INPUTS 1
#define OUTPUTS 1
#define PARAMS 3
#define IS_CLOCK 0
#define IS_ENVELOPE 1
#define IS_SLEW 0
#define PARAMS_MORE_THAN_ONE 1
#define PARAMS_MORE_THAN_TWO 1
#define PARAM0_MAX 30.0f
#define INPUT_PORT_IDS "gate"
#define OUTPUT_PORT_IDS "envelope"
#define INPUT_KINDS WS_CONTROL
#define OUTPUT_KINDS WS_CONTROL
#define PARAM_MINS 0.0001f,0.0001f,-1.0f
#define PARAM_MAXS 30.0f,30.0f,1.0f
#define PARAM_DEFAULTS 0.01f,0.1f,0.0f
#define PARAM_NAMES "Rise","Fall","Curve"
#define PARAM_UNITS "s","s",""
#define PARAM0_VALUES (std::array<float,4>{{0.001f,0.1f,1.0f,0.02f}})
#define PARAM1_VALUES (std::array<float,4>{{0.004f,0.09f,0.3f,0.002f}})
#define PARAM2_VALUES (std::array<float,4>{{-1.0f,1.0f,0.0f,0.5f}})

#include "wirescape/engine.h"
#include "wirescape/module.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr double pi=3.14159265358979323846;
void check(bool okay, const std::string &message) { if (!okay) throw std::runtime_error(message); }
void approx(double actual, double expected, double tol, const std::string &why) {
    if (!std::isfinite(actual) || std::abs(actual-expected)>tol)
        throw std::runtime_error(why+" actual="+std::to_string(actual)+" expected="+std::to_string(expected));
}
struct EngineDeleter { void operator()(ws_engine *e) const { ws_engine_destroy(e); } };
struct Module {
    const ws_module_descriptor *d=nullptr;
    void *p=nullptr;
    explicit Module(const ws_module_descriptor *desc, double rate, uint32_t maxframes=512):d(desc) {
        p=d->create(rate); check(p!=nullptr,"create failed");
        ws_module_config c{sizeof(c),maxframes,rate,1}; check(d->prepare(p,&c)==0,"prepare failed");
    }
    ~Module(){ if(p)d->destroy(p); }
    Module(const Module&)=delete; Module&operator=(const Module&)=delete;
    void reprepare(double rate) { ws_module_config c{sizeof(c),512,rate,2};check(d->prepare(p,&c)==0,"reprepare failed"); }
    void set(uint32_t id, float v) { ws_module_event e{WS_PARAMETER,id,v,0,0}; d->event(p,&e); }
    void reset() {d->reset(p);}
};
void render(const ws_module_descriptor *d, void *p, const float *input, float *first, float *second,
            uint32_t frames, uint64_t pos=0) {
    const float *ins[]={input}; float *outs[]={first,second};
    ws_process_context ctx{sizeof(ctx),frames,pos,1};
    d->process(p, d->input_count ? ins : nullptr, outs, &ctx);
}
void verify_descriptor(const ws_module_descriptor *d) {
    check(d && d->struct_size>=sizeof(ws_module_descriptor) && d->abi_version==1,"ABI mismatch");
    check(d->id && std::strcmp(d->id, MODULE_ID)==0,"module ID mismatch");
    check(d->name && std::strcmp(d->name, MODULE_NAME)==0,"module name mismatch");
    check(d->input_count==INPUTS && d->output_count==OUTPUTS && d->parameter_count==PARAMS,"counts mismatch");
    const char *input_names[] = { INPUT_PORT_IDS };
    const char *output_names[] = { OUTPUT_PORT_IDS };
    const uint32_t input_kinds[] = { INPUT_KINDS };
    const uint32_t output_kinds[] = { OUTPUT_KINDS };
    for(uint32_t i=0;i<INPUTS;++i)
        check(d->inputs && std::strcmp(d->inputs[i].id,input_names[i])==0 && d->inputs[i].kind==input_kinds[i],"input ports mismatch");
    for(uint32_t i=0;i<OUTPUTS;++i)
        check(d->outputs && std::strcmp(d->outputs[i].id,output_names[i])==0 && d->outputs[i].kind==output_kinds[i],"output ports mismatch");
    const float mins[]={ PARAM_MINS }, maxs[]={ PARAM_MAXS }, initial[]={ PARAM_DEFAULTS };
    const char *names[]={ PARAM_NAMES }, *units[]={ PARAM_UNITS };
    for(uint32_t i=0;i<PARAMS;++i) {
        check(d->parameters && d->parameters[i].id==i && std::strcmp(d->parameters[i].name,names[i])==0,
              "parameter ID or name mismatch");
        check(d->parameters[i].minimum==mins[i] && d->parameters[i].maximum==maxs[i] &&
              d->parameters[i].initial==initial[i] && std::strcmp(d->parameters[i].unit,units[i])==0,
              "parameter range, unit, or default mismatch");
    }
}
void common_guards(const ws_module_descriptor *d) {
    Module m(d,48000.0);
    float x[]={0.75f,0.75f};
    float a[]={-777.0f,-99.0f,-778.0f}, b[]={-779.0f,-99.0f,-780.0f};
    const float *ins[]={x}; float *outs[]={a+1,b+1};
    ws_process_context ctx{sizeof(ctx),0,0,1};
    d->process(m.p, d->input_count ? ins:nullptr,outs,&ctx);
    check(a[0]==-777 && a[1]==-99 && a[2]==-778,"zero-frame first-output guard");
    check(b[0]==-779 && b[1]==-99 && b[2]==-780,"zero-frame second-output guard");
    ctx.frames=1;
    d->process(m.p, d->input_count ? ins:nullptr,outs,&ctx);
    check(a[0]==-777 && a[2]==-778 && b[0]==-779 && b[2]==-780,"single-frame adjacent guards");
    a[1]=-99;b[1]=-99;ctx.struct_size=0;
    d->process(m.p,d->input_count ? ins:nullptr,outs,&ctx);
    check(a[1]==-99 && b[1]==-99,"undersized context wrote outputs");
    ws_module_config bad{0,0,0,0};
    check(d->prepare(m.p,&bad)!=0,"invalid prepare accepted");
    bad={sizeof(bad),512,std::numeric_limits<double>::infinity(),0};
    check(d->prepare(m.p,&bad)!=0,"nonfinite prepare accepted");
    ws_module_event ev{WS_PARAMETER,0,std::numeric_limits<float>::quiet_NaN(),0,0};
    d->event(m.p,&ev);
    ev.parameter_id=999;ev.value=0;d->event(m.p,&ev);
    d->event(m.p,nullptr);d->process(nullptr,nullptr,outs,&ctx);
    float *nullouts[]={nullptr,nullptr};ctx.struct_size=sizeof(ctx);
    d->process(m.p,nullptr,nullouts,&ctx);
    d->process(m.p,nullptr,nullptr,&ctx);
}
std::vector<float> fixture_input(uint32_t count) {
    std::vector<float> x(count);
    for(uint32_t i=0;i<count;++i) {
#if IS_CLOCK
        x[i] = (i>=750 && i<860) || (i>=1250 && i<1345) ? 1.0f : 0.0f;
#elif IS_ENVELOPE
        x[i] = (i>=50 && i<480) || (i>=610 && i<1700) ? 1.0f : 0.0f;
#elif IS_SLEW
        x[i] = i<321 ? -0.8f : (i<888 ? 1.6f : (i<1444 ? -2.3f : 0.25f));
#else
        x[i] = static_cast<float>(0.7*std::sin(i*0.017)+0.2*std::cos(i*0.123));
#endif
    }
    return x;
}
std::vector<float> partition_run(const ws_module_descriptor *d, double rate, uint32_t part) {
    constexpr uint32_t total=2273;
    const auto input=fixture_input(total);
    Module m(d,rate);
    std::vector<float> a(total,-200),b(total,-200);
    const std::array<uint32_t,4> boundaries{{113,521,912,1703}};
    uint32_t pos=0;
    while(pos<total) {
        for(uint32_t j=0;j<boundaries.size();++j) if(pos==boundaries[j]) {
            m.set(0,PARAM0_VALUES[j]);
#if PARAMS_MORE_THAN_ONE
            m.set(1,PARAM1_VALUES[j]);
#endif
#if PARAMS_MORE_THAN_TWO
            m.set(2,PARAM2_VALUES[j]);
#endif
        }
        uint32_t n=std::min(part,total-pos);
        for(auto bnd:boundaries) if (bnd>pos) n=std::min(n,bnd-pos);
        render(d,m.p,input.data()+pos,a.data()+pos,b.data()+pos,n,pos);
        pos+=n;
    }
    for(float x:a) check(std::isfinite(x),"nonfinite output in partition run");
    if(OUTPUTS>1)for(float x:b) check(std::isfinite(x),"nonfinite second output in partition run");
    a.insert(a.end(),b.begin(),b.end()); return a;
}
void partition_tests(const ws_module_descriptor *d) {
    for(double rate:{44100.0,48000.0,96000.0}) {
        const auto reference=partition_run(d,rate,512);
        for(uint32_t part:{1u,17u,64u,128u,512u}) {
            const auto other=partition_run(d,rate,part);
            check(reference.size()==other.size(),"partition size mismatch");
            for(size_t i=0;i<other.size();++i) check(other[i]==reference[i],
                "different samples across partitions at index "+std::to_string(i));
        }
    }
}
void reset_instances_and_missing(const ws_module_descriptor *d) {
    Module a(d,48000),b(d,48000);
    std::array<float,256> input{};input.fill(0.5f);
    std::array<float,256> ax{},bx{},ay{},by{};
    render(d,a.p,d->input_count ? input.data():nullptr,ax.data(),ay.data(),256);
    render(d,b.p,d->input_count ? input.data():nullptr,bx.data(),by.data(),256);
    check(ax==bx && (OUTPUTS==1 || ay==by),"instances do not start independently identically");
    a.reset();
    ax.fill(0);ay.fill(0);
    render(d,a.p,d->input_count ? input.data():nullptr,ax.data(),ay.data(),256);
    check(ax==bx && (OUTPUTS==1 || ay==by),"reset did not restore exact initial state");
    a.reprepare(48000);
    ax.fill(0);ay.fill(0);
    render(d,a.p,d->input_count ? input.data():nullptr,ax.data(),ay.data(),256);
    check(ax==bx && (OUTPUTS==1 || ay==by),"reprepare did not restore initial state");
    // Unpatched signal must be supported; a finite, fully written output is required.
    a.reset();ax.fill(-999);ay.fill(-999);
    render(d,a.p,nullptr,ax.data(),ay.data(),256);
    for(float x:ax) check(x!=-999 && std::isfinite(x),"missing-input process did not write output");
    if(OUTPUTS>1)for(float x:ay)check(x!=-999 && std::isfinite(x),"missing-input second output unwritten");
    // Invalid parameters leave the initialized state intact.
    Module c(d,48000),e(d,48000);
    ws_module_event bad{WS_PARAMETER,0,std::numeric_limits<float>::infinity(),0,0};
    c.d->event(c.p,&bad);
    bad.value=PARAM0_MAX+1.0f;c.d->event(c.p,&bad);
    render(d,c.p,d->input_count ? input.data():nullptr,ax.data(),ay.data(),256);
    render(d,e.p,d->input_count ? input.data():nullptr,bx.data(),by.data(),256);
    check(ax==bx && (OUTPUTS==1 || ay==by),"invalid parameter event affected output");
}

double shape(double u, double curve) {
    double k=4.0*curve;return curve==0.0 ? u : std::expm1(k*u)/std::expm1(k);
}
void specific(const ws_module_descriptor *d) {
    for(double rate:{44100.0,48000.0,96000.0})for(float curve:{-1.0f,0.0f,1.0f}) {
        Module m(d,rate);m.set(0,0.01f);m.set(1,0.02f);m.set(2,curve);
        const int n=std::max(1,int(std::llround(double(0.01f)*rate)));
        std::vector<float> gate(n+100,1.0f),out(n+100);
        uint32_t pos=0;while(pos<gate.size()) {uint32_t batch=std::min<uint32_t>(512,gate.size()-pos);
            render(d,m.p,gate.data()+pos,out.data()+pos,nullptr,batch,pos);pos+=batch;}
        for(int at:{0,n/2-1,n-1})approx(out[at],shape(double(at+1)/n,curve),2e-5,"curve analytical shape");
        for(size_t i=n;i<out.size();++i)check(out[i]==1.0f,"held high does not hold exactly1");
        gate.assign(static_cast<size_t>(std::llround(double(0.02f)*rate))+5,0);out.assign(gate.size(),-1);
        pos=0;while(pos<gate.size()){uint32_t batch=std::min<uint32_t>(512,gate.size()-pos);
            render(d,m.p,gate.data()+pos,out.data()+pos,nullptr,batch,pos);pos+=batch;}
        int f=std::max(1,int(std::llround(double(0.02f)*rate)));
        approx(out[f/2-1],1.0-shape(double(f/2)/f,curve),2e-5,"fall analytical midpoint");
        for(size_t i=f-1;i<out.size();++i)check(out[i]==0,"release must clamp exact endpoint");
    }
    Module m(d,1000);m.set(0,0.1f);m.set(1,0.2f);m.set(2,0.0f);
    float gate=1,out=0;
    for(int i=0;i<40;++i)render(d,m.p,&gate,&out,nullptr,1);
    approx(out,0.4,1e-6,"partial rise at sample40");
    // Duration/curve must latch at rise onset; repeated high cannot retrigger.
    m.set(0,1.0f);m.set(2,1.0f);
    for(int i=0;i<60;++i)render(d,m.p,&gate,&out,nullptr,1);
    check(out==1.0f,"mid-rise knob changes must not change latched trajectory");
    gate=0;render(d,m.p,&gate,&out,nullptr,1);
    approx(out,1.0-shape(1.0/200.0,1.0),1e-6,"fall starts with latched new curve");
    // New rising edge from nonzero level, at new long duration.
    const double previous=out;gate=1;render(d,m.p,&gate,&out,nullptr,1);
    approx(out,previous+(1.0-previous)*shape(1.0/1000.0,1.0),1e-6,"continuous retrigger from level");
    m.reset();gate=0;render(d,m.p,&gate,&out,nullptr,1);check(out==0.0f,"reset/unpatched stays zero");
    // Both segment-duration descriptor endpoints are valid.
    Module endpoints(d,48000);
    endpoints.set(0,30.0f);endpoints.set(2,0.0f);
    gate=1;render(d,endpoints.p,&gate,&out,nullptr,1);
    approx(out,1.0/(30*48000),1e-8,"30s endpoint rise duration");
    endpoints.reset();endpoints.set(0,0.0001f);
    for(int i=0;i<5;++i)render(d,endpoints.p,&gate,&out,nullptr,1);
    check(out==1.0f,"minimum rise duration 5 samples at 48k");
    // Early release 50% way through rise starts fall from current value.
    m.set(0,0.1f);m.set(1,0.2f);m.set(2,0);m.reset();gate=1;
    for(int i=0;i<50;++i)render(d,m.p,&gate,&out,nullptr,1);
    approx(out,0.5,1e-6,"50%-rise setup");gate=0;
    render(d,m.p,&gate,&out,nullptr,1);approx(out,0.5*(1.0-1.0/200.0),1e-6,"early fall starts from 0.5");
}

}
int main(int argc,char **argv) {
    try {
        check(argc==2,"expected module shared-library path argument");
        ws_engine_config cfg{sizeof(cfg),48000,512,8,64};
        std::unique_ptr<ws_engine,EngineDeleter> engine(ws_engine_create(&cfg));
        check(engine!=nullptr,"engine creation failed");
        check(ws_engine_load_module(engine.get(),argv[1])==WS_OK,"actual offline engine DLL/.so discovery failed");
        check(ws_engine_module_count(engine.get())==1,"discovery count mismatch");
        const auto *d=ws_engine_module(engine.get(),0);
        verify_descriptor(d);
        common_guards(d);
        reset_instances_and_missing(d);
        partition_tests(d);
        specific(d);
        std::printf("PASS: %s engine discovery, guards, instances, reset, rate/partition and DSP assertions\n",MODULE_ID);
        return 0;
    }catch(const std::exception &error){std::fprintf(stderr,"FAIL %s: %s\n",MODULE_ID,error.what());return 1;}
}
