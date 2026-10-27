"""Local folder-plugin discovery with explicit user trust and API-version guard.

A plugin is arbitrary Python code. Discovery reads JSON only; execution occurs
only for modules previously enabled by the user, or after an explicit ENABLE
request. Updating loaded Python code needs a host restart. No ZIP extraction,
file uploads, or remote code downloads are performed by this service.
"""
from __future__ import annotations
import importlib
import json
import re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
MODULES=ROOT/'modules'
ENABLED=ROOT/'user_data'/'enabled_modules.json'
IDENT=re.compile(r'^[a-z][a-z0-9_]{1,48}$')
ACTIVE={}
FAILED={}
# Only the sequencer shipped in this distribution is pre-approved on first launch.
# Third-party folders remain inert until individually enabled by the user.
BUNDLED_DEFAULTS=frozenset({'sequencer'})

def scan():
    found={}
    for folder in sorted(MODULES.iterdir()):
        if not folder.is_dir() or folder.is_symlink() or not IDENT.fullmatch(folder.name):continue
        source=folder/'module.json'
        if not source.is_file() or source.is_symlink() or source.stat().st_size>16384:continue
        try:
            data=json.loads(source.read_text(encoding='utf8'))
            if data['id']!=folder.name or data.get('api_version')!=1:raise ValueError('Unsupported module id / API version')
            if not isinstance(data.get('name'),str) or len(data['name'])>100:raise ValueError('Invalid module name')
            if not isinstance(data.get('version'),str) or len(data['version'])>60:raise ValueError('Invalid module version')
            entry=data.get('entry','plugin:register')
            if entry!='plugin:register':raise ValueError('Only plugin:register is accepted')
            assets=data.get('assets',[])
            if not isinstance(assets,list) or any(not isinstance(n,str) or not re.fullmatch(r'[a-zA-Z0-9_.-]{1,90}\.(?:js|css)',n) or not (folder/n).is_file() for n in assets):
                raise ValueError('Invalid declared module UI assets')
            found[folder.name]=dict(data)
        except (ValueError,TypeError,KeyError,json.JSONDecodeError) as ex:
            FAILED[folder.name]=f'Invalid module manifest: {ex}'
    return found

def read_enabled():
    if not ENABLED.exists():
        return {ident for ident in BUNDLED_DEFAULTS if (MODULES/ident/'module.json').is_file()}
    try:
        items=json.loads(ENABLED.read_text(encoding='utf8'))
        if isinstance(items,list):return set(x for x in items if isinstance(x,str) and IDENT.fullmatch(x))
    except (OSError,ValueError):pass
    return set()

def write_enabled(ids):
    ENABLED.parent.mkdir(parents=True,exist_ok=True)
    ENABLED.write_text(json.dumps(sorted(ids),indent=2)+'\n',encoding='utf8')

def enable(identifier):
    found=scan()
    if identifier not in found:raise ValueError('No valid module folder: '+str(identifier))
    if identifier not in ACTIVE:
        module=importlib.import_module('modules.'+identifier+'.plugin')
        func=getattr(module,'register',None)
        if not callable(func):raise RuntimeError('plugin.py must export register()')
        func()
        from .schema import DEFINITIONS
        from .plugins import AUDIO_PROCESSORS
        if identifier not in DEFINITIONS or identifier not in AUDIO_PROCESSORS:
            raise RuntimeError('register() must register a matching audio module')
        ACTIVE[identifier]=dict(found[identifier])
    enabled=read_enabled();enabled.add(identifier);write_enabled(enabled)
    FAILED.pop(identifier,None)
    return found[identifier]

def load_enabled():
    for identifier in sorted(read_enabled()):
        if identifier in ACTIVE:continue
        try:enable(identifier)
        except Exception as ex:FAILED[identifier]=f'{type(ex).__name__}: {ex}'

def catalog():
    discovered=scan()
    return [{'id':key,'name':data['name'],'version':data['version'],
             'description':data.get('description',''), 'active':key in ACTIVE,
             'enabled':key in read_enabled(), 'restart_required':key in ACTIVE and data['version']!=ACTIVE[key]['version'],
             'error':FAILED.get(key,'')} for key,data in discovered.items()]

def client_assets():
    return {key:{'version':data['version'],'assets':data.get('assets',[])} for key,data in ACTIVE.items()}

def asset(identifier,filename):
    data=ACTIVE.get(identifier)
    if not data or filename not in data.get('assets',[]):raise FileNotFoundError('Module asset not enabled')
    path=MODULES/identifier/filename
    if not path.is_file() or path.is_symlink():raise FileNotFoundError(filename)
    return path
