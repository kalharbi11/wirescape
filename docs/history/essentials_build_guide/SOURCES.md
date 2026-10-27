# Sources and source boundaries

Project facts are from copied ABI/header/engine/reference files and the full current-module audit. The legacy Python/browser builder kit is not the API for these tasks.

Primary behavior/source references checked October 26, 2026:

- Make Noise MATHS manual: https://www.makenoisemusic.com/wp-content/uploads/2024/03/MATHSmanual2013.pdf — behavior context for rise/fall/response only. No Make Noise code or graphics copied; the assigned Function Envelope is an original defined software behavior, not a claimed exact model.
- Official Mutable Instruments Plaits DSP voice: https://github.com/pichenettes/eurorack/blob/master/plaits/dsp/voice.h
- Official Braids macro oscillator: https://github.com/pichenettes/eurorack/blob/master/braids/macro_oscillator.h
- Shared library repository/license: https://github.com/pichenettes/stmlib and https://github.com/pichenettes/stmlib/blob/master/LICENSE

These are rolling branch references, NOT pinned dependencies or a completed port. No Mutable Instruments DSP source/binaries are bundled in this kit. A later local task must pin exact commits, inspect per-file licensing and dependencies, retain notices, and separate hardware-specific code from reusable DSP. Plaits and Braids are separate projects; choosing either does not mean both are implemented. Names do not imply endorsement.
