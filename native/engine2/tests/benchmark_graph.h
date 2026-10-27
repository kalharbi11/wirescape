// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "engine2.h"
#include "../../tests2/dsp_fixture.h"
#include <memory>
using namespace wirescape::engine2;
static std::unique_ptr<Engine>make(uint32_t voices,bool mod){Config c;c.max_channels=voices;auto e=std::make_unique<Engine>(c);VERIFY(e->add_node(1,ws::Module<ws2ports::Vco>::descriptor()));VERIFY(e->add_node(2,ws::Module<ws2ports::Vcf>::descriptor()));VERIFY(e->add_node(3,ws::Module<ws2ports::Vca>::descriptor()));VERIFY(e->set_channels(1,voices));VERIFY(e->connect(1,0,2,0));VERIFY(e->connect(2,1,3,0));VERIFY(e->output(3,0,0));if(mod){VERIFY(e->add_node(4,ws::Module<ws2ports::Lfo>::descriptor()));VERIFY(e->modulate(4,0,2,0,.2f));}VERIFY(e->prepare());return e;}
