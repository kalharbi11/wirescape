"""Wirescape patch schema: serialized API independent from UI or audio framework.

Audio ports are MONO float32 buffers; stereo is represented by L/R pairs. Control
ports carry BLOCK-RATE bipolar values in [-1,1]. Gate/clock are block-rate bools.
Future data/video ports can add their own domain processor without changing audio.
"""
from dataclasses import dataclass
from collections import defaultdict

MAX_MODULES = 24
MAX_CABLES = 128

@dataclass(frozen=True)
class Port:
    name: str
    direction: str
    kind: str

DEFINITIONS = {
    'chaos': {
        'ins': [Port('pitch','in','mod'),Port('morph','in','mod'),Port('chaos','in','mod'),
                Port('gate','in','gate'),Port('notes','in','notes'),Port('cutoff','in','mod'),Port('audio','in','audio')],
        'outs': [Port('left','out','audio'),Port('right','out','audio'),Port('mod','out','mod')],
        'controls': {'MORPH':0, 'CHAOS':0, 'FOLD':0, 'CUTOFF':75,
                     'PITCH':50, 'DETUNE':0, 'CLUSTER':0, 'SUB':0, 'FM':0,
                     'ROUGH':0, 'JITTER':0, 'DRIFT':0, 'FALL':0,
                     'COUPLE':0, 'BODY':0, 'TONE':50, 'FEEDBACK':0,
                     'RATE':30, 'MOTION':0, 'TAPE':0, 'SPACE':0, 'LOOP':0,
                     'A_ATTACK':15, 'A_DECAY':46, 'A_SUSTAIN':100, 'A_RELEASE':62,
                     'B_ATTACK':54, 'B_DECAY':46, 'B_SUSTAIN':100, 'B_RELEASE':69},
        'modes': ['CLUSTER','FM','NOISE'],
    },
    'modulator': {
        'ins': [Port('clock','in','clock'),Port('reset','in','gate'),Port('rate','in','mod')],
        'outs': [Port('modA','out','mod'),Port('modB','out','mod'),Port('gate','out','gate')],
        'controls': {'RATE':44, 'SHAPE':53, 'DEPTH':67, 'DRIFT':29, 'RANDOM':34},
        'modes': ['LFO','RANDOM','ENVELOPE'],
    },
    'interface': {
        'ins': [Port('inL','in','audio'),Port('inR','in','audio')],
        'outs': [Port('extL','out','audio'),Port('extR','out','audio')],
        'controls': {'OUTPUT LEVEL':71,'INPUT GAIN':36},
        'modes': [],
    },
}

class PatchError(ValueError):
    pass

def compatible(source, dest):
    return source.direction == 'out' and dest.direction == 'in' and (
        source.kind == dest.kind or (source.kind == 'gate' and dest.kind == 'clock'))

def validate_patch(patch):
    """Validate ports, signal typing and directed acyclic graph; returns topological ids.

    Cycles require an explicit future delay-feedback node; don't allow accidental
    Python recursion or uncontrolled audio feedback. Multiple cables per input
    are allowed and summed (audio/control) or OR'ed (gate).
    """
    from . import plugins  # builtin registry
    from . import extensions
    extensions.load_enabled()
    if not isinstance(patch, dict):
        raise PatchError('Patch must be an object')
    nodes = patch.get('modules', [])
    wires = patch.get('connections', [])
    if not isinstance(nodes, list) or not isinstance(wires, list):
        raise PatchError('modules and connections must be lists')
    if len(nodes) > MAX_MODULES or len(wires) > MAX_CABLES:
        raise PatchError('Patch exceeds the safety limit of modules or cables')
    from .plugins import STATE_VALIDATORS
    by_id = {}
    for node in nodes:
        if not isinstance(node, dict) or not isinstance(node.get('id'), str) or not node['id'].strip():
            raise PatchError('Module must have a nonempty string id')
        if node['id'] in by_id:
            raise PatchError('Duplicate module id')
        if node.get('type') not in DEFINITIONS:
            raise PatchError('Unknown module type: ' + str(node.get('type')))
        if node['type'] in STATE_VALIDATORS:
            state = node.get('module_state', node.get('sequence', None))
            if state is not None:
                STATE_VALIDATORS[node['type']](state)
        by_id[node['id']] = node
    ports = {}
    for node in nodes:
        for port in DEFINITIONS[node['type']]['ins'] + DEFINITIONS[node['type']]['outs']:
            ports[(node['id'], port.name)] = port
    edges = defaultdict(set)
    indegree = {node_id: 0 for node_id in by_id}
    unique = set()
    for wire in wires:
        if not isinstance(wire, dict):
            raise PatchError('Cable must be an object')
        source, dest = wire.get('from',''), wire.get('to','')
        if not isinstance(source,str) or not isinstance(dest,str) or ':' not in source or ':' not in dest:
            raise PatchError('Invalid cable address')
        src_id,src_port=source.rsplit(':',1)
        dst_id,dst_port=dest.rsplit(':',1)
        a,b=ports.get((src_id,src_port)),ports.get((dst_id,dst_port))
        if not a or not b or not compatible(a,b):
            raise PatchError('Incompatible connection: '+source+' -> '+dest)
        if src_id == dst_id:
            raise PatchError('Self-patching requires an explicit delay node')
        if (source,dest) in unique:
            raise PatchError('Duplicate cable')
        unique.add((source,dest))
        if dst_id not in edges[src_id]:
            edges[src_id].add(dst_id)
            indegree[dst_id]+=1
    ready = sorted(k for k,v in indegree.items() if v==0)
    ordered=[]
    while ready:
        nid=ready.pop(0)
        ordered.append(nid)
        for next_id in sorted(edges[nid]):
            indegree[next_id]-=1
            if indegree[next_id]==0:
                ready.append(next_id)
        ready.sort()
    if len(ordered)!=len(nodes):
        raise PatchError('Feedback cycle rejected: use a future explicit feedback/delay module')
    return ordered


def normalize_control(kind, name, value):
    if name not in DEFINITIONS[kind]['controls']:
        raise PatchError('Unknown control %s for %s' % (name,kind))
    try:
        value=float(value)
    except (ValueError,TypeError):
        raise PatchError('Control must be numerical')
    import math
    if not math.isfinite(value):
        raise PatchError('Control must be finite')
    return max(0.0,min(100.0,value))
