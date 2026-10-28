// SPDX-License-Identifier: GPL-3.0-only
#include "dsp.h"
#include "../../tests2/utility_port_tests.h"
static void muted_sequence(){
 auto a=std::make_unique<DspFixture>(ws::Module<ws2ports::Noise>::descriptor(),48000,16),b=std::make_unique<DspFixture>(ws::Module<ws2ports::Noise>::descriptor(),48000,16);
 a->parameter[0][0][0]=0;for(int i=0;i<2;++i){a->run();b->run();for(int c=0;c<16;++c)for(int j=0;j<256;++j)VERIFY(a->output[0][c][j]==0);}
 a->parameter[0][0][0]=b->parameter[0][0][0]=.25f;a->run();b->run();a->run(17);b->run(17);
 bool differ=false;for(int c=0;c<16;++c)for(int i=0;i<17;++i){VERIFY(a->output[0][c][i]==b->output[0][c][i]);if(c&&a->output[0][c][i]!=a->output[0][0][i])differ=true;}VERIFY(differ);
}
int main(int argc,char**argv){muted_sequence();return utility_port_tests("noise",argc,argv);}
