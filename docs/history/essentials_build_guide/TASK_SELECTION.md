# Task selection

READY: noise, slew, function_envelope, clock. READY_ADVANCED_REVIEW: vcf, ladder. Each JSON reserves folder, identity, ports, parameter IDs/defaults, exact behavior and acceptance. Pass one JSON to one agent with this entire kit. Do not start CORE_FIRST work using these ready templates.

These specs are bounded first versions: Noise starts with white noise; Slew uses linear rise/fall rate limiting, not exponential lag; Function Envelope is gated AR without cycling; Clock is control-buffer pulses; VCF exposes 12/24 low-pass outputs. Extensions need separate compatible contracts and tests. No returned module is accepted until Codex checks integration locally.
