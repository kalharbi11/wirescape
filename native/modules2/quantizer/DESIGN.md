<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Quantizer design
Original code, GPL-3.0-only. No upstream source, no third-party code, no new dependency.

- Algorithm: textbook nearest-note quantization on a 12-bit pitch-class mask. Scales are interval lists (rotated by the root); Custom uses the note switches directly. Nearest search looks at most 12 semitones each way, so work per sample is bounded.
- Hysteresis: the held note's region (midpoints to its allowed neighbours, widened by 0.1 semitone) is cached when the note is chosen; inside it nothing is recomputed, outside it the nearest note is taken. A mask change invalidates the cache.
- Shift: counts allowed pitch classes below the note, adds the shift, wraps with floor division into octaves, and picks the matching set bit. Shift and octave come after hysteresis, so they never disturb the held note.
- Output `(midi - 60) / 12` is computed in double and rounded to float once, so allowed notes are the correctly rounded value.
- Settings are read at sample 0 of each block (stepped, unsmoothed). Held notes are not saved; they restart on reset or load.
- Input is clamped to +-1200 semitones, so extreme or non-finite input stays finite.
