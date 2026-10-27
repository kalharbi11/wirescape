"""CHAOS preset persistence, separate from patch persistence and audio callback.

Built-in macro presets are read-only. User presets are complete CHAOS snapshots
saved atomically under user_data/chaos_presets, never inside shipped modules.
The host runs on localhost; still validate all names and user-controlled values.
"""
import json
import math
from pathlib import Path
import re
import unicodedata
from .schema import DEFINITIONS, normalize_control

class PresetError(ValueError):
    pass

class PresetStore:
    def __init__(self, root):
        self.root=Path(root)
        self.builtin=self.root/'modules'/'chaos'/'presets'
        self.user=self.root/'user_data'/'chaos_presets'

    def list(self):
        found=[]
        for file in sorted(self.builtin.glob('*.json')):
            found.append({'id':'factory/'+file.stem,'name':file.stem.replace('_',' ').title(),'kind':'factory'})
        if self.user.is_dir():
            for file in sorted(self.user.glob('*.json')):
                try:
                    obj=json.loads(file.read_text(encoding='utf8'))
                    name=obj.get('name') or file.stem
                    if isinstance(name,str) and isinstance(obj.get('values'),dict):
                        found.append({'id':'user/'+file.stem,'name':name,'kind':'user'})
                except (ValueError,OSError):continue
        return found

    @staticmethod
    def _snapshot(obj):
        if not isinstance(obj,dict):raise PresetError('Preset must be an object')
        values=obj.get('values',{})
        advanced=obj.get('advanced',{})
        raw=obj.get('raw',{})
        mode=obj.get('mode',0)
        if not all(isinstance(x,dict) for x in (values,advanced,raw)):
            raise PresetError('Invalid preset parameter collection')
        if len(values)>128 or len(advanced)>128 or len(raw)>1600:
            raise PresetError('Preset contains too many parameters')
        import modules.chaos.macros as M
        import modules.chaos.voices as V
        from .graph import full_raw_keys
        cleaned={}
        for name,v in values.items():
            if name not in DEFINITIONS['chaos']['controls']:raise PresetError('Unknown front parameter')
            try:cleaned[name]=normalize_control('chaos',name,v)
            except (ValueError,TypeError) as exc:raise PresetError(str(exc)) from exc
        macros={}
        for name,v in advanced.items():
            if name not in M.SPEC:raise PresetError('Unknown macro '+str(name))
            try:val=float(v)
            except (ValueError,TypeError):raise PresetError('Invalid macro')
            if not math.isfinite(val):raise PresetError('Nonfinite macro')
            macros[name]=M.clamp(name,val)
        allowed={k for _,keys in V.RAW_GROUPS for k in keys}|set(full_raw_keys())
        raw_clean={}
        for label,v in raw.items():
            if not isinstance(label,str) or '|' not in label:raise PresetError('Invalid raw address')
            voice,key=label.split('|',1)
            if voice not in ('A','B') or key not in allowed:raise PresetError('Invalid raw address')
            try:val=float(v)
            except (TypeError,ValueError):raise PresetError('Invalid raw value')
            if not math.isfinite(val):raise PresetError('Nonfinite raw value')
            raw_clean[label]=min(1.0,max(0.0,val))
        try:mode=int(mode)
        except (ValueError,TypeError):raise PresetError('Invalid mode')
        if mode not in (0,1,2):raise PresetError('Invalid mode')
        return {'values':cleaned,'advanced':macros,'raw':raw_clean,'mode':mode}

    def save(self,name,state):
        if not isinstance(name,str):raise PresetError('Preset name required')
        name=name.strip()
        if not 1<=len(name)<=64 or any(ord(c)<32 for c in name):
            raise PresetError('Use a preset name between 1 and 64 printable characters')
        slug=unicodedata.normalize('NFKD',name).encode('ascii','ignore').decode().lower()
        slug=re.sub(r'[^a-z0-9]+','_',slug).strip('_')[:48]
        if not slug:raise PresetError('Preset name needs at least one letter or number')
        clean=self._snapshot(state)
        self.user.mkdir(parents=True,exist_ok=True)
        candidate=slug
        index=2
        while (self.user/(candidate+'.json')).exists():
            candidate=f'{slug[:40]}_{index}'
            index+=1
            if index>1000:raise PresetError('Too many presets with this name')
        data={'format':'wirescape.chaos.preset','version':1,'name':name,**clean}
        tmp=self.user/(candidate+'.tmp')
        final=self.user/(candidate+'.json')
        tmp.write_text(json.dumps(data,indent=2,ensure_ascii=False),encoding='utf8')
        tmp.replace(final)
        return {'id':'user/'+candidate,'name':name,'kind':'user'}

    def load(self,preset_id):
        if not isinstance(preset_id,str) or not re.fullmatch(r'(factory|user)/[a-zA-Z0-9_\-]{1,90}',preset_id):
            raise PresetError('Invalid preset id')
        kind,stem=preset_id.split('/',1)
        file=(self.builtin if kind=='factory' else self.user)/(stem+'.json')
        if not file.is_file():raise PresetError('Preset not found')
        if file.stat().st_size>512_000:raise PresetError('Preset too large')
        obj=json.loads(file.read_text(encoding='utf8'))
        if kind=='factory' and obj.get('format')!='wirescape.chaos.preset':
            # Legacy CHAOS factory presets are macro-only, retained unchanged.
            payload={'values':dict(DEFINITIONS['chaos']['controls']),
                     'advanced':obj.get('macros',{}),'raw':{},'mode':0}
        else:payload=obj
        return self._snapshot(payload)
