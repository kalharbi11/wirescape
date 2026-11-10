// SPDX-License-Identifier: GPL-3.0-only
/**
 * MIDI Learn commit and Forget (#287, PLAN D8, ENGINE_PROTOCOL §4/§5/§6).
 *
 * The engine only observes a moved CC. The UI owns the document: it writes the
 * `midi.cc_to_cv` row into `config.rows`, adds a CC module (and a MIDI In to feed
 * it) when no usable row exists, and cables the row's output onto the learned
 * parameter. The App sends the result to the engine as ONE live replan of the
 * document (`loadGraph`, as a MIDI input change does) rather than as separate
 * addModule / ccMap.set / connect edits, so a refusal of any part leaves the
 * engine on the previous graph instead of a half-learned row.
 *
 * A row only counts as learned from the device that moved: the converter it sits
 * on must be fed by a `midi.in` whose endpoint takes that device (`all_enabled`,
 * or the same `name` + `system_name`). A converter with no feed gets one in the
 * same edit, so a learned row is never inert. Rows beyond `config.rows` keep their
 * implicit CC n defaults, so a slot past the end is reached by writing those
 * defaults explicitly (no holes) and a wired implicit output never changes meaning.
 *
 * Edits retain inverses for local construction errors. App serializes the
 * atomic load and restores its pre-Learn snapshot on a later refusal.
 */
import type { Device } from '../engine/EngineClient';
import type { PatchDoc } from '../patch/PatchDoc';
import type { Connection } from '../patch/types';

export const CC_TYPE = 'midi.cc_to_cv';
export const MAX_CC_ROWS = 8;
const MIDI_IN_TYPE = 'midi.in';
const ALL_ENABLED = 'all_enabled';
/** World-space offsets for modules Learn has to add, left of the module they feed. */
const CC_DX = 380, SOURCE_DX = 320;

export interface LearnTarget { module: number; param: string }
export interface LearnCapture { deviceId: string; channel: number; controller: number }

export interface LearnServices {
  doc: PatchDoc;
  allocateId(): number;
  /** The engine's current MIDI input list (`status.midi.devices`). */
  devices(): Device[];
}

/** Take this change back out of the document, leaving any later edit in place. */
export interface Revertible { revert(): void }
export interface LearnResult extends Revertible { module: number; row: number; created: boolean; notes: string[] }
export interface ForgetResult extends Revertible { count: number }

interface Endpoint { name: string; systemName: string }

const outPort = (row: number): string => `cc${row + 1}`;
const rowOf = (port: string): number => /^cc[1-8]$/.test(port) ? Number(port.slice(2)) - 1 : -1;
const record = (v: unknown): Record<string, unknown> =>
  v && typeof v === 'object' && !Array.isArray(v) ? v as Record<string, unknown> : {};
const same = (a: unknown, b: unknown): boolean => JSON.stringify(a) === JSON.stringify(b);

/** Document edits with guarded inverses, applied newest first by `revert`. */
class Edit implements Revertible {
  private readonly undo: (() => void)[] = [];
  constructor(private readonly doc: PatchDoc) {}

  add(type: string, x: number, y: number, id: number, config?: Record<string, unknown>): void {
    this.doc.addModule(type, {}, x, y, id);
    if (config) this.doc.setConfig(id, config);
    this.undo.push(() => this.doc.removeModule(id));
  }
  connect(c: Connection): void {
    if (this.doc.has(c)) return;
    this.doc.connect(c);
    this.undo.push(() => this.doc.disconnect(c));
  }
  disconnect(c: Connection): void {
    if (!this.doc.has(c)) return;
    this.doc.disconnect(c);
    this.undo.push(() => {
      if (this.doc.modules.has(c.from.module) && this.doc.modules.has(c.to.module)) this.doc.connect(c);
    });
  }
  /** Replace a CC module's rows (keeping its other config) and, optionally, one `CC n` label. */
  rows(id: number, rows: Record<string, unknown>[], label?: { port: string; controller: number }): void {
    const m = this.doc.modules.get(id);
    if (!m) return;
    const before = m.config === undefined ? undefined : structuredClone(m.config);
    const ui = this.doc.ui as Record<string, unknown>;
    const hadMap = 'ccMap' in ui;
    const labels = record(ui.ccMap);
    const beforeLabels = labels[String(id)];
    // The CC n label on the module follows the learned controller (ui.ccMap is a display hint only).
    if (label) ui.ccMap = { ...labels, [String(id)]: { ...record(beforeLabels), [label.port]: label.controller } };
    const next = { ...record(m.config), rows: structuredClone(rows) };
    this.doc.setConfig(id, next);
    this.undo.push(() => {
      const now = this.doc.modules.get(id);
      if (!now || !same(now.config, next)) return; // edited since: keep the newer rows
      if (label) {
        const u = this.doc.ui as Record<string, unknown>;
        const map = { ...record(u.ccMap) };
        if (beforeLabels === undefined) delete map[String(id)]; else map[String(id)] = beforeLabels;
        if (!hadMap && !Object.keys(map).length) delete u.ccMap; else u.ccMap = map;
      }
      this.doc.setConfig(id, before);
    });
  }
  revert(): void { for (const u of this.undo.splice(0).reverse()) u(); }
}

/** Modulation cables from a CC module's row outputs onto the target parameter. */
export function learnedCables(doc: PatchDoc, t: LearnTarget): Connection[] {
  return doc.connections.filter(c => c.kind === 'modulation' && c.to.module === t.module && c.to.port === t.param
    && doc.modules.get(c.from.module)?.module === CC_TYPE && rowOf(c.from.port) >= 0);
}

/** `name` + `system_name` of an enabled engine MIDI input, or null when it is gone. */
function endpointOf(devices: Device[], id: string): Endpoint | null {
  const d = devices.find(x => x.id === id && x.kind !== 'audio');
  if (!d || d.enabled === false) return null;
  return { name: d.name, systemName: d.system_name ?? (d.id.includes('|') ? d.id.slice(d.id.indexOf('|') + 1) : '') };
}

/** Does this `midi.in` deliver the endpoint's events (ENGINE_PROTOCOL §6)? */
function sourceTakes(doc: PatchDoc, id: number, ep: Endpoint): boolean {
  const m = doc.modules.get(id);
  if (m?.module !== MIDI_IN_TYPE) return false;
  const cfg = record(m.config);
  const endpoint = typeof cfg.endpoint === 'string' ? cfg.endpoint : ALL_ENABLED;
  if (endpoint === ALL_ENABLED) return true;
  return endpoint === ep.name && (typeof cfg.system_name === 'string' ? cfg.system_name : '') === ep.systemName;
}

/** 'none': no MIDI cable in; 'takes': fed by a source that delivers the endpoint; 'other': fed, but not by it. */
function feedOf(doc: PatchDoc, cc: number, ep: Endpoint): 'none' | 'takes' | 'other' {
  const feeds = doc.cablesInto(cc, 'midi');
  if (!feeds.length) return 'none';
  return feeds.some(c => sourceTakes(doc, c.from.module, ep)) ? 'takes' : 'other';
}

function freeRow(doc: PatchDoc, id: number): number {
  for (let row = doc.ccRows(id).length; row < MAX_CC_ROWS; row++) {
    if (!doc.cablesFrom(id, outPort(row)).length) return row;
  }
  return -1;
}

/** First free row on a converter that hears the endpoint, else on one with no feed yet. */
function freeSlot(doc: PatchDoc, ep: Endpoint): { module: number; row: number } | null {
  const ids = [...doc.modules.values()].filter(m => m.module === CC_TYPE).map(m => m.id).sort((a, b) => a - b);
  for (const want of ['takes', 'none'] as const) {
    for (const id of ids) {
      if (feedOf(doc, id, ep) !== want) continue;
      const row = freeRow(doc, id);
      if (row >= 0) return { module: id, row };
    }
  }
  return null;
}

/** Drop a row freed at the end of its module's rows (back to its implicit default). */
function trimRow(doc: PatchDoc, edit: Edit, module: number, port: string): void {
  const rows = doc.ccRows(module);
  if (rowOf(port) === rows.length - 1 && rows.length > 1 && !doc.cablesFrom(module, port).length) {
    edit.rows(module, rows.slice(0, -1).map(r => ({ ...r })));
  }
}

/** Cable a MIDI In that delivers the endpoint into an unfed converter, adding one if none does. */
function feed(svc: LearnServices, edit: Edit, cc: number, ep: Endpoint, notes: string[]): void {
  const { doc } = svc;
  const sources = [...doc.modules.values()].filter(m => sourceTakes(doc, m.id, ep)).map(m => m.id).sort((a, b) => a - b);
  // Prefer a source naming exactly this endpoint over an all-enabled one.
  let source = sources.find(id => typeof record(doc.modules.get(id)?.config).endpoint === 'string'
    && record(doc.modules.get(id)?.config).endpoint !== ALL_ENABLED) ?? sources[0];
  if (source === undefined) {
    const [x, y] = doc.position(cc);
    source = svc.allocateId();
    edit.add(MIDI_IN_TYPE, x - SOURCE_DX, y, source, { endpoint: ep.name, system_name: ep.systemName });
    notes.push(`MIDI In for ${ep.name} added`);
  }
  edit.connect({ kind: 'event', from: { module: source, port: 'midi' }, to: { module: cc, port: 'midi' } });
}

export function commitLearn(svc: LearnServices, target: LearnTarget, capture: LearnCapture,
  anyChannel: boolean): LearnResult {
  const { doc } = svc;
  if (!doc.modules.has(target.module)) throw new Error('The learned parameter no longer exists.');
  const ep = endpointOf(svc.devices(), capture.deviceId);
  if (!ep) throw new Error(`The MIDI device that sent CC ${capture.controller} is no longer connected; nothing was mapped.`);
  const edit = new Edit(doc);
  const notes: string[] = [];
  try {
    // Re-learn in place only when that row is this knob's alone and its converter hears the device.
    const existing = learnedCables(doc, target)[0];
    let slot: { module: number; row: number } | null = null;
    if (existing && feedOf(doc, existing.from.module, ep) !== 'other'
      && doc.cablesFrom(existing.from.module, existing.from.port).length === 1) {
      slot = { module: existing.from.module, row: rowOf(existing.from.port) };
    } else if (existing) {
      edit.disconnect(existing);
      trimRow(doc, edit, existing.from.module, existing.from.port);
    }
    slot ??= freeSlot(doc, ep);
    let created = false;
    if (!slot) {
      const [tx, ty] = doc.position(target.module);
      const ccId = svc.allocateId();
      edit.add(CC_TYPE, tx - CC_DX, ty, ccId);
      created = true;
      slot = { module: ccId, row: 0 };
    }
    if (feedOf(doc, slot.module, ep) === 'none') feed(svc, edit, slot.module, ep, notes);

    writeRow(doc, edit, slot.module, slot.row, { channel: anyChannel ? 0 : capture.channel, controller: capture.controller });
    edit.connect({ kind: 'modulation', from: { module: slot.module, port: outPort(slot.row) },
      to: { module: target.module, port: target.param }, amount: 1 });
    return { module: slot.module, row: slot.row, created, notes, revert: () => edit.revert() };
  } catch (err) {
    edit.revert();
    throw err;
  }
}

/** Set one row, writing implicit-default rows first so there is no hole. */
function writeRow(doc: PatchDoc, edit: Edit, module: number, row: number,
  learned: { channel: number; controller: number }): void {
  const rows = doc.ccRows(module).map(r => ({ ...r }));
  while (rows.length < row) rows.push({ channel: 0, controller: rows.length + 1 });
  const merged: Record<string, unknown> = row < rows.length ? { ...rows[row], ...learned } : { ...learned };
  // Fourteen-bit rows need an MSB controller 0..31; a learned higher controller is a plain 7-bit CC.
  if (merged.fourteen_bit && learned.controller > 31) merged.fourteen_bit = false;
  rows[row] = merged;
  edit.rows(module, rows, { port: outPort(row), controller: learned.controller });
}

/**
 * Remove the CC cables onto a parameter. A row this frees at the end of its module's
 * rows is dropped again (back to its implicit default) so re-learning does not use
 * up rows; a row another knob still uses, and every unrelated cable, stays.
 */
export function forgetLearn(svc: LearnServices, target: LearnTarget): ForgetResult | null {
  const { doc } = svc;
  const cables = learnedCables(doc, target);
  if (!cables.length) return null;
  const edit = new Edit(doc);
  for (const c of cables) edit.disconnect(c);
  for (const c of [...cables].sort((a, b) => rowOf(b.from.port) - rowOf(a.from.port))) {
    trimRow(doc, edit, c.from.module, c.from.port);
  }
  return { count: cables.length, revert: () => edit.revert() };
}
