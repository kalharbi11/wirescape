# Six Essentials demo

Open Six_Essentials_Demo.json from Wirescape's Open button. Opening keeps audio off. Noise goes through VCF, Ladder, VCA and Output. Clock pulses drive Function Envelope, then Slew controls VCA gain. Clock starts running in the graph when it is rendered; it is not the host transport. Manual Hold Gate is not required for this patch. Adjust Noise Level, VCF/Ladder cutoff, Clock BPM and envelope Rise/Fall. Pan/zoom to explore the full graph. The VCF also exposes low12 if you want to replace the low24 connection.

This example intentionally uses all six imported modules plus the existing VCA and Output. It does not change user presets or default startup patches. The Function Envelope has a control-buffer gate input; the existing ADSR remains host-event gated. Slew is linear, not exponential lag. White noise is deterministic on reset, and the filters are original approximations rather than exact analog models.
