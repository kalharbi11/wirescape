"""Automated verification + machine-readable results.

python tools/verify.py           - mocks, type checks, UI JS syntax (if Node installed)
python tools/verify.py --real    - also render test chords with actual compiled Surge
Results -> build_reports/verify.json
"""
import argparse
import json
import os
import pathlib
import shutil
import statistics
import subprocess
import sys
import time
import traceback

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))


def command(argv,timeout=120):
    try:
        p=subprocess.run(argv,cwd=ROOT,text=True,capture_output=True,timeout=timeout)
        return {'pass':p.returncode==0,'returncode':p.returncode,
                'stdout':p.stdout[-5000:],'stderr':p.stderr[-5000:]}
    except Exception as exc:return {'pass':False,'error':str(exc)}


def benchmark_fake():
    from wirescape.graph import AudioGraph
    from tests_sample import FakeEngine
    graph=AudioGraph(engine_factory=FakeEngine)
    from pathlib import Path
    patch=json.loads((ROOT/'examples'/'01_chaos_mod_io.wscape.json').read_text())
    graph.load_patch(patch)
    graph.keyboard('node-2',60,True)
    values=[]
    for i in range(300):
        t=time.perf_counter()
        graph.process(512)
        if i>=20:values.append((time.perf_counter()-t)*1000)
    values.sort()
    result={'pass':len(values)==280,'blocks':len(values),'sample_rate':48000,'block_size':512,
            'budget_ms':round(512/48000*1000,3),'median_ms':round(statistics.median(values),3),
            'p95_ms':round(values[int(len(values)*.95)],3),'engine':'FAKE (no Surge)'}
    return result


def benchmark_real():
    from wirescape.server import create_surge_factory
    from wirescape.graph import AudioGraph,SilentSynth
    engine_factory,error=create_surge_factory()
    if error:return {'pass':False,'error':error}
    graph=AudioGraph(engine_factory=engine_factory)
    patch=json.loads((ROOT/'examples'/'01_chaos_mod_io.wscape.json').read_text())
    graph.load_patch(patch)
    for n in [48,52,55,60]:graph.keyboard('node-2',n,True)
    import numpy as np
    rms=[];times=[]
    for i in range(120):
        t=time.perf_counter()
        audio=graph.process(512)
        if i>=10:
            times.append((time.perf_counter()-t)*1000)
            rms.append(float(np.sqrt(np.mean(audio*audio))))
    times.sort()
    return {'pass':bool(rms and max(rms)>1e-4 and np.isfinite(rms).all()),
            'sample_rate':48000,'block_size':512,'budget_ms':round(512/48000*1000,3),
            'median_ms':round(statistics.median(times),3), 'p95_ms':round(times[int(len(times)*.95)],3),
            'realtime_headroom':round((512/48000*1000)/max(times[int(len(times)*.95)],.001),2),
            'peak_rms':round(max(rms),4),'engine':'REAL SURGE / 4-NOTE TEST'}


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--real',action='store_true',help='actual surgepy polyphonic render')
    ap.add_argument('--ui',action='store_true',help='optional Chromium UI smoke test; needs Playwright')
    args=ap.parse_args()
    report={'wirescape_version':'0.5.0','python':sys.version.split()[0],
            'host_os':sys.platform,'performed_at_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())}
    tests=command([sys.executable,'-m','unittest','discover','-s','tests','-q'])
    report['unittest']=tests
    report['compile']=command([sys.executable,'-m','compileall','-q','wirescape','modules/chaos','modules/sequencer'])
    if shutil.which('node'):
        report['javascript']=command(['node','--check','web/client.js'])
        report['plugin_javascript']=command(['node','--check','modules/sequencer/editor.js'])
        # Check the embedded original Studio template's JS as well.
        import re, tempfile
        inline=re.findall(r'<script(?:\s[^>]*)?>(.*?)</script>',(ROOT/'web'/'index.html').read_text(),re.S)
        with tempfile.NamedTemporaryFile(mode='w',suffix='.js',encoding='utf8',delete=False) as f:
            f.write(inline[-1]);tempname=f.name
        report['inline_javascript']=command(['node','--check',tempname])
        try:os.unlink(tempname)
        except OSError:pass
    try:
        from types import SimpleNamespace
        # Inline mock lives separately so this file is self-contained when moved.
        report['mock_performance']=benchmark_fake()
    except Exception as exc:
        report['mock_performance']={'pass':False,'error':str(exc),'traceback':traceback.format_exc()[-1200:]}
    if args.ui:
        argv=[sys.executable,'tools/ui_smoke.py']
        browser=shutil.which('chromium') or shutil.which('chromium-browser')
        if browser:argv.extend(['--chromium',browser])
        report['browser_ui']=command(argv,timeout=80)
        report['plugin_browser_ui']=command([sys.executable,'tools/ui_module_smoke.py'],timeout=80)
    else:
        report['browser_ui']={'pass':None,'status':'NOT CHECKED (optional: --ui with Playwright/Chromium)'}
    if args.real:
        try:report['surge_performance']=benchmark_real()
        except Exception as exc:report['surge_performance']={'pass':False,'error':str(exc),'traceback':traceback.format_exc()[-1200:]}
    else:report['surge_performance']={'pass':None,'status':'NOT CHECKED (run --real on your built Windows/Pi machine)'}
    directory=ROOT/'build_reports';directory.mkdir(exist_ok=True)
    target=directory/'verify.json'
    target.write_text(json.dumps(report,indent=2))
    for k,v in report.items():
        if isinstance(v,dict) and 'pass' in v:print(f"{k:20} {'PASS' if v['pass'] else 'FAIL' if v['pass'] is False else 'SKIP'}",str(v.get('median_ms',''))+' ms' if v.get('median_ms') else '')
    print('Machine-readable results:',target)
    failed=[k for k,v in report.items() if isinstance(v,dict) and v.get('pass') is False]
    return int(bool(failed))

if __name__=='__main__':sys.exit(main())
