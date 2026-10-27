# SPDX-License-Identifier: GPL-3.0-only
"""Offline ABI1 rendering of native patch v1; never creates a device stream."""
import ctypes as C
from pathlib import Path


class Config(C.Structure):
    _fields_ = [('size', C.c_uint32), ('rate', C.c_double), ('block', C.c_uint32),
                ('nodes', C.c_uint32), ('events', C.c_uint32)]


class Event(C.Structure):
    _fields_ = [('size', C.c_uint32), ('frame', C.c_uint64), ('node', C.c_uint32),
                ('type', C.c_uint32), ('parameter', C.c_uint32), ('value', C.c_float),
                ('source', C.c_uint32), ('note', C.c_uint32), ('generation', C.c_uint64)]


class Port(C.Structure):
    _fields_ = [('id', C.c_char_p), ('name', C.c_char_p), ('kind', C.c_uint32)]


class Descriptor(C.Structure):
    _fields_ = [('size', C.c_uint32), ('abi', C.c_uint32), ('id', C.c_char_p), ('name', C.c_char_p),
                ('inputs', C.c_uint32), ('outputs', C.c_uint32), ('parameters', C.c_uint32),
                ('in_ports', C.POINTER(Port)), ('out_ports', C.POINTER(Port))]


def tone_patch():
    return dict(format='wirescape.native.patch', version=1, outputNode=2,
                modules=[dict(id=1, type='essentials.vco', values={'0': 440}),
                         dict(id=2, type='essentials.output', values={})],
                connections=[dict(fromNode=1, fromPort='audio', toNode=2, toPort='audio')])


def render(engine, modules, patch, events=(), seconds=2):
    if patch.get('format') != 'wirescape.native.patch' or patch.get('version') != 1:
        raise ValueError('only executable native patch v1 is supported')
    if not 0.1 <= seconds <= 5 or not 1 <= len(patch['modules']) <= 32 or len(events) > 1024:
        raise ValueError('reference exceeds duration/graph/event bounds')
    lib = C.CDLL(str(Path(engine).resolve()))
    def bind(name, result, *args):
        f = getattr(lib, 'ws_engine_' + name)
        f.restype, f.argtypes = result, args
        return f
    ptr, u32 = C.c_void_p, C.c_uint32
    create = bind('create', ptr, C.POINTER(Config))
    destroy = bind('destroy', None, ptr)
    load = bind('load_module', C.c_int, ptr, C.c_char_p)
    count = bind('module_count', u32, ptr)
    descriptor = bind('module', C.POINTER(Descriptor), ptr, u32)
    add = bind('add_node', C.c_int, ptr, u32, C.c_char_p)
    connect = bind('connect', C.c_int, ptr, u32, u32, u32, u32)
    prepare = bind('prepare', C.c_int, ptr, u32, u32)
    parameter = bind('initialize_parameter', C.c_int, ptr, u32, u32, C.c_float)
    enqueue = bind('enqueue', C.c_int, ptr, C.POINTER(Event))
    generation = bind('generation', C.c_uint64, ptr)
    process = bind('render', C.c_int, ptr, C.POINTER(C.c_float), u32)
    error = bind('error', C.c_char_p, ptr)
    e = create(C.byref(Config(C.sizeof(Config), 48000, 512, 32, 2048)))
    if not e:
        raise RuntimeError('engine creation failed')
    def check(code):
        if code:
            raise RuntimeError(f'engine result {code}: {error(e).decode("utf-8", "replace")}')
    try:
        loaded, files, nodes = {}, [], {n['id']: n for n in patch['modules']}
        if len(nodes) != len(patch['modules']):
            raise ValueError('duplicate node IDs')
        for node in nodes.values():
            identity = node['type']
            if identity not in loaded:
                suffix = identity.removeprefix('essentials.')
                if identity != 'essentials.' + suffix or not suffix.replace('_', '').isalnum():
                    raise ValueError('unsupported module identity')
                candidates = [Path(modules) / name for name in
                              (f'ws_{suffix}.dll', f'libws_{suffix}.so', f'libws_{suffix}.dylib')]
                file = next((p for p in candidates if p.is_file()), None)
                if file is None:
                    raise ValueError('module unavailable: ' + identity)
                check(load(e, str(file.resolve()).encode('utf-8')))
                d = descriptor(e, count(e) - 1).contents
                if d.id.decode() != identity:
                    raise ValueError('module identity mismatch')
                loaded[identity] = d
                files.append(file)
            check(add(e, node['id'], identity.encode()))
        def port(node, name, output):
            d = loaded[nodes[node]['type']]
            ports, size = (d.out_ports, d.outputs) if output else (d.in_ports, d.inputs)
            return next(i for i in range(size) if ports[i].id.decode() == name)
        for wire in patch['connections']:
            a, b = wire['fromNode'], wire['toNode']
            check(connect(e, a, port(a, wire['fromPort'], True), b, port(b, wire['toPort'], False)))
        check(prepare(e, patch['outputNode'], 0))
        for node in nodes.values():
            for key, value in node.get('values', {}).items():
                check(parameter(e, node['id'], int(key), float(value)))
        previous = -1
        for ev in events:
            frame = ev['frame']
            if not isinstance(frame, int) or not previous <= frame < seconds * 48000:
                raise ValueError('events must be ordered and inside the reference')
            previous = frame
            value = Event(C.sizeof(Event), frame, ev['node'], ev['type'], ev.get('parameter', 0),
                          ev.get('value', 0), ev.get('source', 1), ev.get('note', 1), generation(e))
            check(enqueue(e, C.byref(value)))
        samples, buffer = [], (C.c_float * 512)()
        while len(samples) < round(seconds * 48000):
            n = min(512, round(seconds * 48000) - len(samples))
            check(process(e, buffer, n))
            samples.extend(float(buffer[i]) * 0.05 for i in range(n))
        return samples, files
    finally:
        destroy(e)
