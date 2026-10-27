// SPDX-License-Identifier: GPL-3.0-only
#include "engine2.h"
#include "../../tests2/dsp_fixture.h"
#include "../../tests2/allocation_watch.h"
#include <memory>
using namespace wirescape::engine2;
static ws2_event note(uint32_t type,uint32_t source=1,int32_t id=7){ws2_event e{};e.struct_size=sizeof(e);e.type=type;e.key=69;e.note_id=id;e.source_id=source;e.channel=0;e.value=type==WS2_NOTE_ON?1.f:0.f;return e;}
static void adsr(){
 auto f=std::make_unique<DspFixture>(ws::Module<ws2ports::Adsr>::descriptor());f->ins[0].flags=WS2_CONSTANT;auto e=note(WS2_NOTE_ON);f->process.events={sizeof(ws2_event_list),1,&e};f->run(17);float held=f->output[0][0][16];
 e=note(WS2_NOTE_OFF,2);f->run(17);VERIFY(f->output[0][0][0]>held);held=f->output[0][0][16];e=note(WS2_NOTE_OFF,1,8);f->run(17);VERIFY(f->output[0][0][0]>held);held=f->output[0][0][16];e=note(WS2_NOTE_OFF);f->run(17);VERIFY(f->output[0][0][0]<held);
 f->reset();f->ins[0].flags=WS2_CONNECTED|WS2_CONSTANT;f->input[0][0][0]=0;e=note(WS2_NOTE_ON);f->run(17);for(int i=0;i<17;++i)VERIFY(f->output[0][0][i]==0);
 f->input[0][0][0]=1;e=note(WS2_NOTE_OFF);f->run(17);VERIFY(f->output[0][0][0]>0);held=f->output[0][0][16];f->input[0][0][0]=0;f->run(17);VERIFY(f->output[0][0][0]<held);
 auto poly=std::make_unique<DspFixture>(ws::Module<ws2ports::Adsr>::descriptor(),48000,4);poly->ins[0].flags=WS2_CONSTANT;e=note(WS2_NOTE_ON);e.channel=2;poly->process.events={sizeof(ws2_event_list),1,&e};poly->run(17);for(int c=0;c<4;++c)VERIFY((poly->output[0][c][0]>0)==(c==2));
}
static void lfo(){
 auto a=std::make_unique<DspFixture>(ws::Module<ws2ports::Lfo>::descriptor()),b=std::make_unique<DspFixture>(ws::Module<ws2ports::Lfo>::descriptor());VERIFY(a->parameter[3][0][0]==0);ws2_transport t{sizeof(t),WS2_PLAYING,300,.75,0,4,4,0,0};b->process.transport=&t;a->run(17);b->run(17);for(int i=0;i<17;++i)VERIFY(a->output[0][0][i]==b->output[0][0][i]);
 a->reset();b->reset();a->parameter[3][0][0]=1;b->process.transport=nullptr;b->parameter[3][0][0]=0;a->run(17);b->run(17);for(int i=0;i<17;++i)VERIFY(a->output[0][0][i]==b->output[0][0][i]);
 a->process.transport=&t;t.flags=0;t.beat=.25;a->run(17);for(int i=0;i<17;++i)VERIFY(a->output[0][0][i]==1);t.beat=.5;a->run(17);for(int i=0;i<17;++i)VERIFY(std::abs(a->output[0][0][i])<1.e-6f);t.flags=WS2_PLAYING;t.beat=0;a->run(17);VERIFY(a->output[0][0][0]==0&&a->output[0][0][1]>0);
}
static void vca(){
 for(uint32_t mask=0;mask<8;++mask){auto f=std::make_unique<DspFixture>(ws::Module<ws2ports::Vca>::descriptor(),48000,4);f->ins[0].flags=WS2_CONNECTED|((mask&1)?WS2_CONSTANT:0);f->ins[1].flags=WS2_CONNECTED|((mask&2)?WS2_CONSTANT:0);f->pars[0].flags=(mask&4)?WS2_CONSTANT:0;
  for(int c=0;c<4;++c)for(int i=0;i<17;++i){f->input[0][c][i]=1.f+i*.125f;f->input[1][c][i]=.25f+i*.015625f;f->parameter[0][c][i]=.5f+i*.03125f;}f->run(17);VERIFY(bool(f->outs[0].flags&WS2_CONSTANT)==(mask==7));
  for(int c=0;c<4;++c)for(int i=0;i<(mask==7?1:17);++i)VERIFY(f->output[0][c][i]==f->input[0][c][(mask&1)?0:i]*f->input[1][c][(mask&2)?0:i]*f->parameter[0][0][(mask&4)?0:i]);
 }
}
static void vcf(){
 auto f=std::make_unique<DspFixture>(ws::Module<ws2ports::Vcf>::descriptor());f->input[0][0][0]=NAN;VERIFY(f->d->process(f->state,&f->process)==WS2_INVALID);for(int p=0;p<2;++p)VERIFY((f->outs[p].flags&WS2_CONSTANT)&&f->output[p][0][0]==0);
 auto g=std::make_unique<DspFixture>(ws::Module<ws2ports::Vcf>::descriptor());f->reset();g->reset();std::fill_n(f->input[0][0],256,0);std::fill_n(g->input[0][0],256,0);f->run(17);for(int p=0;p<2;++p)for(int i=0;i<17;++i)VERIFY(f->output[p][0][i]==0);f->reset();f->input[0][0][0]=INFINITY;g->input[0][0][0]=1.e6f;f->run(17);g->run(17);for(int p=0;p<2;++p)for(int i=0;i<17;++i)VERIFY(std::isfinite(f->output[p][0][i])&&f->output[p][0][i]==g->output[p][0][i]);
 ws2ports::Vcf dsp;dsp.reset();auto probe=std::make_unique<DspFixture>(ws::Module<ws2ports::Vcf>::descriptor(),48000,4);probe->process.frames=17;ws::Block block(probe->process);dsp.process(block);VERIFY(dsp.coefficient_recomputes==4);probe->parameter[0][0][0]=2000;dsp.process(block);VERIFY(dsp.coefficient_recomputes==8);dsp.process(block);VERIFY(dsp.coefficient_recomputes==8);probe->pars[0].flags=0;for(int i=0;i<17;++i)probe->parameter[0][0][i]=1000+100.f*i;dsp.process(block);VERIFY(dsp.coefficient_recomputes==76);
}
static void vco(){
 auto a=std::make_unique<DspFixture>(ws::Module<ws2ports::Vco>::descriptor()),b=std::make_unique<DspFixture>(ws::Module<ws2ports::Vco>::descriptor());for(auto*f:{a.get(),b.get()}){f->ins[0].flags=WS2_CONSTANT;std::fill_n(f->input[1][0],256,0);}auto e=note(WS2_NOTE_ON);a->process.events=b->process.events={sizeof(ws2_event_list),1,&e};a->run(17);b->run(17);b->process.events.count=0;e=note(WS2_NOTE_OFF);a->run(17);b->run(17);for(int i=0;i<17;++i)VERIFY(a->output[0][0][i]==b->output[0][0][i]);e=note(WS2_NOTE_CHOKE);a->run(17);VERIFY(a->output[0][0][0]==0);VERIFY(a->output[0][0][1]==static_cast<float>(std::sin(6.283185307179586476925286766559*220./48000.)));
 a->process.events.count=0;a->ins[1].flags=WS2_CONNECTED|WS2_CONSTANT;a->input[1][0][0]=-1000;a->run(17);VERIFY(a->output[0][0][0]!=0);for(int i=1;i<17;++i)VERIFY(a->output[0][0][i]==a->output[0][0][0]);
}
struct Cv {
 static constexpr ws::Info info{"test.cv","CV",1};static constexpr std::array<ws2_port,0>inputs{};static constexpr std::array outputs{ws::port("cv","CV",WS2_CV_BI)};static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant();for(uint32_t c=0;c<b.channels();++c)b.out(0).ch(c)[0]=.75f;}
};
static void cv(){const ws2_descriptor*modules[]={ws::Module<ws2ports::Adsr>::descriptor(),ws::Module<ws2ports::Lfo>::descriptor(),ws::Module<ws2ports::Vca>::descriptor(),ws::Module<ws2ports::Vcf>::descriptor(),ws::Module<ws2ports::Vco>::descriptor()};
 for(auto*d:modules)for(uint32_t p=0;p<d->param_count;++p)if(!(d->params[p].flags&WS2_STEPPED)){VERIFY(d->params[p].flags&WS2_MODULATABLE);Engine e;VERIFY(e.add_node(1,ws::Module<Cv>::descriptor()));VERIFY(e.add_node(2,d));VERIFY(e.set_channels(1,4));if(d->input_count&&d->inputs[0].kind==WS2_SIGNAL)VERIFY(e.connect(1,0,2,0));VERIFY(e.modulate(1,0,2,p,.01f));VERIFY(e.output(2,0,0));VERIFY(e.prepare());float samples[256]{};float*out[]={samples};{allocation_watch::Scope watch;for(int i=0;i<3;++i)VERIFY(e.render(out,1,256)==WS2_OK);VERIFY(watch.allocations()==0);}VERIFY(e.meter().faults==0);for(float sample:samples)VERIFY(std::isfinite(sample));}
}
int main(){adsr();lfo();vca();vcf();vco();cv();std::puts("PASS existing-port behavior regressions and every continuous D7 parameter");}
