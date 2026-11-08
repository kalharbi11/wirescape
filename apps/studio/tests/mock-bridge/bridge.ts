// SPDX-License-Identifier: GPL-3.0-only
import type { Page } from '@playwright/test';

/** Electron's five-member bridge, injected before any Studio module runs. */
export async function mockBridge(page: Page): Promise<void> {
  await page.addInitScript(() => {
    type Message = Record<string, any>;
    const listeners = new Set<(m: Message) => void>();
    const audio = [{ id: 'USB Stereo', name: 'USB Stereo', kind: 'audio', sampleRates: [48000], index: 3 }];
    const midi = [{ id: 'M8 MIDI 1|M8:M8 MIDI 1', name: 'M8 MIDI 1',
      system_name: 'M8:M8 MIDI 1', enabled: true, kind: 'midi' }];
    const audioStatus = { state: 'running', deviceId: 'USB Stereo', deviceName: 'USB Stereo',
      sampleRate: 48000, bufferFrames: 256, latencyMs: 10.6667, cpuPercent: 2.1, dropouts: 0 };
    let sessionNo = 1, gen = 0, graph: Message = { modules: [], connections: [] };
    let catalogue: Message = { modules: [] };
    let delay = 0, saved = '', openText: { text: string; name: string } | null = null;
    const refused: { type: string; code: string }[] = [];
    const held: { cmd: Message; session: string; gen: string }[] = [];
    const sent: Message[] = [];
    const session = () => 's' + sessionNo;
    const emit = (m: Message) => { for (const cb of listeners) cb(m); };
    const copy = (v: any) => JSON.parse(JSON.stringify(v));
    const reply = (cmd: Message, ok: boolean, response: Message = {}, s = session(), g = String(gen)) => {
      emit({ v: 0, session: s, re: cmd.id, ok, gen: g, ...response });
    };
    const hello = async () => {
      if (!catalogue.modules.length) {
        catalogue = await (await fetch('/wirescape_modules.json')).json();
      }
      emit({ v: 0, type: 'hello', session: session(), protocol: 0,
        engine: { version: 'test', sha: 'abc1234' }, capabilities: [],
        catalogue, audio: audioStatus,
        midi: { devices: copy(midi), activity: {} } });
    };
    const accept = (cmd: Message, s: string) => {
      if (s !== session()) return;
      const rejectAt = refused.findIndex(x => x.type === cmd.type);
      if (rejectAt >= 0) {
        const [r] = refused.splice(rejectAt, 1);
        reply(cmd, false, { code: r!.code, message: 'Scripted ' + r!.code }, s);
        return;
      }
      switch (cmd.type) {
        case 'loadGraph':
          if (cmd.patch.modules.some((m: Message) =>
            !catalogue.modules.some((d: Message) => d.id === m.module))) {
            reply(cmd, false, { code: 'unknown-module', message: 'Module not installed' }, s);
            return;
          }
          graph = copy(cmd.patch); gen++; reply(cmd, true, {}, s);
          break;
        case 'addModule':
          if (!catalogue.modules.some((d: Message) => d.id === cmd.moduleType)) {
            reply(cmd, false, { code: 'unknown-module', message: 'Module not installed' }, s);
            break;
          }
          graph.modules.push({ id: cmd.moduleId, module: cmd.moduleType, parameters: cmd.params });
          gen++; reply(cmd, true, {}, s); break;
        case 'removeModule':
          graph.modules = graph.modules.filter((m: Message) => m.id !== cmd.moduleId);
          graph.connections = graph.connections.filter((c: Message) =>
            c.from.module !== cmd.moduleId && c.to.module !== cmd.moduleId);
          gen++; reply(cmd, true, {}, s); break;
        case 'connect':
          graph.connections.push(copy(cmd.connection)); gen++; reply(cmd, true, {}, s); break;
        case 'disconnect':
          graph.connections = graph.connections.filter((c: Message) =>
            JSON.stringify([c.kind, c.from, c.to]) !==
            JSON.stringify([cmd.connection.kind, cmd.connection.from, cmd.connection.to]));
          gen++; reply(cmd, true, {}, s); break;
        case 'setParam': {
          const node = graph.modules.find((m: Message) => m.id === cmd.moduleId);
          if (!node) { reply(cmd, false, { code: 'not-found', message: 'No module' }, s); break; }
          (node.parameters ??= {})[cmd.paramId] = cmd.value;
          reply(cmd, true, {}, s); break;
        }
        case 'setModAmount': {
          const conn = graph.connections.find((c: Message) =>
            c.kind === cmd.connection.kind && c.from.module === cmd.connection.from.module &&
            c.from.port === cmd.connection.from.port && c.to.module === cmd.connection.to.module &&
            c.to.port === cmd.connection.to.port);
          if (!conn) { reply(cmd, false, { code: 'not-found', message: 'No cable' }, s); break; }
          conn.amount = cmd.amount; gen++; reply(cmd, true, {}, s); break;
        }
        case 'listAudioDevices': reply(cmd, true, { result: { devices: copy(audio) } }, s); break;
        case 'listMidiDevices': reply(cmd, true, { result: { devices: copy(midi) } }, s); break;
        case 'setMidiEnabled':
          for (const d of midi) if (d.id === cmd.deviceId) d.enabled = cmd.on;
          reply(cmd, true, {}, s); break;
        default: reply(cmd, true, {}, s);
      }
    };
    const send = (cmd: Message) => {
      sent.push(copy(cmd));
      if (cmd.type === 'app.restartEngine') {
        emit({ v: 0, re: cmd.id, ok: true, gen: '0' });
        exit(true);
        return;
      }
      const s = session();
      if (holdType === cmd.type) { holdType = ''; held.push({ cmd: copy(cmd), session: s, gen: String(gen) }); return; }
      setTimeout(() => accept(cmd, s), delay);
    };
    let holdType = '';
    const exit = (expected = false) => {
      emit({ v: 0, type: 'app.engineExited', exitCode: expected ? 0 : null,
        signal: null, expected });
      sessionNo++; gen = 0;
      graph = { modules: [], connections: [] };
      setTimeout(() => void hello(), 250);
    };
    (window as any).wirescape = {
      send,
      onMessage(cb: (m: Message) => void) {
        listeners.add(cb);
        setTimeout(() => void hello(), 0);
        return () => listeners.delete(cb);
      },
      openPatch: async (request?: { library: string }) => request
        ? request.library === 'list' ? { name: 'User patches', text: '[]' } : null
        : openText,
      savePatch: async (text: string) => { saved = text; return { name: 'Saved.wirescape.json' }; },
      platform: 'linux',
    };
    (window as any).__mockBridge = {
      sent, getGraph: () => copy(graph), getSession: session,
      getSaved: () => saved,
      setOpen: (value: { text: string; name: string } | null) => { openText = value; },
      setDelay: (ms: number) => { delay = ms; },
      refuseNext: (type: string, code = 'refused') => { refused.push({ type, code }); },
      holdNext: (type: string) => { holdType = type; },
      releaseHeld: () => { for (const h of held.splice(0)) reply(h.cmd, true, {}, h.session, h.gen); },
      exit,
      devices: () => emit({ v: 0, type: 'devices', session: session(), gen: String(gen),
        audioDevices: copy(audio), midiDevices: copy(midi) }),
      status: () => emit({ v: 0, type: 'status', session: session(), gen: String(gen),
        status: { audio: copy(audioStatus), midi: { devices: copy(midi), activity: {} } } }),
      telemetry: (data: Message) => emit({ v: 0, type: 'telemetry', session: session(),
        gen: String(gen), telemetry: { tick: 12, time: 0.4, effective: {}, phase: {},
          envelope: {}, meters: {}, scope: {}, eventActivity: {}, heldNotes: [], ...data } }),
    };
  });
}
