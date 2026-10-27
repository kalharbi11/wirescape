"""Optional one-time enum label scanner for the user's built Surge XT binding.

Run with Wirescape stopped to avoid CPU interference:
    py -3.12 tools/scan_surge_choices.py

Queries the *actual local Surge version* for readable labels for discrete
oscillator/filter/LFO/FX type selectors. Writes user_data/surge_enum_labels.json.
No source rebuild, no mutation of shipped synth patches or binary.
"""
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from wirescape.server import create_surge_factory
from wirescape.preset_store import PresetError


def main():
    factory,error=create_surge_factory()
    if error:
        print('SURGE NOT AVAILABLE:',error)
        return 1
    import surgepy
    s=surgepy.createSurge(48000)
    lookup={}
    for gid in range(32):
        try:entries=s.getControlGroup(gid).getEntries()
        except Exception:continue
        for entry in entries:
            for param in entry.getParams():lookup[(entry.getScene(),param.getName())]=param
    records=json.loads((ROOT/'modules'/'chaos'/'params.json').read_text(encoding='utf8'))
    output={};count=0
    for item in records:
        key=f"S{item['scene']}:{item['name']}"
        if item['type']!='int' or item['max']-item['min']>64 or item['min']<-100:
            continue
        if not re.search(r'\b(type|subtype|configuration|mode|routing|shape|behavior)\b',item['name'],re.I):
            continue
        ref=lookup.get((item['scene'],item['name']))
        if ref is None:continue
        labels={}
        try:old=s.getParamVal(ref)
        except Exception:continue
        for n in range(int(item['min']),int(item['max'])+1):
            try:
                s.setParamVal(ref,float(n))
                text=str(s.getParamDisplay(ref)).strip()
                if text and text!='-':labels[str(n)]=text
            except Exception:continue
        try:s.setParamVal(ref,old)
        except Exception:pass
        if labels:
            output[key]=labels
            if item['scene']==1:output[item['name']]=labels
            count+=1
    dst=ROOT/'user_data'/'surge_enum_labels.json'
    dst.parent.mkdir(parents=True,exist_ok=True)
    dst.write_text(json.dumps(output,indent=2),encoding='utf8')
    print(f'Loaded {count} discrete type-selector label tables from local Surge XT -> {dst}')
    print('Restart Wirescape to populate the new dropdown labels.')
    return 0

if __name__=='__main__':sys.exit(main())
