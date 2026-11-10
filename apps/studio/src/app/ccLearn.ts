// SPDX-License-Identifier: GPL-3.0-only
/**
 * MIDI Learn commit and Forget (#287, PLAN D8, ENGINE_PROTOCOL §4/§5).
 *
 * The engine only observes a moved CC. The UI owns the document: it writes the
 * `midi.cc_to_cv` row into `config.rows`, adds a CC module (and a MIDI In to feed
 * it) when no free row exists, and cables the row's output onto the learned
 * parameter. The result goes to the engine as ONE live replan of the document
 * (`loadGraph`, as a MIDI input change does) rather than as separate
 * addModule / ccMap.set / connect edits, so a refusal of any part leaves the
 * engine and the document exactly as they were instead of a half-learned row.
 * Rows beyond `config.rows` keep their implicit CC n defaults, so a slot past the
 * end is reached by writing those defaults explicitly (no holes) and a wired
 * implicit output never changes meaning.
 */
import type { PatchDoc } from '../patch/PatchDoc';
import type { Connection, PatchV2 } from '../patch/types';

export const CC_TYPE = 'midi.cc_to_cv';
export const MAX_CC_ROWS = 8;
const MIDI_IN_TYPE = 'midi.in';
/** World-space offsets for modules Learn has to add, left of the learned knob's module. */
const CC_DX = 380, SOURCE_DX = 700;

export interface LearnTarget { module: number; param: string }
export interface LearnCapture { channel: number; controller: number }

export interface LearnServices {
  doc: PatchDoc;
  allocateId(): number;
  /** Send the edited document as one live replan; `previous` is restored if the engine refuses it. */
  reload(previous: PatchV2): void;
}

export interface LearnResult { module: number; row: number; created: boolean; notes: string[] }

const outPort = (row: number): string => `cc${row + 1}`;
const rowOf = (port: string): number => /^cc[1-8]$/.test(port) ? Number(port.slice(2)) - 1 : -1;

/** Modulation cables from a CC module's row outputs onto the target parameter. */
export function learnedCables(doc: PatchDoc, t: LearnTarget): Connection[] {
  return doc.connections.filter(c => c.kind === 'modulation' && c.to.module === t.module && c.to.port === t.param
    && doc.modules.get(c.from.module)?.module === CC_TYPE && rowOf(c.from.port) >= 0);
}

/** First CC module with a row past its explicit rows whose output is not cabled yet. */
function freeSlot(doc: PatchDoc): { module: number; row: number } | null {
  const ids = [...doc.modules.values()].filter(m => m.module === CC_TYPE).map(m => m.id).sort((a, b) => a - b);
  for (const id of ids) {
    for (let row = doc.ccRows(id).length; row < MAX_CC_ROWS; row++) {
      if (!doc.cablesFrom(id, outPort(row)).length) return { module: id, row };
    }
  }
  return null;
}

export function commitLearn(svc: LearnServices, target: LearnTarget, capture: LearnCapture,
  anyChannel: boolean): LearnResult {
  const { doc } = svc;
  if (!doc.modules.has(target.module)) throw new Error('The learned parameter no longer exists.');
  const previous = doc.toJSON();
  const notes: string[] = [];
  try {
    const existing = learnedCables(doc, target)[0];
    let slot = existing ? { module: existing.from.module, row: rowOf(existing.from.port) } : freeSlot(doc);
    let created = false;
    if (!slot) {
      const [tx, ty] = doc.position(target.module);
      const ccId = svc.allocateId();
      doc.addModule(CC_TYPE, {}, tx - CC_DX, ty, ccId);
      created = true;
      let source = [...doc.modules.values()].find(m => m.module === MIDI_IN_TYPE)?.id;
      if (source === undefined) {
        source = svc.allocateId();
        doc.addModule(MIDI_IN_TYPE, {}, tx - SOURCE_DX, ty, source);
        notes.push('MIDI In added');
      }
      doc.connect({ kind: 'event', from: { module: source, port: 'midi' }, to: { module: ccId, port: 'midi' } });
      slot = { module: ccId, row: 0 };
    } else if (!doc.cablesInto(slot.module, 'midi').length) {
      notes.push('its CC to CV has no MIDI input connected yet');
    }

    writeRow(doc, slot.module, slot.row, { channel: anyChannel ? 0 : capture.channel, controller: capture.controller });
    doc.connect({ kind: 'modulation', from: { module: slot.module, port: outPort(slot.row) },
      to: { module: target.module, port: target.param }, amount: 1 });
    svc.reload(previous);
    return { module: slot.module, row: slot.row, created, notes };
  } catch (err) {
    doc.replace(previous);
    throw err;
  }
}

/** Set one row, writing implicit-default rows first so there is no hole. */
function writeRow(doc: PatchDoc, module: number, row: number, learned: { channel: number; controller: number }): void {
  const rows = doc.ccRows(module).map(r => ({ ...r }));
  while (rows.length < row) rows.push({ channel: 0, controller: rows.length + 1 });
  const merged: Record<string, unknown> = row < rows.length ? { ...rows[row], ...learned } : { ...learned };
  // Fourteen-bit rows need an MSB controller 0..31; a learned higher controller is a plain 7-bit CC.
  if (merged.fourteen_bit && learned.controller > 31) merged.fourteen_bit = false;
  rows[row] = merged;
  // The CC n label on the module follows the learned controller (ui.ccMap is a display hint only).
  const ui = doc.ui as Record<string, unknown>;
  const labels = (ui.ccMap ?? {}) as Record<string, Record<string, number>>;
  ui.ccMap = { ...labels, [String(module)]: { ...(labels[String(module)] ?? {}), [outPort(row)]: learned.controller } };
  doc.setCcRows(module, rows);
}

/**
 * Remove the CC cables onto a parameter. A row this frees at the end of its module's
 * rows is dropped again (back to its implicit default) so re-learning does not use
 * up rows; any other row, and every unrelated cable, stays. One live replan.
 */
export function forgetLearn(svc: LearnServices, target: LearnTarget): number {
  const { doc } = svc;
  const cables = learnedCables(doc, target);
  if (!cables.length) return 0;
  const previous = doc.toJSON();
  for (const c of cables) doc.disconnect(c);
  for (const c of [...cables].sort((a, b) => rowOf(b.from.port) - rowOf(a.from.port))) {
    const module = c.from.module, row = rowOf(c.from.port);
    const rows = doc.ccRows(module).map(r => ({ ...r }));
    if (row === rows.length - 1 && rows.length > 1 && !doc.cablesFrom(module, c.from.port).length) {
      rows.pop();
      doc.setCcRows(module, rows);
    }
  }
  svc.reload(previous);
  return cables.length;
}
