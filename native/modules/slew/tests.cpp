// SPDX-License-Identifier: GPL-3.0-only
#define MODULE_ID "essentials.slew"
#define MODULE_NAME "Slew"
#define INPUTS 1
#define OUTPUTS 1
#define PARAMS 2
#define IS_CLOCK 0
#define IS_ENVELOPE 0
#define IS_SLEW 1
#define PARAMS_MORE_THAN_ONE 1
#define PARAMS_MORE_THAN_TWO 0
#define PARAM0_MAX 10.0f
#define INPUT_PORT_IDS "control"
#define OUTPUT_PORT_IDS "control"
#define INPUT_KINDS WS_CONTROL
#define OUTPUT_KINDS WS_CONTROL
#define PARAM_MINS 0.0f,0.0f
#define PARAM_MAXS 10.0f,10.0f
#define PARAM_DEFAULTS 0.1f,0.1f
#define PARAM_NAMES "Rise","Fall"
#define PARAM_UNITS "s/unit","s/unit"
#define PARAM0_VALUES (std::array<float,4>{{0.0f,0.5f,10.0f,0.01f}})
#define PARAM1_VALUES (std::array<float,4>{{0.0f,0.7f,0.01f,10.0f}})
#define PARAM2_VALUES (std::array<float,4>{{0.0f,0.0f,0.0f,0.0f}})

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

void specific(const ws_module_descriptor *d) {
    for(double rate:{44100.0,48000.0,96000.0}) {
        Module m(d,rate);m.set(0,0.1f);m.set(1,0.2f);
        uint32_t n=static_cast<uint32_t>(std::ceil(rate*0.1));
        std::vector<float> step(n+16,1.0f),out(n+16);
        uint32_t pos=0;
        while(pos<step.size()) { uint32_t batch=std::min<uint32_t>(512,step.size()-pos);
            render(d,m.p,step.data()+pos,out.data()+pos,nullptr,batch,pos);pos+=batch; }
        for(uint32_t i=0;i<step.size();++i)
            approx(out[i],std::min(1.0, double(i+1)/(rate*static_cast<double>(0.1f))),2e-6,"linear rise analytic trajectory");
        std::vector<float> fall(static_cast<size_t>(rate*0.2)+16,-0.0f),down(fall.size());pos=0;
        while(pos<fall.size()) { uint32_t batch=std::min<uint32_t>(512,fall.size()-pos);
            render(d,m.p,fall.data()+pos,down.data()+pos,nullptr,batch,pos);pos+=batch; }
        for(uint32_t i=0;i<fall.size();++i)
            approx(down[i],std::max(0.0,1.0-double(i+1)/(rate*static_cast<double>(0.2f))),2e-6,"linear fall analytic trajectory");
    }
    Module m(d,48000);
    m.set(0,0);m.set(1,0);
    float target=4.5f,out=0;
    render(d,m.p,&target,&out,nullptr,1);check(out==4.5f,"rise zero-time must snap unclipped >1");
    target=-3.5f;
    render(d,m.p,&target,&out,nullptr,1);check(out==-3.5f,"fall zero-time must snap negative");
    m.set(0,0.1f);target=-2.5f;
    render(d,m.p,&target,&out,nullptr,1);
    approx(out,-3.5+1.0/(48000.0*double(0.1f)),1e-6,"changing target uses live rate immediately");
    m.reset();target=0;
    render(d,m.p,&target,&out,nullptr,1);check(out==0.0f,"reset output0 independent of stored rate targets");
    m.set(0,10.0f);m.reset();target=1.0f;
    render(d,m.p,&target,&out,nullptr,1);
    approx(out,1.0/(48000*10.0),1e-9,"maximum Rise endpoint is 10s per unit");
    // 2-unit travel at 0.1 seconds/unit takes 9600 samples.
    target=2.0f; m.set(0,0.1f);m.reset();
    for(int i=0;i<9599;++i)render(d,m.p,&target,&out,nullptr,1);
    check(out<2.0f,"two-unit transition prematurely completed");
    render(d,m.p,&target,&out,nullptr,1);approx(out,2.0,1e-5,"two-unit expected duration");
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
