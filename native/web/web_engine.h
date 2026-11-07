// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
// C surface for the browser demo (one thread: commands and renders interleave).
extern "C" {
int wsw_init(double sample_rate);
const char *wsw_catalogue();
// Protocol v0 command object (type + fields). Returns {"ok":bool,"gen":"N"[,"code","message"]}.
const char *wsw_command(const char *json);
const char *wsw_telemetry();
// Renders frames (<= 1024) and returns the left buffer; wsw_right() is the right buffer.
float *wsw_render(uint32_t frames);
float *wsw_right();
float wsw_peak();
char *wsw_alloc(uint32_t bytes);
void wsw_free(char *);
}
