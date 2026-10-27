"""Summarize completed offline benchmark files without rerunning audio tests."""
from __future__ import annotations
import csv
import json
import pathlib
import re
import statistics

ROOT=pathlib.Path(__file__).resolve().parents[2]
OUT=ROOT/'build_reports'/'bench'

def read_csv(name):
    with (OUT/name).open(newline='',encoding='utf-8-sig') as f:return list(csv.DictReader(f))

def med(rows,key):return statistics.median(float(r[key]) for r in rows)
def fmt(value,digits=2):return f'{value:.{digits}f}'

chaos=read_csv('chaos_performance.csv')
graph=read_csv('graph_performance.csv')
memory=read_csv('memory.csv')
env=json.loads((OUT/'environment.json').read_text(encoding='utf-8'))
baseline=json.loads((OUT/'baseline_tests.json').read_text(encoding='utf-8-sig'))
profile_text=(OUT/'profiling_summary.txt').read_text(encoding='utf-8')
profile=json.loads(re.search(r'Wall-clock boundary timings[^\n]*\n(\{[\s\S]*?\})',profile_text).group(1))

lines=["# Wirescape Windows baseline benchmark","",
       "## Scope and method","",
       "Measured on this Windows installation with Python 3.12 and the compiled Surge XT binding. All audio renders were offline; no device stream or audible playback was started. The 48 kHz / 512-frame block deadline is 10.67 ms. Each CHAOS case renders 937 blocks (9.995 seconds) three times for each of two factory presets after 20 warmup blocks. Creation/preset loading, warmup, synthesis and teardown are recorded separately in the CSV. Realtime ratio uses audio duration divided by the sum of measured `graph.process()` block times; whole-loop wall time is also recorded separately. Timings use `perf_counter_ns`; process CPU time and Windows working set are sampled. The script isolates plugin enablement state in a temporary directory. All benchmark scripts are under `tools/bench/`.","",
       "## Baseline tests and environment","",
       "Direct `unittest discover -s tests -q`: 71 tests passed, with one aiohttp deprecation warning. `tools/verify.py --real`: unittest, compile, JavaScript syntax, mock benchmark and real Surge benchmark passed. Its optional browser UI checks were skipped because `--ui` was not requested. The verification script labels its internal report version 0.5.0, while the server reports 0.5.2; see `baseline_tests.json`.","",
       f"System: {env['windows_cim']['os']['Caption']} build {env['windows_cim']['os']['BuildNumber']}; {env['windows_cim']['cpu']['Name']} ({env['windows_cim']['cpu']['NumberOfCores']} physical / {env['windows_cim']['cpu']['NumberOfLogicalProcessors']} logical cores); {env['windows_cim']['computer']['TotalPhysicalMemory']/2**30:.1f} GiB RAM. Python {env['python'].split()[0]}, NumPy {env['numpy']}, SciPy {env['scipy']}, sounddevice {env['sounddevice']}, PortAudio {env.get('portaudio_version')}. Surge {env['surgepy']['version']} from a compiled `.pyd`. Full device and host API inventory is in `environment.json`.","",
       "## CHAOS performance","",
       "Case mapping: A01=0 instances/0 notes, A02=1/0, A03=1/1, A04=1/4, A05=1/8, A06=2/4, A07=3/4, A08=3/8. Notes are per instance. A01 and A02 are intentionally silent. Factory presets are `1_clean_to_scream` and `3_cluster_mass`. These legacy factory presets are read through `PresetStore` as CHAOS macro snapshots; the complex preset can expand held notes through its CLUSTER setting.","",
       "| Preset | Case | Instances | Notes each | Median block ms | p99 block ms | p99/deadline | Realtime ratio | Peak output | Status |","|---|---:|---:|---:|---:|---:|---:|---:|---:|---|"]
for preset in ('1_clean_to_scream','3_cluster_mass'):
    for case in ('A01','A02','A03','A04','A05','A06','A07','A08'):
        rows=[r for r in chaos if r['preset']==preset and r['case']==case]
        if len(rows)!=3 or any(r['status']!='ok' for r in rows):
            lines.append(f"| {preset} | {case} | — | — | — | — | — | — | — | Incomplete: {len(rows)}/3 |")
            continue
        r=rows[0]
        lines.append(f"| {preset} | {case} | {r['instances']} | {r['notes_per_instance']} | {fmt(med(rows,'median_block_ms'))} | {fmt(med(rows,'p99_block_ms'))} | {fmt(med(rows,'p99_deadline_utilization'))} | {fmt(med(rows,'realtime_ratio'))} | {fmt(med(rows,'peak_output'),3)} | pass |")
lines += ["","A realtime ratio below 1 means the offline render took longer than its audio duration. p99/deadline above 1 means the measured p99 block exceeded the nominal 10.67 ms budget. Runs vary with Windows scheduling, load and temperature. Neither value proves live stability or glitch behavior. The CSV also contains means, p95, maxima, total processing duration, CPU time, working set, RMS, and finite/nonzero checks.",""]

active=[r for r in chaos if r['status']=='ok' and int(r['notes_per_instance'])>0]
failed=[r for r in chaos if r['status']!='ok']
bad=[r for r in active if r.get('all_finite')!='True' or r.get('expected_nonzero')!='True']
lines += [f"Validation: {len(active)} active-note renders completed; {len(bad)} had non-finite or unexpectedly silent output; {len(failed)} benchmark runs failed.","",
          "## Graph cost and profiling","",
          "Graph isolation uses lightweight `FakeEngine` only for graph-scalability cases; it is explicitly separate from the real CHAOS results. It tests serial, parallel/mixed-input and modulation patches at 1, 5, 10, 20 and 24 modules within the 24-module/128-cable limits. `tracemalloc` is active during these block timings, so compare these rows with each other, not directly with untraced CHAOS timings. The CSV reports validation, graph construction, per-block time, traced peak allocation and parameter enqueue/drain cost.",""]
for shape in ('serial','parallel','modulation'):
    rows=[r for r in graph if r['shape']==shape and r['modules'] in ('5','24')]
    if len(rows)==2:
        a,b=sorted(rows,key=lambda r:int(r['modules']))
        lines.append(f"- {shape}: 5 modules {float(a['mean_block_ms']):.3f} ms mean; 24 modules {float(b['mean_block_ms']):.3f} ms mean (under allocation tracing).")
lines += ["","`profiling_summary.txt` has a 300-block cProfile listing and a separate boundary-timed breakdown for Surge A, Surge B, remaining CHAOS processing and graph overhead. Native time is the duration of Python-facing `processMultiBlock()` calls; it is not a C++ internal profile. Source inspection confirms that routing dictionaries and many audio arrays are recreated each block.","",
          f"In the isolated one-CHAOS, one-note profiling run, Surge A took {profile['surge_a_call_ms']:.1f} ms and B {profile['surge_b_call_ms']:.1f} ms across 300 blocks. Together their Python-facing calls were {profile['native_call_share_percent']:.1f}% of measured graph wall time. Remaining CHAOS Python/NumPy work was {profile['chaos_python_and_numpy_ms']:.1f} ms ({profile['chaos_python_and_numpy_ms']/profile['graph_total_ms']*100:.1f}%), and graph work outside `Engine.render()` was {profile['graph_excluding_engine_ms']:.1f} ms ({profile['graph_excluding_engine_ms']/profile['graph_total_ms']*100:.1f}%). These shares apply only to this profiling configuration; cProfile itself perturbs timing.","",
          "## Memory","",
          "Windows process working set sampled after graph warmup and garbage collection. This includes Python and native Surge allocations; Python `tracemalloc` alone would miss native memory.","",
          "| Stage | Working set MiB |","|---|---:|"]
for r in memory:lines.append(f"| {r['stage']} | {float(r['working_set_mb']):.1f} |")
lines += ["","The stage samples show released memory after removal, but one cleanup point cannot prove absence of leaks. The source allocates a 5-second stereo float32 looper buffer and 4-second mono analysis ring per CHAOS instance; Surge objects and other DSP state account for the remaining process growth. Repeated create/remove cycles are still needed to assess stability.","",
          "## Realtime and architecture","",
          "`realtime_audio_audit.md` documents callback allocations, queue draining, locks, device errors and plugin failure behavior. `module_architecture_audit.md` documents the host-specific CHAOS/modulator/interface branches, static port identities, audio-block-only plugin contract and missing lifecycle/domain abstractions.","",
          "## Recommended upgrades","",
          "1. Precompile patch routing and input lookups at patch-load time.",
          "2. Reuse audio buffers and reduce repeated NumPy copies and scans.",
          "3. Bound callback command work and define plugin preparation/cleanup outside the callback.",
          "4. Unify module registration, stable port IDs and separate audio/event/control schedules.",
          "5. Reprofile after those changes before considering selective native acceleration.","",
          "`optimization_recommendations.md` compares Python/NumPy, graph compilation, selective native code, native executor replacement and PyPy by benefit, complexity, compatibility and incremental path.","",
          "## Unknowns and skipped tests","",
          "No PortAudio stream, live input/output, audible XRUN measurement, device disconnect or physical-controller test was run because the task prohibits automatic playback and settings changes. No Raspberry Pi performance is claimed. Optional browser UI smoke tests were skipped by the requested `--real` verification command; they are not part of the performance baseline. No PyPy/Numba/Cython/Rust installation or benchmark was performed. Precise native Surge internal function timing is unavailable through cProfile. The offline graph test uses mock processors to isolate host overhead, not to claim real synthesis throughput.",""]

(OUT/'BENCHMARK_REPORT.md').write_text('\n'.join(lines),encoding='utf-8')
print('Wrote',OUT/'BENCHMARK_REPORT.md')
