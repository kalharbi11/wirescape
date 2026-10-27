#pragma once
// Host output policy for live stereo duplication and offline 16-bit mono WAVs.
// Processor buffers remain unscaled; apply once at the device/file boundary.
inline constexpr float WS_OUTPUT_GAIN = .2f;
