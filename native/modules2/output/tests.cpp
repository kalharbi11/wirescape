// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../tests2/utility_port_tests.h"
#include "../../engine2/engine2.h"
struct BusSource {
 static constexpr ws::Info info{"test.bus","Bus source",1};static constexpr std::array<ws2_port,0>inputs{};static constexpr std::array outputs{ws::port("audio","Audio",WS2_AUDIO)};static constexpr std::array<ws2_param,0>params{};
 void prepare(const ws::Setup&){}void reset(){}
 void process(ws::Block&b){b.out(0).set_channels(b.channels());b.out(0).mark_constant();for(uint32_t c=0;c<b.channels();++c)b.out(0).ch(c)[0]=float(c+1);}
};
static void bus(){wirescape::engine2::Engine e;VERIFY(e.add_node(1,ws::Module<BusSource>::descriptor()));VERIFY(e.add_node(2,ws::Module<ws2ports::Output>::descriptor()));VERIFY(e.set_channels(1,4));VERIFY(e.connect(1,0,2,0));VERIFY(e.output(2,0,0));VERIFY(e.prepare());float buffer[17]{};float*out[]={buffer};{allocation_watch::Scope watch;VERIFY(e.render(out,1,17)==WS2_OK);VERIFY(watch.allocations()==0);}VERIFY(e.output_channels(2,0)==4);for(float value:buffer)VERIFY(value==10);}
int main(int argc,char**argv){bus();return utility_port_tests("output",argc,argv);}
