// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../tests2/utility_port_tests.h"
static void latched_curve(){auto a=std::make_unique<DspFixture>(ws::Module<ws2ports::FunctionEnvelope>::descriptor()),b=std::make_unique<DspFixture>(ws::Module<ws2ports::FunctionEnvelope>::descriptor());for(auto*f:{a.get(),b.get()}){f->ins[0].flags=WS2_CONNECTED|WS2_CONSTANT;f->input[0][0][0]=1;f->parameter[2][0][0]=-1;f->run(1);}b->parameter[2][0][0]=1;a->run(17);b->run(17);for(int i=0;i<17;++i)VERIFY(a->output[0][0][i]==b->output[0][0][i]);float held=b->output[0][0][16];a->input[0][0][0]=b->input[0][0][0]=0;a->run(17);b->run(17);VERIFY(b->output[0][0][0]>0&&b->output[0][0][0]<held);bool differ=false;for(int i=0;i<17;++i)differ|=a->output[0][0][i]!=b->output[0][0][i];VERIFY(differ);b->reset();b->run(1);VERIFY(b->output[0][0][0]==0);}
int main(int argc,char**argv){latched_curve();return utility_port_tests("function_envelope",argc,argv);}
