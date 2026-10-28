<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Bounded OSC codec

Original GPL-3.0-only code. Reference: https://opensoundcontrol.stanford.edu/spec-1_0.html .
Types i/f/s/b/T/F/N/I; arrays rejected,32 arguments,255-byte address, strict padding,
checked blob lengths and at most4 nested bundles. Bundles are validated before service
callbacks and flattened immediately, ignoring timetags for J. Parser views borrow the
current packet only. No allocation occurs during parsing/matching/normalization.
Normalized ABI payloads are at most1472bytes; long strings/blobs are truncated with
valid packet structure and a visible truncation counter; numeric fields survive.
Glob matching is a bounded byte-indexed NFA (maximum256states), with wildcard matching
never crossing slash and explicit classes/alternatives. No regex or exponential backtracking.
The malformed/truncated corpus and allocator instrumentation are in CTest.
