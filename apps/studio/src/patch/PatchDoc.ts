// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2, PatchModule, PatchUi, Connection, Endpoint } from './types';
import { EMPTY_PATCH, connKey } from './types';

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
  private extras: Record<string, unknown> = {};
  private listeners = new Set<Listener>();
  private _dirty = false;

  get dirty(): boolean { return this._dirty; }
  markClean(): void { this._dirty = false; }

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
    const { format: _f, version: _v, modules, connections, ui, ...rest } = patch as PatchV2 & Record<string, unknown>;
    this.extras = rest;
    this.modules.clear();
    for (const m of modules ?? []) {
      if (typeof m?.id !== 'number' || typeof m?.module !== 'string') continue;
      this.modules.set(m.id, { ...m, parameters: { ...(m.parameters ?? {}) } });
    }
    this.connections = (connections ?? []).filter(isConnection).map(c => ({ ...c }));
    this.ui = {
      positions: { ...(ui?.positions ?? {}) },
      view: { ...(ui?.view ?? { x: 0, y: 0, zoom: 1 }) },
      labels: { ...(ui?.labels ?? {}) },
      ...stripKnownUi(ui),
    };
    this.emit({ t: 'reset' });
  }

  toJSON(): PatchV2 {
    return {
      format: 'wirescape.native.patch',
      version: 2,
      ...this.extras,
      modules: [...this.modules.values()].map(m => ({ ...m })),
      connections: this.connections.map(c => ({ ...c })),
      ui: {
        ...this.ui,
        positions: { ...this.ui.positions },
        labels: { ...this.ui.labels },
        view: { ...this.ui.view! },
      },
    } as PatchV2;
  }

  // ---- modules ----------------------------------------------------------

  nextId(): number {
    let n = 1;
    for (const id of this.modules.keys()) if (id >= n) n = id + 1;
    return n;
  }

  addModule(type: string, params: Record<string, number>, x: number, y: number, id = this.nextId()): number {
    this.modules.set(id, { id, module: type, parameters: { ...params } });
    this.ui.positions![String(id)] = [Math.round(x), Math.round(y)];
    this.emit({ t: 'module-added', id });
    return id;
  }

  removeModule(id: number): void {
    if (!this.modules.has(id)) return;
    for (const c of this.connections.filter(c => c.from.module === id || c.to.module === id)) {
      this.disconnect(c);
    }
    this.modules.delete(id);
    delete this.ui.positions![String(id)];
    delete this.ui.labels![String(id)];
    this.emit({ t: 'module-removed', id });
  }

  position(id: number): [number, number] { return this.ui.positions?.[String(id)] ?? [0, 0]; }
  setPosition(id: number, x: number, y: number): void {
    this.ui.positions![String(id)] = [Math.round(x), Math.round(y)];
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
    if (t) this.ui.labels![String(id)] = t; else delete this.ui.labels![String(id)];
    this.emit({ t: 'label', id });
  }

  setView(x: number, y: number, zoom: number): void {
    this.ui.view = { x, y, zoom };
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
  return !!x && typeof x === 'object' && typeof x.module === 'number' && typeof x.port === 'string';
}
function stripKnownUi(ui: PatchUi | undefined): Record<string, unknown> {
  if (!ui) return {};
  const { positions: _p, view: _v, labels: _l, ...rest } = ui;
  return rest;
}

export const EMPTY = EMPTY_PATCH;
