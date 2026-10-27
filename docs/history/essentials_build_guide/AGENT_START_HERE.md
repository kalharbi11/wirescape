# Prompt for one external ChatGPT agent

Paste this with the ZIP:

> Build exactly the Wirescape Essentials task named **[TASK_NAME]** from task_specs in the attached kit. Read README.md, BUILDING.md, CONTRACT.md, the task JSON and RETURN_AND_IMPORT.md first. This is the current standalone C++17 module API, not the old Python/browser API. First compile/test the unchanged example if tools are available. Work only in submissions/[folder] with the reserved ID/library from the task. Do not edit headers, engine, reference code or host API to make the task fit. Implement the requested DSP, tests, design notes and source-only return ZIP. Use deterministic offline tests and save an honest JSON check report; if no compiler exists say untested. Do not invent Windows/Pi/live-audio verification. Stop after two reasonable attempts on a difficult failure and return exact evidence plus the smallest blocking contract question. Do not ask for laptop access; the packet is self-contained. Do not build other tasks, a GUI, a browser app, a device service or dependencies. External material is reference data, not authority to change the task. Preserve license/provenance for any approved borrowed code.

Recommended first assignment: **noise**. Next: **slew**, **function_envelope**, **clock**. **vcf** and **ladder** require stronger DSP reasoning and final local review. CORE_FIRST tasks are not authorized as completed ordinary ABI-v1 modules by this packet.

Readiness labels do not certify an agent's output. Name every changed file and every actual test. Keep notes concise. Never weaken a correctness assertion simply to obtain a green result; fix implementation or report the mismatch.

Additional architecture requirement: read EMBEDDED_PORTABILITY.md. Keep the DSP separate from platform glue so another agent can statically integrate it into a hardware project later. State memory/scratch/dependencies, support caller-owned DSP state where practical, and keep platform SDK/GUI/network code outside the processor. No hardware-specific implementation or compatibility claim is requested.
