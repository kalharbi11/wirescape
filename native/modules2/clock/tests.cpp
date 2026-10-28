// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../tests2/utility_port_tests.h"
static void rounded_ticks_run_reset(){auto f=std::make_unique<DspFixture>(ws::Module<ws2ports::Clock>::descriptor());std::fill_n(f->input[0][0],256,0);f->parameter[1][0][0]=1.6f;int high=0;for(int block=0;block<30;++block){f->run();for(int i=0;i<256;++i)high+=f->output[0][0][i]==1;}VERIFY(high==6000);f->parameter[3][0][0]=.49f;f->run(17);for(int i=0;i<17;++i)VERIFY(f->output[0][0][i]==0);f->parameter[3][0][0]=.5f;std::fill_n(f->input[0][0],256,.5f);f->run(1);VERIFY(f->output[0][0][0]==1);bool low=false;for(int block=0;block<30;++block){f->run();for(int i=0;i<256;++i)low|=f->output[0][0][i]==0;}VERIFY(low);f->input[0][0][0]=0;f->run(1);f->input[0][0][0]=1;f->run(1);VERIFY(f->output[0][0][0]==1);}
int main(int argc,char**argv){rounded_ticks_run_reset();return utility_port_tests("clock",argc,argv);}
