// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "../modules2/adsr/dsp.h"
#include "../modules2/chaos_lite/dsp.h"
#include "../modules2/clock/dsp.h"
#include "../modules2/function_envelope/dsp.h"
#include "../modules2/host_keyboard/dsp.h"
#include "../modules2/ladder/dsp.h"
#include "../modules2/lfo/dsp.h"
#include "../modules2/midi_cc_to_cv/dsp.h"
#include "../modules2/midi_in/dsp.h"
#include "../modules2/midi_to_cv/dsp.h"
#include "../modules2/mixer/dsp.h"
#include "../modules2/noise/dsp.h"
#include "../modules2/output/dsp.h"
#include "../modules2/scale_offset/dsp.h"
#include "../modules2/slew/dsp.h"
#include "../modules2/vca/dsp.h"
#include "../modules2/vcf/dsp.h"
#include "../modules2/vco/dsp.h"
#include <vector>
namespace wirescape::app {
inline const std::vector<const ws2_descriptor *> &builtin_modules() {
    static const std::vector<const ws2_descriptor *> modules{
#define WS_APP_MODULE(Class) ws::Module<Class>::descriptor(),
#include "module_catalog.inc"
#undef WS_APP_MODULE
    };
    return modules;
}
} // namespace wirescape::app
