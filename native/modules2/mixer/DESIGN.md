<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Mixer design
DSP comes from `native/modules/mixer/module.cpp` at main `f23ea96`; double multiplies, addition order and 5 ms plain-unit ramps are retained. The engine also supplies its D7 parameter pipeline. Missing inputs contribute zero. Each voice has private fixed state; no process/reset allocation or I/O. Version-1 state uses little-endian binary64 ramp fields, and loads atomically.

Provenance: to be confirmed by the user. No ABI1 DESIGN.md exists; no external source or pinned upstream revision is claimed. License: GPL-3.0-only, full text in the root LICENSE.
