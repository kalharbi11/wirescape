// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../tests2/utility_port_tests.h"
static void unpatched_offset(){auto f=std::make_unique<DspFixture>(ws::Module<ws2ports::ScaleOffset>::descriptor());f->ins[0].channels=0;f->ins[0].flags=WS2_CONSTANT;f->parameter[1][0][0]=.5f;f->run(17);for(int i=0;i<17;++i)VERIFY(f->output[0][0][i]==.5f);f->parameter[1][0][0]=-.5f;f->run(17);VERIFY(f->output[0][0][0]<.5f&&f->output[0][0][0]>-.5f);f->reset();f->run(17);for(int i=0;i<17;++i)VERIFY(f->output[0][0][i]==-.5f);}
int main(int argc,char**argv){unpatched_offset();return utility_port_tests("scale_offset",argc,argv);}
