# Wirescape Essentials — external agent kit

Start with AGENT_START_HERE.md. This kit is for the CURRENT standalone C++17/Qt app, not the legacy Python/browser module kit. User-facing built-ins are **Essentials**; native/ is an implementation directory, not a category to display.

Use one regular ChatGPT conversation per task. Upload this entire ZIP and paste the prompt in AGENT_START_HERE.md with exactly one READY assignment. An agent with files and a C++ compiler can work without laptop access. No compiler is also usable for drafting, but returned work must clearly say untested; never fabricate a build result.

Read MODULE_STATUS_AND_OWNERSHIP.md for every requested module. task_specs contains six independent DSP assignments plus CORE_FIRST.md for work that must not be presented as a finished ordinary module yet. Build the example before editing. Keep the ABI, host and reference files unchanged. No network/device calls, Qt, Python DSP or custom UI are needed for READY tasks.

Return SOURCE files and tests. Do not return compiled executables, DLLs, fonts, user data or the entire app. RETURN_AND_IMPORT.md explains exactly where to put the result and how Codex integrates it. The example is teaching material, not a requested product module.

The ABI is experimental. References and host are exact project snapshots listed in SOURCE_PROVENANCE.json. Inclusion does not mean event outputs, polyphony or arbitrary data messages are supported. The packet itself does not build new requested modules.

BUILDING.md and tools/run_checks.py cover offline builds and JSON results; tools/verify_submission.py is a structural check only. LICENSE is the project GPLv3 text. Preserve upstream license notices if a separately approved task uses third-party code; no third-party DSP code or fonts are bundled here.
