// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2, Connection } from '../patch/types';
import { connKey } from '../patch/types';
import type { ModuleDescriptor } from '../modules/types';
import type { ModuleRegistry } from '../modules/registry';
import { EngineRefusal, type EngineClient, type EngineError, type Device, type SystemStatus, type Telemetry } from './EngineClient';

/** The complete five-member Electron preload surface; never expose Node to the renderer. */
export interface WirescapeBridge {
  send(obj: Record<string, unknown>): void;
  onMessage(cb: (message: Record<string, unknown>) => void): () => void;
  openPatch(request?: { library: 'list' | 'read' | 'reset'; name?: string }): Promise<{ text: string; name: string } | null>;
  savePatch(text: string, options?: { saveAs?: boolean }): Promise<{ name: string } | null>;
  readonly platform: 'linux' | 'win32' | 'darwin';
  /** Set only by the web demo bridge (D14). */
  readonly web?: boolean;
}
declare global { interface Window { wirescape?: WirescapeBridge } }

type Wire = Record<string, unknown>;
type Mutation = { type: string; payload: Wire };
type Edit = {
  id: string; session: string; mutation: Mutation; inverse: PatchV2 | null;
  rollback?: () => void;
};
type Pending = {
  session: string; type: string; kind: 'load' | 'edit' | 'query' | 'control' | 'restart';
  resolve?: (value: Wire) => void; reject?: (reason: EngineRefusal) => void;
  revision?: number; edit?: Edit; timeout?: number;
};
type Load = {
  id: string; patch: PatchV2; revision: number;
  resolve: () => void; reject: (reason: EngineRefusal) => void; sent: boolean;
};

const copy = <T>(value: T): T => JSON.parse(JSON.stringify(value)) as T;
const isRecord = (v: unknown): v is Wire => v !== null && typeof v === 'object' && !Array.isArray(v);
const code = (v: unknown): EngineError['code'] =>
  (typeof v === 'string' && ['unknown-module', 'kind-mismatch', 'duplicate', 'not-found',
    'refused', 'invalid', 'busy', 'limit', 'unavailable'].includes(v)) ? v as EngineError['code'] : 'refused';
const refusal = (c: unknown, msg: unknown): EngineRefusal =>
  new EngineRefusal({ code: code(c), message: typeof msg === 'string' && msg ? msg : 'Engine command refused.' });
const generation = (s: unknown): bigint | null =>
  typeof s === 'string' && /^\d+$/.test(s) ? BigInt(s) : null;

/**
 * Serializes optimistic edits over v0. Only one edit is in flight so a failed
 * mutation can be removed and dependent unsent edits revalidated in order.
 * The editable PatchDoc is never replaced by the executable projection.
 */
export class RealEngine implements EngineClient {
  private session = '';
  private gen = '0';
  private playheadEnabled = false;
  private monitorEnabled = false;
  private playhead: Telemetry['playhead'] = {};
  private statusGen = '';
  private serial = 0;
  private revision = 0;
  private loadedOnce = false;
  private ready = false;
  private started = false;
  private unsubscribe?: () => void;
  private pending = new Map<string, Pending>();
  private load?: Load;
  private queue: Edit[] = [];
  private active?: Edit;
  private lastEdit?: Edit;
  private settledWaiters = new Set<() => void>();
  private confirmed: PatchV2 | null = null;
  private usedIds = new Set<number>();
  private audioDevices: Device[] = [];
  private midiDevices: Device[] = [];
  private status?: SystemStatus;
  private state: 'real' | 'restarting' = 'restarting';
  private telemetryCbs = new Set<(t: Telemetry) => void>();
  private statusCbs = new Set<(s: SystemStatus) => void>();
  private refusalCbs = new Set<(r: { requestId: string; type: string; detail: EngineError; gen: string }) => void>();
  private stateCbs = new Set<(state: 'real' | 'restarting') => void>();
  private graphErrorCbs = new Set<(error: EngineRefusal) => void>();
  private catalogueCbs = new Set<() => void>();
  private generationCbs = new Set<(gen: string) => void>();
  setSampleRate?: (hz: number) => void;
  setBufferFrames?: (frames: number) => void;
  midiLearn?: (armed: boolean) => void;

  constructor(
    private readonly bridge: WirescapeBridge,
    private readonly currentDocument: () => PatchV2,
    private readonly registry: ModuleRegistry,
  ) {}

  start(): void {
    if (this.started) return;
    this.started = true;
    this.unsubscribe = this.bridge.onMessage(m => this.receive(m));
    this.setState('restarting');
  }

  stop(): void { this.unsubscribe?.(); this.unsubscribe = undefined; this.started = false; }

  canApplyGroove(moduleId: number): boolean {
    if (!this.ready || this.state !== 'real' || this.load || this.active || this.queue.length) return false;
    if (this.statusGen === this.gen && this.status && this.status.audio.state !== 'running') return true;
    const position = this.playheadEnabled ? this.playhead?.[moduleId] : undefined;
    return position?.[0] === -1 && position[1] === -1;
  }

  onTelemetry(cb: (t: Telemetry) => void): () => void { this.telemetryCbs.add(cb); return () => this.telemetryCbs.delete(cb); }
  onStatus(cb: (s: SystemStatus) => void): () => void {
    this.statusCbs.add(cb);
    if (this.status) cb(this.status);
    return () => this.statusCbs.delete(cb);
  }
  onRefusal(cb: (r: { requestId: string; type: string; detail: EngineError; gen: string }) => void): () => void {
    this.refusalCbs.add(cb); return () => this.refusalCbs.delete(cb);
  }
  onConnection(cb: (state: 'real' | 'restarting') => void): () => void {
    this.stateCbs.add(cb); cb(this.state); return () => this.stateCbs.delete(cb);
  }
  onGraphError(cb: (error: EngineRefusal) => void): () => void {
    this.graphErrorCbs.add(cb); return () => this.graphErrorCbs.delete(cb);
  }
  onCatalogue(cb: () => void): () => void {
    this.catalogueCbs.add(cb); return () => this.catalogueCbs.delete(cb);
  }
  generation(): string { return this.gen; }
  onGeneration(cb: (gen: string) => void): () => void {
    this.generationCbs.add(cb); return () => this.generationCbs.delete(cb);
  }
  settleUnansweredLoad(applied: boolean): void {
    const load = this.load;
    if (!load) return;
    this.pending.delete(load.id);
    this.load = undefined;
    if (applied) {
      this.confirmed = copy(load.patch);
      this.usedIds = new Set(this.confirmed.modules.map(m => m.id));
      this.ready = true; this.loadedOnce = true;
      load.resolve(); this.setState('real'); this.pumpEdits();
    } else {
      this.ready = this.confirmed !== null;
      load.reject(refusal('unavailable', 'Load was not applied.'));
    }
    this.notifySettled();
  }
  private waitForSessionReady(session: string, timeout: number): Promise<void> {
    if (this.session !== session && this.ready && this.state === 'real') return Promise.resolve();
    return new Promise<void>((resolve, reject) => {
      let off = () => {};
      const timer = window.setTimeout(() => {
        off(); reject(refusal('unavailable', 'Engine restart timed out before the patch loaded.'));
      }, timeout);
      off = this.onConnection(state => {
        if (this.session !== session && this.ready && state === 'real') {
          window.clearTimeout(timer); off(); resolve();
        }
      });
    });
  }
  restartEngine(timeoutMs = 10_000): Promise<void> {
    const id = this.id(), session = this.session, deadline = Date.now() + timeoutMs;
    return new Promise<void>((resolve, reject) => {
      const timer = window.setTimeout(() => {
        this.pending.delete(id); reject(refusal('unavailable', 'Engine restart timed out.'));
      }, timeoutMs);
      this.pending.set(id, { session, type: 'app.restartEngine', kind: 'restart', timeout: timer,
        resolve: () => {
          window.clearTimeout(timer);
          void this.waitForSessionReady(session, Math.max(0, deadline - Date.now())).then(resolve, reject);
        },
        reject: err => { window.clearTimeout(timer); reject(err); } });
      try { this.bridge.send({ v: 0, id, type: 'app.restartEngine' }); }
      catch { window.clearTimeout(timer); this.pending.delete(id); reject(refusal('unavailable', 'Could not restart engine.')); }
    });
  }
  /** A document switch must keep its owner visible until all inverses have settled. */
  async whenEditsSettled(): Promise<void> {
    while (this.active || this.queue.length || this.load)
      await new Promise<void>(resolve => this.settledWaiters.add(resolve));
  }
  private notifySettled(): void {
    if (this.active || this.queue.length || this.load) return;
    for (const resolve of this.settledWaiters) resolve();
    this.settledWaiters.clear();
  }

  /** Attach an App-owned inverse to the most recently queued optimistic edit. */
  registerRollback(fn: () => void): void { if (this.lastEdit) this.lastEdit.rollback = fn; }

  private setState(next: 'real' | 'restarting'): void {
    this.state = next;
    for (const cb of this.stateCbs) cb(next);
  }
  private id(): string { return 'ui' + (++this.serial); }
  private send(id: string, type: string, payload: Wire = {}): void {
    this.bridge.send({ v: 0, id, type, ...payload });
  }
  private emitStatus(s: SystemStatus): void {
    this.status = s;
    for (const cb of this.statusCbs) cb(s);
  }
  private rejectPending(error: EngineRefusal): void {
    for (const [id, p] of this.pending) {
      if (p.kind === 'restart') continue;
      if (p.timeout !== undefined) window.clearTimeout(p.timeout);
      p.reject?.(error); this.pending.delete(id);
    }
    this.active = undefined;
    this.queue = [];
    this.lastEdit = undefined;
    this.load?.reject(error);
    this.load = undefined;
    this.notifySettled();
  }

  private receive(message: Wire): void {
    if (!isRecord(message) || message.v !== 0) return;
    if (message.type === 'app.engineExited') {
      this.session = '';
      this.playhead = {}; this.statusGen = '';
      this.playheadEnabled = false;
      this.monitorEnabled = false;
      this.ready = false;
      this.confirmed = null;
      this.setState('restarting');
      this.rejectPending(refusal('unavailable', 'Engine disconnected.'));
      // Main supervises crash restarts (bounded crash-loop budget); the renderer only waits for the next hello.
      return;
    }
    if (message.type === 'hello') {
      if (message.protocol !== 0 || typeof message.session !== 'string' || !message.session) return;
      if (this.session === message.session) return;
      const resumeLoad = this.load && !this.load.sent ? this.load : undefined;
      const wasRunning = this.loadedOnce && !resumeLoad;
      if (!resumeLoad) this.rejectPending(refusal('unavailable', 'Engine session replaced.'));
      else { this.pending.clear(); this.active = undefined; this.queue = []; }
      this.session = message.session;
      this.gen = '0';
      this.playhead = {}; this.statusGen = '';
      this.ready = false;
      this.usedIds.clear();
      this.confirmed = null;
      const capabilities = Array.isArray(message.capabilities) ? message.capabilities : [];
      this.playheadEnabled = capabilities.includes('telemetry.playhead');
      this.monitorEnabled = capabilities.includes('telemetry.monitor');
      this.setSampleRate = capabilities.includes('setSampleRate')
        ? hz => this.control('setSampleRate', { hz }) : undefined;
      this.setBufferFrames = capabilities.includes('setBufferFrames')
        ? frames => this.control('setBufferFrames', { frames }) : undefined;
      this.midiLearn = capabilities.includes('midiLearn')
        ? armed => this.control('midiLearn', { armed }) : undefined;
      if (isRecord(message.catalogue) && Array.isArray(message.catalogue.modules)) {
        this.registry.replace(message.catalogue.modules as ModuleDescriptor[]);
        for (const cb of this.catalogueCbs) cb();
      }
      if (isRecord(message.audio) && isRecord(message.midi)) {
        this.midiDevices = Array.isArray(message.midi.devices) ? message.midi.devices as Device[] : [];
        this.statusGen = this.gen;
        this.emitStatus({ audio: message.audio as unknown as SystemStatus['audio'],
          midi: { devices: this.midiDevices, activity: isRecord(message.midi.activity)
            ? message.midi.activity as Record<string, number> : {},
          ...(isRecord(message.midi.learn) ? { learn: message.midi.learn as unknown as
            NonNullable<SystemStatus['midi']['learn']> } : {}) } });
      }
      if (resumeLoad) this.dispatchLoad();
      else if (wasRunning) {
        void this.loadGraph(this.currentDocument()).catch(e => {
          for (const cb of this.graphErrorCbs) cb(e as EngineRefusal);
        });
      }
      return;
    }
    if (message.type?.toString().startsWith('app.menu')) return; // App consumes menu intents.
    if (message.type === 'status' || message.type === 'telemetry' || message.type === 'devices') {
      const pushedGen = generation(message.gen);
      if (message.session !== this.session || pushedGen === null || pushedGen < BigInt(this.gen)) return;
      if (pushedGen > BigInt(this.gen)) {
        this.gen = message.gen as string;
        this.playhead = {}; this.statusGen = '';
      }
      for (const cb of this.generationCbs) cb(this.gen);
      if (message.type === 'status' && isRecord(message.status)) {
        const next = message.status as unknown as SystemStatus;
        this.midiDevices = next.midi?.devices ?? this.midiDevices;
        this.statusGen = this.gen;
        this.emitStatus(next);
      } else if (message.type === 'devices') {
        if (Array.isArray(message.audioDevices)) this.audioDevices = message.audioDevices as Device[];
        if (Array.isArray(message.midiDevices)) this.midiDevices = message.midiDevices as Device[];
        if (this.status) this.emitStatus({ ...this.status, midi: { ...this.status.midi, devices: this.midiDevices } });
      } else if (message.type === 'telemetry' && isRecord(message.telemetry)) {
        const t = { ...message.telemetry, gen: message.gen } as unknown as Telemetry;
        t.monitorAvailable = this.monitorEnabled; // Renderer-only support bit from hello.
        if (!this.monitorEnabled) delete t.monitor;
        // Older/absent generations are unknown: never let them authorize Apply.
        this.playhead = this.playheadEnabled && message.gen === this.gen && isRecord(t.playhead) ? t.playhead : {};
        if (!this.playheadEnabled) { delete t.playhead; delete t.groove; }
        // v0 transmits JSON arrays, never invent absent scope/effective/outputs.
        t.scope = Object.fromEntries(Object.entries(t.scope ?? {}).map(([k, v]) =>
          [k, Array.isArray(v) ? Float32Array.from(v) : v])) as Telemetry['scope'];
        for (const cb of this.telemetryCbs) cb(t);
      }
      return;
    }
    if (typeof message.re !== 'string') return;
    const p = this.pending.get(message.re);
    if (!p || (p.kind !== 'restart' && (message.session !== p.session || message.session !== this.session))) return;
    const g = generation(message.gen);
    if (p.kind !== 'restart' && (g === null || g < BigInt(this.gen))) return;
    this.pending.delete(message.re);
    if (p.timeout !== undefined) window.clearTimeout(p.timeout);
    if (g !== null && g > BigInt(this.gen) && p.kind !== 'restart') {
      this.gen = message.gen as string;
      this.playhead = {}; this.statusGen = '';
    }
    if (message.ok !== true) {
      const err = refusal(message.code, message.message);
      if (p.kind === 'load') {
        // A refused loadGraph leaves the engine on its previous graph (sidecar rollback).
        (err.detail as { engine?: true }).engine = true;
        if (p.revision === this.revision) {
          this.ready = this.confirmed !== null; // the atomic refusal leaves the confirmed graph untouched
          this.queue = []; this.active = undefined;
          this.load?.reject(err); this.load = undefined;
          this.notifySettled();
        }
      } else if (p.kind === 'edit' && p.edit) this.failEdit(p.edit, err);
      else p.reject?.(err);
      return;
    }
    if (p.kind === 'load') {
      if (p.revision === this.revision && this.load) {
        this.confirmed = copy(this.load.patch);
        this.usedIds = new Set(this.confirmed.modules.map(m => m.id));
        this.ready = true;
        this.loadedOnce = true;
        this.load!.resolve();
        this.load = undefined;
        this.setState('real');
        this.pumpEdits();
        this.notifySettled();
      }
    } else if (p.kind === 'edit' && p.edit) {
      if (this.confirmed) applyMutation(this.confirmed, p.edit.mutation, this.usedIds);
      this.queue.shift();
      this.active = undefined;
      this.pumpEdits();
      this.notifySettled();
    } else p.resolve?.(message);
  }

  async loadGraph(patch: PatchV2): Promise<void> {
    this.playhead = {};
    this.loadedOnce = true;
    this.ready = false;
    this.active = undefined;
    this.queue = [];
    this.lastEdit = undefined;
    if (this.load) this.load.reject(refusal('unavailable', 'Patch load superseded.'));
    const revision = ++this.revision;
    return new Promise<void>((resolve, reject) => {
      this.load = { id: this.id(), patch: copy(patch), revision, resolve, reject, sent: false };
      this.dispatchLoad();
    });
  }
  private dispatchLoad(): void {
    const l = this.load;
    if (!l || !this.session || l.sent) return;
    l.sent = true;
    this.pending.set(l.id, { session: this.session, type: 'loadGraph', kind: 'load',
      revision: l.revision });
    try { this.send(l.id, 'loadGraph', { patch: l.patch }); }
    catch { this.pending.delete(l.id); l.reject(refusal('unavailable', 'Bridge unavailable.')); this.load = undefined; this.notifySettled(); }
  }

  private enqueue(type: string, payload: Wire): void {
    // A blank/unavailable patch may have failed its initial load. Once an
    // editable change makes it runnable, resubmit the full document instead
    // of sending an edit against an unknown native generation.
    if (!this.ready && !this.load && this.session) {
      void this.loadGraph(this.currentDocument()).catch(e => {
        for (const cb of this.graphErrorCbs) cb(e as EngineRefusal);
      });
      return;
    }
    const mutation = { type, payload };
    const projected = this.projection();
    const inverse = projected ? copy(projected) : null;
    const entry: Edit = { id: this.id(), session: this.session, mutation, inverse };
    this.queue.push(entry);
    this.lastEdit = entry;
    this.pumpEdits();
  }
  private projection(): PatchV2 | null {
    if (!this.confirmed) return null;
    const patch = copy(this.confirmed);
    const ids = new Set(this.usedIds);
    for (const edit of this.queue) applyMutation(patch, edit.mutation, ids);
    return patch;
  }
  private pumpEdits(): void {
    if (!this.ready || !this.session || this.active || !this.confirmed) return;
    while (this.queue.length) {
      const entry = this.queue[0]!;
      const projection = copy(this.confirmed);
      if (!applyMutation(projection, entry.mutation, new Set(this.usedIds))) {
        this.failEdit(entry, refusal('not-found', 'Edit no longer applies to the running graph.'));
        continue;
      }
      entry.session = this.session;
      this.active = entry;
      this.playhead = {};
      this.pending.set(entry.id, { session: this.session, type: entry.mutation.type, kind: 'edit', edit: entry });
      try { this.send(entry.id, entry.mutation.type, entry.mutation.payload); }
      catch { this.pending.delete(entry.id); this.failEdit(entry, refusal('unavailable', 'Bridge unavailable.')); }
      return;
    }
  }
  private failEdit(entry: Edit, err: EngineRefusal): void {
    this.active = undefined;
    this.queue = this.queue.filter(x => x !== entry);
    entry.rollback?.();
    for (const cb of this.refusalCbs) cb({
      requestId: entry.id, type: entry.mutation.type, detail: err.detail, gen: this.gen,
    });
    // Revalidation of later queued edits is performed against the last ack.
    this.pumpEdits();
    this.notifySettled();
  }
  private request(type: string, payload: Wire = {}): Promise<Wire> {
    if (!this.session) return Promise.reject(refusal('unavailable', 'Engine disconnected.'));
    const id = this.id();
    return new Promise<Wire>((resolve, reject) => {
      this.pending.set(id, { session: this.session, type, kind: 'query', resolve, reject });
      try { this.send(id, type, payload); }
      catch { this.pending.delete(id); reject(refusal('unavailable', 'Bridge unavailable.')); }
    });
  }
  private control(type: string, payload: Wire = {}): void {
    void this.request(type, payload).catch(err => {
      for (const cb of this.refusalCbs) cb({ requestId: '', type, detail: (err as EngineRefusal).detail, gen: this.gen });
    });
  }

  addModule(moduleId: number, moduleType: string, params: Record<string, number> = {}): void {
    this.enqueue('addModule', { moduleId, moduleType, params });
  }
  removeModule(moduleId: number): void { this.enqueue('removeModule', { moduleId }); }
  connect(connection: Connection): void { this.enqueue('connect', { connection: copy(connection) }); }
  disconnect(connection: Connection): void { this.enqueue('disconnect', { connection: copy(connection) }); }
  setParam(moduleId: number, paramId: string, value: number): void { this.enqueue('setParam', { moduleId, paramId, value }); }
  setModAmount(connection: Connection, amount: number): void {
    this.enqueue('setModAmount', { connection: copy(connection), amount });
  }
  noteOn(note: number, velocity: number): void { this.control('noteOn', { note, velocity }); }
  noteOff(note: number): void { this.control('noteOff', { note }); }
  panic(): void { this.control('panic'); }
  async listAudioDevices(): Promise<Device[]> {
    const result = await this.request('listAudioDevices');
    const body = isRecord(result.result) ? result.result : {};
    if (Array.isArray(body.devices)) this.audioDevices = body.devices as Device[];
    // Registry failures are informational, never extra selectable output IDs.
    const reported = isRecord(body.audio) ? body.audio.unavailableDrivers : undefined;
    const unavailable = Array.isArray(reported) ? reported.filter(v =>
      isRecord(v) && typeof v.hostApi === 'string' && typeof v.name === 'string'
    ).map(v => ({
      hostApi: v.hostApi as string, name: v.name as string,
      ...(typeof v.reason === 'string' ? { reason: v.reason } : {}),
      ...(typeof v.detail === 'string' ? { detail: v.detail } : {}),
    })) : [];
    if (this.status) {
      const audio = { ...this.status.audio };
      delete audio.unavailableDrivers;
      if (unavailable.length) audio.unavailableDrivers = unavailable;
      this.emitStatus({ ...this.status, audio });
    }
    return this.audioDevices.slice();
  }
  selectAudioDevice(deviceId: string): void { this.control('selectAudioDevice', { deviceId }); }
  async listMidiDevices(): Promise<Device[]> {
    const result = await this.request('listMidiDevices');
    const devices = (result.result as { devices?: Device[] } | undefined)?.devices;
    if (Array.isArray(devices)) this.midiDevices = devices;
    return this.midiDevices.slice();
  }
  setMidiEnabled(deviceId: string, on: boolean): void { this.control('setMidiEnabled', { deviceId, on }); }
}

/** Pure execution projection: intentionally does not touch ui or other saved metadata. */
function applyMutation(patch: PatchV2, m: Mutation, used: Set<number>): boolean {
  const p = m.payload;
  const id = p.moduleId as number;
  const connection = p.connection as Connection;
  switch (m.type) {
    case 'addModule':
      if (used.has(id) || typeof p.moduleType !== 'string') return false;
      patch.modules.push({ id, module: p.moduleType, parameters: copy(p.params ?? {}) as Record<string, number> });
      used.add(id); return true;
    case 'removeModule':
      if (!patch.modules.some(x => x.id === id)) return false;
      patch.modules = patch.modules.filter(x => x.id !== id);
      patch.connections = patch.connections.filter(x => x.from.module !== id && x.to.module !== id);
      return true;
    case 'connect':
      if (!patch.modules.some(x => x.id === connection.from.module) ||
          !patch.modules.some(x => x.id === connection.to.module) ||
          patch.connections.some(x => connKey(x) === connKey(connection))) return false;
      patch.connections.push(copy(connection)); return true;
    case 'disconnect': {
      const i = patch.connections.findIndex(x => connKey(x) === connKey(connection));
      if (i < 0) return false;
      patch.connections.splice(i, 1); return true;
    }
    case 'setParam': {
      const node = patch.modules.find(x => x.id === id);
      if (!node) return false;
      (node.parameters ??= {})[p.paramId as string] = p.value as number;
      return true;
    }
    case 'setModAmount': {
      const found = patch.connections.find(x => connKey(x) === connKey(connection));
      if (!found) return false;
      found.amount = p.amount as number; return true;
    }
  }
  return true;
}
