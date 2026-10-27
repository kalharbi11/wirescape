// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../tests2/utility_port_tests.h"
static void missing_and_nonfinite(){auto f=std::make_unique<DspFixture>(ws::Module<ws2ports::Slew>::descriptor());std::fill_n(f->input[0][0],256,1);f->run();float held=f->output[0][0][255];VERIFY(held>0);f->ins[0].channels=0;f->ins[0].flags=WS2_CONSTANT;f->run(1);VERIFY(f->output[0][0][0]>0&&f->output[0][0][0]<held);f->ins[0].channels=1;f->ins[0].flags=WS2_CONNECTED;f->parameter[0][0][0]=0;f->run(1);VERIFY(f->output[0][0][0]==1);for(float value:{NAN,INFINITY}){f->input[0][0][0]=value;f->run(1);VERIFY(f->output[0][0][0]==1);}f->reset();}
int main(int argc,char**argv){missing_and_nonfinite();return utility_port_tests("slew",argc,argv);}
