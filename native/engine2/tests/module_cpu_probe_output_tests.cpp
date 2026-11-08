// SPDX-License-Identifier: GPL-3.0-only
#include "module_cpu_probe_output.h"
#include <limits>
int main() {
    float left[] = {.25f, std::numeric_limits<float>::quiet_NaN(), -.5f};
    float right[] = {1.f, 2.f, std::numeric_limits<float>::infinity()};
    float *rows[] = {left, right};
    ws2_signal_out output{sizeof(ws2_signal_out), rows, 2, 0, 2};
    module_cpu_probe::OutputEvidence evidence;
    evidence.scan(output, {3, 2});
    if (evidence.peak != 2 || evidence.faults != 2) return 1;
    output.flags = WS2_CONSTANT;
    module_cpu_probe::OutputEvidence constant;
    constant.scan(output, {3, 2});
    if (constant.peak != 1 || constant.faults) return 2;
    output.channels = 3;
    constant.scan(output, {3, 2});
    if (constant.faults != 1) return 3;
    return 0;
}
