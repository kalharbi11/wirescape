// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2, PatchModule, PatchUi, Connection, Endpoint } from './types';
import { EMPTY_PATCH, connKey } from './types';
import catalogue from '../../public/wirescape_modules.json' with { type: 'json' };

const descriptors = new Map(catalogue.modules.map(m => [m.id, m]));
const outputRoutes = (module: number) => [0, 1].map(bus_channel => ({ module, port: 'audio', bus_channel }));

export type PatchEvent =
  | { t: 'reset' }
  | { t: 'module-added'; id: number }
  | { t: 'module-removed'; id: number }
  | { t: 'connection-added'; c: Connection }
  | { t: 'connection-removed'; c: Connection }
  | { t: 'param'; id: number; paramId: string; value: number }
  | { t: 'config'; id: number }
  | { t: 'mod-amount'; c: Connection; amount: number }
  | { t: 'position'; id: number }
  | { t: 'label'; id: number }
  | { t: 'view' };

type Listener = (e: PatchEvent) => void;

/**
 * The patch document. Pure state — no DOM, no engine. Everything the user
 * edits lives here, and this is what gets written to disk.
 *
 * Unknown fields survive load -> save: each module and connection keeps the
 * object it was parsed from, and unrecognised top-level and `ui` keys are
 * held in `extras`.
 */
export class PatchDoc {
  modules = new Map<number, PatchModule>();
  connections: Connection[] = [];
  ui: PatchUi = { positions: {}, view: { x: 0, y: 0, zoom: 1 }, labels: {} };
  /** Unrecognised top-level keys, replayed on save. */
  private extras: Record<string, unknown> = { outputs: [] };
  private listeners = new Set<Listener>();
  private _dirty = false;
  private sourceHasUi = true;

  get dirty(): boolean { return this._dirty; }
  markClean(): void { this._dirty = false; }
  /** Restores the unsaved state of a document that was put back after a refused load. */
  markDirty(): void { this._dirty = true; }

  on(cb: Listener): () => void { this.listeners.add(cb); return () => this.listeners.delete(cb); }
  private emit(e: PatchEvent): void {
    if (e.t !== 'view') this._dirty = true;
    for (const l of [...this.listeners]) l(e);
  }

  // ---- lifecycle --------------------------------------------------------

  static from(patch: PatchV2): PatchDoc {
    const d = new PatchDoc();
    d.replace(patch);
    d._dirty = false;
    return d;
  }

  replace(patch: PatchV2): void {
    // Validate the ingress envelope before clearing the current editable document.
    // Legacy Python/visual formats are inspection-only, never native migrations.
    if (!patch || patch.format !== 'wirescape.native.patch' ||
        !Number.isInteger(patch.version) || ![1, 2].includes(patch.version)) {
      throw new Error('Unsupported patch format or version; the current patch was kept.');
    }
    if ((patch.modules !== undefined && !Array.isArray(patch.modules)) ||
        (patch.connections !== undefined && !Array.isArray(patch.connections))) {
      throw new Error('Invalid patch containers; the current patch was kept.');
    }
    const object = (value: unknown): boolean => !!value && typeof value === 'object' && !Array.isArray(value);
    if (patch.ui !== undefined && (!object(patch.ui) ||
        ['positions', 'view', 'labels', 'cableColors'].some(key => patch.ui?.[key] !== undefined && !object(patch.ui[key])))) {
      throw new Error('Invalid patch UI containers; the current patch was kept.');
    }
    const v1 = (patch as { version: number }).version === 1;
    if (v1 && (!Array.isArray(patch.modules) || !Array.isArray(patch.connections))) {
      throw new Error('Invalid v1 patch containers; the current patch was kept.');
    }
    const ids = new Set<number>();
    for (const m of patch.modules ?? []) {
      if (!m || !Number.isSafeInteger(m.id) || m.id <= 0 || ids.has(m.id) ||
          typeof (v1 ? m.type : m.module) !== 'string' ||
          (v1 && m.values != null && typeof m.values !== 'object')) {
        throw new Error('Invalid or duplicate patch module; the current patch was kept.');
      }
      ids.add(m.id);
    }
    for (const c of patch.connections ?? []) {
      const readable = v1 ? c && ['audio', 'control', 'signal', 'modulation', 'event'].includes(c.kind) &&
        Number.isSafeInteger(c.fromNode) && Number(c.fromNode) > 0 && typeof c.fromPort === 'string' &&
        Number.isSafeInteger(c.toNode) && Number(c.toNode) > 0 && typeof c.toPort === 'string' : isConnection(c);
      if (!readable) throw new Error('Invalid patch connection; the current patch was kept.');
    }
    // Mirror the native document's v1 migration, preserving the original envelope.
    if ((patch as { version: number }).version === 1) patch = migrateV1(patch);
    patch = structuredClone(patch);
    const { format: _f, version: _v, modules, connections, ui, ...rest } = patch as PatchV2 & Record<string, unknown>;
    this.extras = rest;
    this.modules.clear();
    for (const m of modules ?? []) {
      this.modules.set(m.id, m);
    }
    this.connections = (connections ?? []).map(c => ({ ...c }));
    this.sourceHasUi = 'ui' in patch;
    this.ui = ui ?? {};
    this.emit({ t: 'reset' });
  }

  toJSON(): PatchV2 {
    // Defaults belong to the engine execution copy (protocol §5). A read/save
    // must not author settings, routes, descriptor versions or UI metadata.
    return structuredClone({
      format: 'wirescape.native.patch',
      version: 2,
      ...this.extras,
      modules: [...this.modules.values()],
      connections: this.connections,
      ...(this.sourceHasUi || Object.keys(this.ui).length ? { ui: this.ui } : {}),
    }) as PatchV2;
  }

  // ---- modules ----------------------------------------------------------

  nextId(): number {
    let n = 1;
    for (const id of this.modules.keys()) if (id >= n) n = id + 1;
    return n;
  }

  addModule(type: string, params: Record<string, number>, x: number, y: number, id = this.nextId()): number {
    const version = descriptors.get(type)?.version;
    this.modules.set(id, { id, module: type, ...(version !== undefined ? { version } : {}), parameters: { ...params } });
    if (type === 'essentials.output') {
      if (Array.isArray(this.extras.outputs)) this.extras.outputs.push(...outputRoutes(id));
    }
    (this.ui.positions ??= {})[String(id)] = [Math.round(x), Math.round(y)];
    this.emit({ t: 'module-added', id });
    return id;
  }

  removeModule(id: number): void {
    if (!this.modules.has(id)) return;
    for (const c of this.connections.filter(c => c.from.module === id || c.to.module === id)) {
      this.disconnect(c);
    }
    this.modules.delete(id);
    if (Array.isArray(this.extras.outputs)) this.extras.outputs = this.extras.outputs.filter(o => o.module !== id);
    if (this.ui.positions) delete this.ui.positions[String(id)];
    if (this.ui.labels) delete this.ui.labels[String(id)];
    this.emit({ t: 'module-removed', id });
  }

  position(id: number): [number, number] { return this.ui.positions?.[String(id)] ?? [0, 0]; }
  setPosition(id: number, x: number, y: number): void {
    (this.ui.positions ??= {})[String(id)] = [Math.round(x), Math.round(y)];
    this.emit({ t: 'position', id });
  }

  cableColor(key: string): string | undefined { return this.ui.cableColors?.[key]; }
  setCableColor(key: string, name: string | null): void {
    const map = { ...(this.ui.cableColors ?? {}) };
    if (name) map[key] = name; else delete map[key];
    this.ui.cableColors = map;
  }

  label(id: number): string | undefined { return this.ui.labels?.[String(id)]; }
  setLabel(id: number, text: string): void {
    const t = text.trim();
    if (t) (this.ui.labels ??= {})[String(id)] = t;
    else if (this.ui.labels) delete this.ui.labels[String(id)];
    this.emit({ t: 'label', id });
  }

  setView(x: number, y: number, zoom: number): void {
    this.ui.view = { ...this.ui.view, x, y, zoom };
    this.emit({ t: 'view' });
  }

  // ---- parameters -------------------------------------------------------

  param(id: number, paramId: string): number | undefined {
    return this.modules.get(id)?.parameters?.[paramId];
  }

  setParam(id: number, paramId: string, value: number): void {
    const m = this.modules.get(id);
    if (!m) return;
    (m.parameters ??= {})[paramId] = value;
    this.emit({ t: 'param', id, paramId, value });
  }

  /** Preserve unrelated native module config keys when selecting MIDI input. */
  setModuleEndpoint(id: number, endpoint: string, systemName?: string): void {
    const m = this.modules.get(id);
    if (!m || m.module !== 'midi.in') return;
    const current = m.config;
    const config = current && typeof current === 'object' && !Array.isArray(current)
      ? current as Record<string, unknown> : {};
    const { system_name: _drop, ...rest } = config;
    m.config = systemName === undefined ? { ...rest, endpoint }
      : { ...rest, endpoint, system_name: systemName };
    this.emit({ t: 'config', id });
  }

  /** The explicit `config.rows` of a `midi.cc_to_cv` module (rows beyond them keep their CC n defaults). */
  ccRows(id: number): Record<string, unknown>[] {
    const m = this.modules.get(id);
    const rows = m?.module === 'midi.cc_to_cv' && m.config && typeof m.config === 'object'
      ? (m.config as { rows?: unknown }).rows : undefined;
    return Array.isArray(rows) ? rows.filter((r): r is Record<string, unknown> => !!r && typeof r === 'object') : [];
  }

  /** Replace a module's whole `config` (absent when `undefined`); Learn's rows and their inverse. */
  setConfig(id: number, config: unknown): void {
    const m = this.modules.get(id);
    if (!m) return;
    if (config === undefined) delete m.config;
    else m.config = structuredClone(config) as typeof m.config;
    this.emit({ t: 'config', id });
  }

  // ---- connections ------------------------------------------------------

  has(c: Connection): boolean {
    const k = connKey(c);
    return this.connections.some(x => connKey(x) === k);
  }

  connect(c: Connection): void {
    if (this.has(c)) return;
    this.connections.push(c);
    this.emit({ t: 'connection-added', c });
  }

  disconnect(c: Connection): void {
    const k = connKey(c);
    const i = this.connections.findIndex(x => connKey(x) === k);
    if (i < 0) return;
    const [gone] = this.connections.splice(i, 1);
    this.emit({ t: 'connection-removed', c: gone! });
  }

  setModAmount(c: Connection, amount: number): void {
    const k = connKey(c);
    const found = this.connections.find(x => connKey(x) === k);
    if (!found) return;
    found.amount = Math.max(-1, Math.min(1, amount));
    this.emit({ t: 'mod-amount', c: found, amount: found.amount });
  }

  /** Signal/event cables arriving at a given input port. */
  cablesInto(module: number, port: string): Connection[] {
    return this.connections.filter(c => c.kind !== 'modulation' && c.to.module === module && c.to.port === port);
  }

  /** Modulation cables arriving at a given parameter. */
  modsInto(module: number, paramId: string): Connection[] {
    return this.connections.filter(c => c.kind === 'modulation' && c.to.module === module && c.to.port === paramId);
  }

  cablesFrom(module: number, port: string): Connection[] {
    return this.connections.filter(c => c.from.module === module && c.from.port === port);
  }

  /** Cables that loop back upstream, which get a feedback marker. */
  feedbackKeys(): Set<string> {
    const adj = new Map<number, Set<number>>();
    for (const c of this.connections) {
      if (!adj.has(c.from.module)) adj.set(c.from.module, new Set());
      adj.get(c.from.module)!.add(c.to.module);
    }
    const marks = new Set<string>();
    for (const c of this.connections) {
      if (c.from.module === c.to.module) { marks.add(connKey(c)); continue; }
      // A cable is "feedback" when its target can already reach its source.
      if (reaches(adj, c.to.module, c.from.module, new Set())) marks.add(connKey(c));
    }
    return marks;
  }
}

function reaches(adj: Map<number, Set<number>>, from: number, target: number, seen: Set<number>): boolean {
  if (from === target) return true;
  if (seen.has(from)) return false;
  seen.add(from);
  for (const n of adj.get(from) ?? []) if (reaches(adj, n, target, seen)) return true;
  return false;
}

function isConnection(c: unknown): c is Connection {
  const x = c as Connection;
  return !!x && typeof x === 'object'
    && (x.kind === 'signal' || x.kind === 'modulation' || x.kind === 'event')
    && isEndpoint(x.from) && isEndpoint(x.to);
}
function isEndpoint(e: unknown): e is Endpoint {
  const x = e as Endpoint;
  return !!x && typeof x === 'object' && Number.isSafeInteger(x.module) && x.module > 0 && typeof x.port === 'string';
}
export const EMPTY = EMPTY_PATCH;

function migrateV1(source: PatchV2): PatchV2 {
  const original = structuredClone(source);
  const modules = source.modules.map(m => {
    const type = String(m.type);
    const descriptor = descriptors.get(type);
    const mapping = (descriptor?.legacy_parameters ?? {}) as Record<string, number>;
    const values = (m.values ?? {}) as Record<string, number>;
    const parameters = Object.fromEntries(Object.entries(mapping)
      .filter(([, legacy]) => String(legacy) in values).map(([id, legacy]) => [id, values[String(legacy)]]));
    // Preserve colliding v1 fields in the same envelope as native patch_document.
    const { type: _t, values: _v, state: _s, inactive: _i, channel_seed: _c, ...rest } = m;
    return { ...rest, module: type, ...(descriptor ? { version: descriptor.version } : {}),
      config: {}, parameters, preserved: structuredClone(m), ...(!descriptor ? { inactive: true } : {}) };
  });
  const connections = source.connections.map(c => {
    const { fromNode, fromPort, toNode, toPort, ...rest } = c;
    const kind = (c.kind as string) === 'audio' || (c.kind as string) === 'control' ? 'signal' : c.kind;
    if (kind === 'signal' && kind !== c.kind) {
      delete rest.amount; delete rest.feedback; delete rest.stereo_pair;
    }
    return { ...rest, kind, from: { module: Number(fromNode), port: String(fromPort) },
      to: { module: Number(toNode), port: String(toPort) } };
  });
  return { ...source, version: 2, modules, connections, preserved: original,
    // The native migration selects output port zero (audio for the existing examples).
    outputs: modules.filter(m => m.id === source.outputNode).flatMap(m => outputRoutes(m.id)),
    host_mappings: [], devices: [] };
}
