"""Portable offline Wirescape baseline. Never opens an audio stream or writes user data."""
from __future__ import annotations

import argparse
import cProfile
import csv
import gc
import json
import os
import pathlib
import platform
import pstats
import statistics
import sys
import tempfile
import time
import tracemalloc

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "build_reports" / "bench"
sys.path.insert(0, str(ROOT))

import numpy as np
from wirescape import extensions
from wirescape.graph import AudioGraph
from wirescape.schema import validate_patch, MAX_MODULES, MAX_CABLES
from wirescape.server import create_surge_factory
from wirescape.preset_store import PresetStore


def rss_bytes():
    if sys.platform == "win32":
        import ctypes
        from ctypes import wintypes
        class Counters(ctypes.Structure):
            _fields_ = [("cb", wintypes.DWORD), ("PageFaultCount", wintypes.DWORD),
                        ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                        ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                        ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                        ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t)]
        p = Counters(); p.cb = ctypes.sizeof(p)
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        psapi = ctypes.WinDLL("psapi", use_last_error=True)
        psapi.GetProcessMemoryInfo.argtypes = [wintypes.HANDLE, ctypes.POINTER(Counters), wintypes.DWORD]
        psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
        if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(p), p.cb):
            raise OSError(ctypes.get_last_error())
        return p.WorkingSetSize
    import resource
    value = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return value * (1024 if sys.platform != "darwin" else 1)  # peak, not current, on Unix


def write_csv(name, rows):
    if not rows:
        return
    with (OUT / name).open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(dict.fromkeys(k for row in rows for k in row)))
        w.writeheader(); w.writerows(rows)


def patch_for_chaos(count, preset):
    modules = [{"id": f"c{i}", "type": "chaos", "advanced": preset["advanced"],
                "values": preset["values"], "raw": preset["raw"], "mode": preset["mode"]}
               for i in range(count)]
    modules.append({"id": "out", "type": "interface"})
    cables = [{"from": f"c{i}:{side}", "to": f"out:in{channel}"}
              for i in range(count) for side, channel in (("left", "L"), ("right", "R"))]
    return {"modules": modules, "connections": cables}


def percentile(values, q):
    ordered = sorted(values)
    return ordered[min(len(ordered)-1, int((len(ordered)-1)*q))]


def chaos_run(factory, case, preset_id, notes, repeat):
    cases={"A01":0,"A02":1,"A03":1,"A04":1,"A05":1,"A06":2,"A07":3,"A08":3}
    instances=cases[case]
    row = dict(protocol="correct_A01_A08_v2",case=case, preset=preset_id, instances=instances, notes_per_instance=notes,
               repetition=repeat, status="ok", error="")
    graph = None
    try:
        preset = PresetStore(ROOT).load("factory/" + preset_id)
        start = time.perf_counter_ns()
        graph = AudioGraph(engine_factory=factory)
        graph.load_patch(patch_for_chaos(instances, preset))
        row["creation_preset_load_ms"] = (time.perf_counter_ns()-start)/1e6
        start = time.perf_counter_ns()
        for _ in range(20): graph.process(512)
        row["warmup_ms"] = (time.perf_counter_ns()-start)/1e6
        for i in range(instances):
            for note in (48, 52, 55, 60, 64, 67, 71, 74)[:notes]:
                graph.keyboard(f"c{i}", note, True)
        timings = []; peaks = []; squares = []; finite = True
        count = 0
        cpu0 = time.process_time_ns(); wall0 = time.perf_counter_ns()
        for _ in range(int(10*48000/512)):
            tick = time.perf_counter_ns()
            block = graph.process(512)
            timings.append((time.perf_counter_ns()-tick)/1e6)
            finite &= bool(np.isfinite(block).all())
            peaks.append(float(np.max(np.abs(block))))
            squares.append(float(np.sum(np.square(block, dtype=np.float64))))
            count += block.size
        wall_ns = time.perf_counter_ns()-wall0
        cpu_ns = time.process_time_ns()-cpu0
        duration = len(timings)*512/48000
        p99 = percentile(timings, .99)
        row.update(blocks=len(timings), audio_seconds=duration,
                   mean_block_ms=statistics.mean(timings), median_block_ms=statistics.median(timings),
                   p95_block_ms=percentile(timings,.95), p99_block_ms=p99,
                   max_block_ms=max(timings), total_processing_ms=sum(timings),
                   wall_processing_ms=wall_ns/1e6, realtime_ratio=duration/(sum(timings)/1000),
                   p99_deadline_utilization=p99/(512/48000*1000),
                   process_cpu_seconds=cpu_ns/1e9, process_cpu_wall_percent=cpu_ns/wall_ns*100,
                   process_rss_mb=rss_bytes()/2**20, peak_output=max(peaks),
                   rms_output=(sum(squares)/count)**.5, all_finite=finite,
                   expected_nonzero=max(peaks)>1e-4)
        start = time.perf_counter_ns()
        graph.panic(); graph.process(512)
        graph.load_patch({"modules": [], "connections": []}); graph.process(512)
        del graph; graph = None; gc.collect()
        row["teardown_ms"] = (time.perf_counter_ns()-start)/1e6
    except Exception as exc:
        row.update(status="failed", error=f"{type(exc).__name__}: {exc}")
    finally:
        if graph is not None:
            del graph; gc.collect()
    return row


def graph_patch(size, shape):
    if shape == "modulation":
        count = size-2
        modules = [{"id": f"m{i:02}", "type": "modulator"} for i in range(count)]
        modules += [{"id": "s", "type": "chaos"}, {"id": "out", "type": "interface"}]
        targets = ("pitch", "morph", "chaos", "cutoff")
        cables = [{"from": f"m{i:02}:modA", "to": f"s:{targets[i%4]}"} for i in range(count)]
        cables += [{"from": "s:left", "to": "out:inL"}, {"from": "s:right", "to": "out:inR"}]
    else:
        modules = [{"id": f"g{i:02}", "type": "gain"} for i in range(size-1)]
        modules.append({"id": "out", "type": "interface"})
        cables = []
        if shape == "serial":
            for i in range(size-2):
                for a,b in (("outL","inL"),("outR","inR")):
                    cables.append({"from": f"g{i:02}:{a}", "to": f"g{i+1:02}:{b}"})
            last = [size-2]
        else:
            last = list(range(size-1))
        for i in last:
            for a,b in (("outL","inL"),("outR","inR")):
                cables.append({"from": f"g{i:02}:{a}", "to": f"out:{b}"})
    return {"modules": modules, "connections": cables}


def graph_bench():
    from tests_sample import FakeEngine
    rows = []
    for size in (1,5,10,20,MAX_MODULES):
        for shape in ("serial", "parallel", "modulation"):
            if size == 1 and shape == "modulation": continue
            patch = {"modules": [{"id":"out","type":"interface"}],"connections":[]} if size == 1 else graph_patch(size,shape)
            if len(patch["connections"]) > MAX_CABLES: continue
            row = dict(modules=size, shape=shape, cables=len(patch["connections"]),
                       engine="FakeEngine (graph isolation)", status="ok", error="")
            try:
                start=time.perf_counter_ns(); validate_patch(patch)
                row["validation_ms"]=(time.perf_counter_ns()-start)/1e6
                graph=AudioGraph(engine_factory=FakeEngine)
                start=time.perf_counter_ns(); graph.load_patch(patch)
                row["construction_ms"]=(time.perf_counter_ns()-start)/1e6
                for _ in range(20):graph.process(512)
                times=[]; tracemalloc.start()
                for _ in range(200):
                    start=time.perf_counter_ns(); graph.process(512)
                    times.append((time.perf_counter_ns()-start)/1e6)
                _,peak=tracemalloc.get_traced_memory(); tracemalloc.stop()
                row.update(mean_block_ms=statistics.mean(times),median_block_ms=statistics.median(times),
                           p95_block_ms=percentile(times,.95), traced_peak_bytes=peak)
                node=next((n for n in patch["modules"] if n["type"] in ("gain","modulator","chaos")),None)
                if node:
                    key={"gain":"GAIN","modulator":"RATE","chaos":"CUTOFF"}[node["type"]]
                    start=time.perf_counter_ns()
                    for _ in range(200):graph.control(node["id"],key,50)
                    row["parameter_enqueue_us"]=(time.perf_counter_ns()-start)/200/1e3
                    start=time.perf_counter_ns();graph.process(512)
                    row["parameter_drain_ms"]=(time.perf_counter_ns()-start)/1e6
            except Exception as exc:
                row.update(status="failed",error=f"{type(exc).__name__}: {exc}")
            rows.append(row)
    write_csv("graph_performance.csv",rows)
    return rows


def memory_bench(factory):
    rows=[]; graph=AudioGraph(engine_factory=factory)
    for count,playing in ((0,False),(1,False),(1,True),(2,True),(3,True),(0,False)):
        label=("cleanup" if rows and count==0 else f"{count}_chaos_{'playing' if playing else 'idle'}")
        start=time.perf_counter_ns()
        graph.load_patch(patch_for_chaos(count,{"advanced":{},"values":{},"raw":{},"mode":0}))
        for _ in range(4):graph.process(512)
        if playing:
            for i in range(count):graph.keyboard(f"c{i}",60,True)
            for _ in range(20):graph.process(512)
        gc.collect()
        rows.append(dict(stage=label,instances=count,working_set_mb=rss_bytes()/2**20,
                         elapsed_ms=(time.perf_counter_ns()-start)/1e6,
                         looper_buffer_mb=count*2*5*48000*4/2**20,
                         analysis_ring_mb=count*4*48000*4/2**20))
    write_csv("memory.csv",rows)
    return rows


def profile(factory):
    from tests_sample import FakeEngine
    lines=[]
    for label,engine in (("real_surge",factory),("fake_graph",FakeEngine)):
        g=AudioGraph(engine_factory=engine)
        g.load_patch(patch_for_chaos(1,{"advanced":{},"values":{},"raw":{},"mode":0}))
        g.keyboard("c0",60,True)
        for _ in range(20):g.process(512)
        prof=cProfile.Profile();prof.enable()
        for _ in range(300):g.process(512)
        prof.disable()
        import io
        buf=io.StringIO()
        pstats.Stats(prof,stream=buf).strip_dirs().sort_stats("cumtime").print_stats(45)
        lines.append(label+" (300 blocks, profiler overhead included)\n"+buf.getvalue())
    # Wall-clock timers around the Python-facing native calls include C++ time.
    class TimedSurge:
        def __init__(self, target): self.target=target; self.ns=0
        def __getattr__(self,name): return getattr(self.target,name)
        def processMultiBlock(self, block):
            start=time.perf_counter_ns()
            try:return self.target.processMultiBlock(block)
            finally:self.ns+=time.perf_counter_ns()-start
    g=AudioGraph(engine_factory=factory)
    g.load_patch(patch_for_chaos(1,{"advanced":{},"values":{},"raw":{},"mode":0}))
    g.keyboard("c0",60,True)
    for _ in range(20):g.process(512)
    engine=g.nodes["c0"].synth
    original_a,original_b=engine.s,engine.sB
    a,b=TimedSurge(original_a),TimedSurge(original_b)
    engine.s,engine.sB=a,b
    graph_ns=engine_ns=0
    original_render=engine.render
    def timed_render(*args,**kwargs):
        nonlocal engine_ns
        start=time.perf_counter_ns()
        try:return original_render(*args,**kwargs)
        finally:engine_ns+=time.perf_counter_ns()-start
    engine.render=timed_render
    for _ in range(300):
        start=time.perf_counter_ns();g.process(512);graph_ns+=time.perf_counter_ns()-start
    engine.render=original_render;engine.s=original_a;engine.sB=original_b
    pieces={"graph_total_ms":graph_ns/1e6,"engine_render_ms":engine_ns/1e6,
            "surge_a_call_ms":a.ns/1e6,"surge_b_call_ms":b.ns/1e6,
            "chaos_python_and_numpy_ms":(engine_ns-a.ns-b.ns)/1e6,
            "graph_excluding_engine_ms":(graph_ns-engine_ns)/1e6,
            "native_call_share_percent":(a.ns+b.ns)/graph_ns*100,
            "wirescape_share_percent":(graph_ns-a.ns-b.ns)/graph_ns*100}
    lines.append("Wall-clock boundary timings, one CHAOS + I/O, 300 blocks; native-call time is measured at Python boundary, and wrapper overhead is included:\n"+json.dumps(pieces,indent=2))
    (OUT/"profiling_summary.txt").write_text("\n".join(lines),encoding="utf-8")


def environment():
    import importlib
    import subprocess
    import sounddevice as sd
    import scipy
    info={"platform":platform.platform(),"architecture":platform.machine(),"python":sys.version,
          "python_executable":sys.executable,"numpy":np.__version__,"scipy":scipy.__version__,
          "sounddevice":getattr(sd,"__version__",None),"sample_rate":48000,"block_frames":512,
          "max_modules":MAX_MODULES,"max_cables":MAX_CABLES,"device_query_only":True}
    try:
        import surgepy
        info["surgepy"]={"file":surgepy.__file__,"version":str(surgepy.getVersion())}
    except Exception as exc:info["surgepy_error"]=str(exc)
    try:info["portaudio_version"]=sd.get_portaudio_version()
    except Exception as exc:info["portaudio_error"]=str(exc)
    try:info["audio_devices"]=[dict(x) for x in sd.query_devices()];info["audio_host_apis"]=[dict(x) for x in sd.query_hostapis()];info["default_device"]=list(sd.default.device)
    except Exception as exc:info["audio_query_error"]=str(exc)
    if sys.platform=="win32":
        command=["powershell","-NoProfile","-Command", "@{computer=Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer,Model,TotalPhysicalMemory; cpu=Get-CimInstance Win32_Processor | Select-Object Manufacturer,Name,NumberOfCores,NumberOfLogicalProcessors; os=Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,FreePhysicalMemory} | ConvertTo-Json -Depth 4"]
        try:
            result=subprocess.run(command,capture_output=True,text=True,timeout=20)
            info["windows_cim"]=json.loads(result.stdout) if result.returncode==0 else {"error":result.stderr}
        except Exception as exc:info["windows_cim_error"]=str(exc)
    (OUT/"environment.json").write_text(json.dumps(info,indent=2,default=str),encoding="utf-8")
    return info


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("parts",nargs="*",choices=["environment","chaos","graph","memory","profile"])
    args=parser.parse_args()
    parts=args.parts or ["environment","chaos","graph","memory","profile"]
    OUT.mkdir(parents=True,exist_ok=True)
    # Keep plugin state writes out of the user's existing enabled_modules.json.
    with tempfile.TemporaryDirectory(prefix="wirescape_bench_") as tmp:
        extensions.ENABLED=pathlib.Path(tmp)/"enabled_modules.json"
        extensions.ENABLED.write_text("[]",encoding="utf-8")
        factory,error=create_surge_factory()
        if error:raise RuntimeError(error)
        import surgepy
        if not str(surgepy.__file__).lower().endswith((".pyd",".so")):
            raise RuntimeError("Surge binding is not a compiled extension")
        if "environment" in parts:print("environment",environment()["surgepy"])
        if "chaos" in parts:
            existing=OUT/"chaos_performance.csv"
            rows=list(csv.DictReader(existing.open(newline="",encoding="utf-8-sig"))) if existing.exists() else []
            if rows and rows[0].get("protocol")!="correct_A01_A08_v2":rows=[]
            completed={(r["preset"],r["case"],int(r["repetition"])) for r in rows if r["status"]=="ok"}
            for preset in ("1_clean_to_scream","3_cluster_mass"):
                for case,notes in (("A01",0),("A02",0),("A03",1),("A04",4),("A05",8),("A06",4),("A07",4),("A08",8)):
                    for repeat in range(1,4):
                        if (preset,case,repeat) in completed:continue
                        row=chaos_run(factory,case,preset,notes,repeat);rows.append(row)
                        write_csv("chaos_performance.csv",rows)
                        print("chaos",preset,case,repeat,row["status"],row.get("realtime_ratio"),flush=True)
        if "graph" in parts:print("graph rows",len(graph_bench()))
        if "memory" in parts:print("memory",memory_bench(factory))
        if "profile" in parts:profile(factory);print("profile complete")


if __name__=="__main__":main()
