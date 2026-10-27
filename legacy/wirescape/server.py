"""Local-only Wirescape host. The browser is an editor, not the audio callback."""
import argparse
import asyncio
import json
import os
import pathlib
import sys
import time
import webbrowser
from aiohttp import web, WSMsgType

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from .graph import AudioGraph, SilentSynth, SR, BLOCK
from .audio import AudioService
from .schema import DEFINITIONS, PatchError
from .preset_store import PresetStore, PresetError
from . import extensions


def create_surge_factory():
    chaos=str(ROOT/'modules'/'chaos')
    if chaos not in sys.path:sys.path.insert(0,chaos)
    # First search in a user-specified directory, then this archive's location.
    # A compiled surgepy binary is intentionally NOT shipped in Wirescape.
    if not os.environ.get('SURGEPY_DIR'):
        for candidate in [ROOT/'modules'/'chaos'/'surgepy_bin',ROOT.parent/'CHAOS'/'surgepy_bin',
                          ROOT.parent/'guttr'/'surgepy_bin']:
            if candidate.is_dir():
                os.environ['SURGEPY_DIR']=str(candidate)
                break
    try:
        import engine as chaos_engine
        return chaos_engine.Engine, ''
    except Exception as exc:
        # Import failure must be shown explicitly; never imply a fake synth exists.
        return SilentSynth, f'Surge XT unavailable: {type(exc).__name__}: {exc}'


def schema_payload():
    import modules.chaos.macros as M
    import modules.chaos.voices as V
    from .mapping import CHAOS_MOD_DEST
    from .plugins import STATE_DEFAULTS
    groups={
        'PERFORMANCE':['BREAK','DETUNE','CUTOFF','DRIFT','JITTER','ROUGH_RATE','ROUGH_DEPTH','FM_RATIO','SUB'],
        'PITCH / VOICING':['VOICES','CLUSTER','QUARTER','OFFSET','FALL'],
        'TIMBRE / FX':['DRIVE_TYPE','TAPE','SPACE','SPACE_MIX','LOOP_SPEED','LOOP_FEEDBACK','LOOP_MIX'],
        'GENERATOR BUSES':['A_LEVEL','A_PAN','A_MUTE','B_LEVEL','B_PAN','B_MUTE','MASTER_DB'],
        'ROUTING / BODY':['ROUTE_MODE','ROUTE_DEPTH','BODY_MIX','BODY_TONE','BODY_FEED','EXT_MIX'],
        'INTERNAL MODULATION':['LFO_RATE','LFO_DEPTH','LFO_SELF'] + [f'MOD{i}_{suffix}' for i in range(1,5) for suffix in ('SRC','DST','AMT')]
    }
    known={k for lst in groups.values() for k in lst}
    missing=set(M.SPEC)-known
    if missing:groups['OTHER']=sorted(missing)
    raw_json=json.loads((ROOT/'modules'/'chaos'/'params.json').read_text())
    full_groups={}
    for record in raw_json:
        scene=record['scene']
        group=record['group'].replace('cg_','')
        group_label=('GLOBAL' if scene==0 else 'SCENE '+('A' if scene==1 else 'B'))+' / '+group
        full_groups.setdefault(group_label,[]).append([f"S{scene}:{record['name']}",record['name']])
    presets=PresetStore(ROOT).list()
    # Snapshot metadata is indispensable for discrete enums/bools and correct
    # unedited defaults. The browser works in normalized 0..1 raw values.
    raw_metadata={}
    for record in raw_json:
        key=f"S{record['scene']}:{record['name']}"
        if key not in raw_metadata:
            raw_metadata[key]={k:record[k] for k in ('min','max','def','type','display')}
    for _,keys in V.RAW_GROUPS:
        for key in keys:
            info=raw_metadata.get('S1:'+key)
            if info:raw_metadata[key]=info
    enum_labels={}
    label_file=ROOT/'user_data'/'surge_enum_labels.json'
    if label_file.is_file():
        try:enum_labels=json.loads(label_file.read_text(encoding='utf8'))
        except (ValueError,OSError):pass
    return {'macros':{k:{'min':lo,'max':hi,'default':default,'integer':k in M.INT_MACROS}
                      for k,(lo,hi,default) in M.SPEC.items()},
            'groups':groups,'raw_groups':V.RAW_GROUPS,'full_raw_groups':full_groups,
            'raw_metadata':raw_metadata,'enum_labels':enum_labels,'modes':{t:v['modes'] for t,v in DEFINITIONS.items()},
            'presets':presets,
            'plugin_definitions':{key:{'name':spec.get('title',key.upper()),'desc':spec.get('desc',''),
              'kind':spec.get('kind','PROCESSOR'),'code':spec.get('code',key.upper()),
              'w':spec.get('w',480),'h':spec.get('h',400),
              'controls':[[k,v] for k,v in spec['controls'].items()],
              **({'default_state':STATE_DEFAULTS[key]()} if key in STATE_DEFAULTS else {}),
              'modes':spec['modes'],
              'ins':[[p.name,p.name.upper(),p.kind] for p in spec['ins']],
              'outs':[[p.name,p.name.upper(),p.kind] for p in spec['outs']]}
              for key,spec in DEFINITIONS.items() if key not in ('chaos','modulator','interface')},
            'module_assets':extensions.client_assets(),
            'installed_modules':extensions.catalog(),
            'front_controls':DEFINITIONS['chaos']['controls'],
            'front_descriptions':{
                'MORPH':'Equal-power blend between two complete Surge generators A and B.',
                'CHAOS':'Aggregate instability: jitter, rough AM and oscillator drift.',
                'FOLD':'Existing BREAK nonlinear waveshaper (also engages FM/ring/feedback).',
                'CUTOFF':'Surge filter 1 cutoff through the original CUTOFF macro.',
                'PITCH':'Fine offset of ±50 cents, independent of note selection.',
                'DETUNE':'Surge unison separation, up to ±60 cents.',
                'CLUSTER':'One to five simultaneous adjacent notes per held key.',
                'SUB':'Classic oscillator sub-harmonic oscillator mixture.',
                'FM':'Original FM_RATIO: inharmonic offset of oscillator 2 pitch.',
                'ROUGH':'30–150 Hz host roughness AM intensity; use ROUGH_RATE under MACROS.',
                'JITTER':'Nonlinear host pitch jitter and intermittent pitch jumps.',
                'DRIFT':'Surge native per-voice oscillator drift.',
                'FALL':'Gradual sinking pitch while notes are held.',
                'COUPLE':'Cross-bus ring/delay-bend intensity; mode selects processor topology.',
                'BODY':'Wet amount of the 3-comb resonator body.',
                'TONE':'Tone damping of the resonant comb body.',
                'FEEDBACK':'Feedback intensity inside the resonant comb body.',
                'RATE':'Speed of the internal self-modulating LFO.',
                'MOTION':'LFO depth; automatically modulates CUTOFF (full routing under MACROS).',
                'TAPE':'Surge tape/wow/flutter aging effects.',
                'SPACE':'Surge reverb size and wet amount.',
                'LOOP':'Live stereo looper playback and feedback mixture.',
                'A_ATTACK':'Surge voice A amplitude attack: fade-in time after a note starts.',
                'A_DECAY':'Surge voice A decay: time from peak toward sustain level.',
                'A_SUSTAIN':'Surge voice A sustain level while a note is held.',
                'A_RELEASE':'Surge voice A release time after a note ends.',
                'B_ATTACK':'Surge voice B amplitude attack: fade-in time after a note starts.',
                'B_DECAY':'Surge voice B decay: time from peak toward sustain level.',
                'B_SUSTAIN':'Surge voice B sustain level while a note is held.',
                'B_RELEASE':'Surge voice B release time after a note ends.'},
            'macro_descriptions':{
                'CLUSTER':'Adjacent pitches triggered for every played key. Higher values use more polyphony.',
                'BREAK':'Combines drive, FM, ring modulation and feedback as one instability gesture.',
                'FM_RATIO':'Offsets the second oscillator pitch to create inharmonic beating and FM sidebands.',
                'ROUTE_MODE':'Generator bus routing: Parallel, Ring interaction, or Delay Bend.',
                'DRIVE_TYPE':'Choose one of the eight actual Surge waveshaper families.',
                'BODY_MIX':'Mix of the three-comb resonating body after the two generators.',
                'BODY_FEED':'Energy returned into the comb delays; higher values give longer ringing.',
                'MOD1_SRC':'Source of internal modulation slot 1 (off/LFO/random/envelope).',
                'MOD1_DST':'Destination of internal modulation slot 1.',
                'LFO_SELF':'Self-modulation of the LFO rate, for evolving nonperiodic motion.',
                'QUARTER':'Use quarter-tone steps when CHAOS creates note clusters.'}}

class Host:
    def __init__(self,engine_factory=None,error=None):
        extensions.load_enabled()
        if engine_factory is None:engine_factory,self.surge_error=create_surge_factory()
        else:self.surge_error=error or ''
        self.graph=AudioGraph(engine_factory=engine_factory)
        self.audio=AudioService(self.graph)
        self.connections=set()
        self.last_patch=None
        self.presets=PresetStore(ROOT)

    async def send(self,ws,obj):
        if not ws.closed:await ws.send_json(obj)

    async def broadcast(self,obj):
        for ws in tuple(self.connections):
            try:await self.send(ws,obj)
            except Exception:pass

    async def process_message(self,ws,p):
        op=p.get('type')
        if op=='patch':
            state=p.get('state')
            self.graph.load_patch(state)
            self.last_patch=state
            await self.send(ws,{'type':'ack','op':'patch','modules':len(state['modules'])})
        elif op=='module_state':self.graph.module_state(p['id'],p['state'])
        elif op=='modules_scan':
            await self.send(ws,{'type':'modules_catalog','modules':extensions.catalog()})
        elif op=='module_enable':
            if self.audio.status().get('running'):raise PatchError('Stop audio before enabling a new Python module')
            identifier=p.get('id')
            if identifier in extensions.ACTIVE:raise PatchError('Already active; restart Wirescape to reload edited code')
            extensions.enable(identifier)  # explicit local user action; Python code executes here
            await self.broadcast({'type':'modules_changed','schema':schema_payload(),'modules':extensions.catalog()})
        elif op=='control':self.graph.control(p['id'],p['name'],p['value'])
        elif op=='mode':self.graph.mode(p['id'],p['index'])
        elif op=='monitor':self.graph.monitor(p['id'],p['on'])
        elif op=='advanced':self.graph.advanced(p['id'],p['name'],p.get('value'),p.get('clear',False))
        elif op=='raw':self.graph.raw(p['id'],p['voice'],p['name'],p.get('value',0.5),p.get('clear',False))
        elif op=='note':self.graph.keyboard(p['id'],p['note'],p['on'])
        elif op=='panic':self.graph.panic()
        elif op=='preset':
            if p.get('id') not in self.graph.nodes or self.graph.nodes[p['id']].type!='chaos':
                raise PatchError('Preset target must be a CHAOS instance')
            preset=self.presets.load(p['preset'])
            # Browser applies a complete snapshot and sends one authoritative
            # patch synchronization. No half-applied state on the audio thread.
            await self.send(ws,{'type':'preset_loaded','id':p['id'],'preset':preset})
        elif op=='preset_save':
            if p.get('id') not in self.graph.nodes or self.graph.nodes[p['id']].type!='chaos':
                raise PatchError('Preset target must be a CHAOS instance')
            entry=self.presets.save(p.get('name'),p.get('state'))
            await self.send(ws,{'type':'preset_saved','id':p['id'],'entry':entry,'presets':self.presets.list()})
        elif op=='devices':await self.send(ws,{'type':'devices','devices':AudioService.list_devices()})
        elif op=='audio_start':
            result=await asyncio.to_thread(self.audio.start,device=p.get('device'),duplex=bool(p.get('input',False)))
            await self.broadcast({'type':'audio','state':result})
        elif op=='audio_stop':
            await asyncio.to_thread(self.audio.stop)
            await self.broadcast({'type':'audio','state':self.audio.status()})
        else:raise PatchError('Unknown message type: '+str(op))

    async def websocket(self,request):
        origin=request.headers.get('Origin')
        expected=f'{request.scheme}://{request.host}'
        if origin and origin!=expected:
            raise web.HTTPForbidden(text='Cross-origin WebSocket connections are disabled')
        ws=web.WebSocketResponse(max_msg_size=1_048_576,heartbeat=30)
        await ws.prepare(request)
        self.connections.add(ws)
        await self.send(ws,{'type':'hello','version':'0.5.2','surge_ready':not bool(self.surge_error),
                            'surge_error':self.surge_error,'schema':schema_payload(),'status':self.audio.status()})
        try:
            async for msg in ws:
                if msg.type!=WSMsgType.TEXT:continue
                try:
                    payload=json.loads(msg.data)
                    if not isinstance(payload,dict):raise PatchError('Message must be a JSON object')
                    await self.process_message(ws,payload)
                except Exception as exc:
                    await self.send(ws,{'type':'error','message':str(exc)})
        finally:self.connections.discard(ws)
        return ws

    async def status_loop(self,app):
        while True:
            await asyncio.sleep(0.25)
            if self.connections:
                await self.broadcast({'type':'meters','state':self.audio.status()})

    async def status_route(self,request):
        return web.json_response({'version':'0.5.2','surge_error':self.surge_error,
                                   'audio':self.audio.status(),'modules':list(DEFINITIONS)})

    async def module_asset_route(self,request):
        try:path=extensions.asset(request.match_info['module_id'],request.match_info['filename'])
        except FileNotFoundError:raise web.HTTPNotFound()
        return web.FileResponse(path)

    async def shutdown(self,app):
        self.audio.stop()

    def make_app(self):
        app=web.Application(client_max_size=1_048_576)
        app.router.add_get('/',lambda r:web.FileResponse(ROOT/'web'/'index.html'))
        app.router.add_get('/index.html',lambda r:web.FileResponse(ROOT/'web'/'index.html'))
        app.router.add_get('/client.js',lambda r:web.FileResponse(ROOT/'web'/'client.js'))
        app.router.add_get('/module-assets/{module_id}/{filename}',self.module_asset_route)
        app.router.add_get('/health',self.status_route)
        app.router.add_get('/ws',self.websocket)
        app.on_startup.append(lambda a: self._setup_status(a))
        app.on_cleanup.append(self.shutdown)
        return app

    async def _setup_status(self,app):
        task=asyncio.create_task(self.status_loop(app))
        app.on_cleanup.append(lambda a:self._cancel_status(task))

    async def _cancel_status(self,task):
        task.cancel()
        try:await task
        except asyncio.CancelledError:pass


def main():
    parser=argparse.ArgumentParser(description='Wirescape local audio host + patch editor')
    parser.add_argument('--port',type=int,default=8765)
    parser.add_argument('--headless',action='store_true',help='do not launch the web browser')
    args=parser.parse_args()
    host=Host()
    print('WIRESCAPE / 0.5.2 — Open http://127.0.0.1:%d' % args.port)
    if host.surge_error:print('[WARNING]',host.surge_error)
    print('Audio is OFF until enabled in the editor. The server listens on loopback only.')
    if not args.headless:
        import threading
        threading.Timer(1.0,lambda:webbrowser.open('http://127.0.0.1:%d' % args.port)).start()
    web.run_app(host.make_app(),host='127.0.0.1',port=args.port,print=None)

if __name__=='__main__':main()
