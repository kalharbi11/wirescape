// SPDX-License-Identifier: GPL-3.0-only
#include "midi_backend.h"
#include <cstdlib>
int main() { return wirescape::midi::real_backend(false) ? EXIT_FAILURE : EXIT_SUCCESS; }
