// SPDX-License-Identifier: GPL-3.0-only
import { el } from './dom';
import { Modal } from './Modal';

/** OSC Out v1: unknown config properties are preserved, including inside rows. */
export interface OscOutRow {
  address: string;
  kind: 'cv' | 'gate';
  deadband: number;
  enabled?: boolean;
  [key: string]: unknown;
}
export interface OscOutConfig {
  version?: number;
  port: number;
  rate_hz: number;
  rows: OscOutRow[];
  notes: { address: string; enabled?: boolean; [key: string]: unknown };
  [key: string]: unknown;
}

const record = (v: unknown): Record<string, unknown> =>
  v !== null && typeof v === 'object' && !Array.isArray(v) ? v as Record<string, unknown> : {};
const defaultRow = (n: number): OscOutRow =>
  ({ address: `/ws/row/${n}`, kind: 'cv', deadband: 0.001 });
const addressOK = (s: string): boolean =>
  s.length >= 2 && s.length <= 63 && /^\/[\x21-\x7e]+$/.test(s) && !/[#*,?\[\]{}]/.test(s);

/** Defaults are for display only. Opening the editor never alters the document. */
export function readOscOutConfig(raw: unknown): OscOutConfig {
  const source = record(raw);
  const rows = Array.isArray(source.rows) ? source.rows : [];
  const notes = record(source.notes);
  return {
    ...source,
    port: typeof source.port === 'number' ? source.port : 9001,
    rate_hz: typeof source.rate_hz === 'number' ? source.rate_hz : 100,
    rows: Array.from({ length: 8 }, (_, i) => {
      const v = record(rows[i]);
      return { ...defaultRow(i + 1), ...v } as OscOutRow;
    }),
    notes: { address: '/ws/note', ...notes },
  };
}

/** Commit only on Save; no network destination can be authored from this editor. */
export function showOscOutEditor(modal: Modal, raw: unknown,
  save: (value: OscOutConfig) => Promise<boolean>): void {
  const config = readOscOutConfig(raw);
  const unsupportedVersion = record(raw).version !== undefined && record(raw).version !== 1;
  const forbiddenDestination = ['host', 'lan'].some(key =>
    Object.prototype.hasOwnProperty.call(record(raw), key));
  const body = modal.body;
  body.replaceChildren();
  const intro = el('p', 'osc-help', 'Sends to this computer (127.0.0.1) only. Each CV/gate row reads channel 0 (FIRST).');
  const hint = el('p', 'osc-help', 'Unknown config properties are preserved when saved.');
  const warning = el('p', 'osc-warning', unsupportedVersion
    ? 'Unsupported OSC Out version; editing is disabled until the patch is migrated.'
    : forbiddenDestination
      ? 'OSC Out v1 does not support host or lan fields; saving is disabled without migration.' : '');
  warning.hidden = !unsupportedVersion && !forbiddenDestination;

  const number = (value: number, min: string, max: string, id: string, step = '1'): HTMLInputElement => {
    const input = el('input', 'input-number') as HTMLInputElement;
    input.type = 'number'; input.min = min; input.max = max; input.step = step;
    input.value = String(value); input.dataset.testid = id;
    return input;
  };
  const field = (caption: string, input: HTMLElement): HTMLElement => {
    const holder = el('label', 'osc-field');
    holder.append(el('span', '', caption), input);
    return holder;
  };
  const text = (value: string, id: string): HTMLInputElement => {
    const input = el('input', 'input-text') as HTMLInputElement;
    input.type = 'text'; input.value = value; input.dataset.testid = id;
    return input;
  };
  const check = (enabled: boolean, id: string): HTMLInputElement => {
    const input = el('input') as HTMLInputElement;
    input.type = 'checkbox'; input.checked = enabled; input.dataset.testid = id;
    return input;
  };
  const port = number(config.port, '1', '65535', 'osc-out-port');
  const rate = number(config.rate_hz, '1', '500', 'osc-out-rate');
  const fields: { address: HTMLInputElement; kind: HTMLSelectElement;
    deadband: HTMLInputElement; enabled: HTMLInputElement }[] = [];
  const rows = el('div', 'osc-rows');
  for (let i = 0; i < 8; i++) {
    const row = config.rows[i];
    const group = el('fieldset', 'osc-row');
    group.append(el('legend', '', `row${i + 1}`));
    const enabled = check(row.enabled !== false, `osc-out-row${i + 1}-enabled`);
    const address = text(String(row.address), `osc-out-row${i + 1}-address`);
    const kind = el('select', 'input-select') as HTMLSelectElement;
    kind.dataset.testid = `osc-out-row${i + 1}-kind`;
    for (const value of ['cv', 'gate']) {
      const option = document.createElement('option');
      option.value = value; option.textContent = value.toUpperCase(); kind.append(option);
    }
    kind.value = row.kind;
    const deadband = number(row.deadband, '0', '1', `osc-out-row${i + 1}-deadband`, 'any');
    const toggle = () => group.classList.toggle('osc-row-off', !enabled.checked);
    enabled.addEventListener('change', toggle); toggle();
    group.append(field('Send', enabled), field('OSC address', address),
      field('Kind', kind), field('CV deadband (0–1)', deadband));
    rows.append(group);
    fields.push({ address, kind, deadband, enabled });
  }
  const noteGroup = el('fieldset', 'osc-row');
  noteGroup.append(el('legend', '', 'notes (event input)'));
  const notesAddress = text(String(config.notes.address), 'osc-out-notes-address');
  const notesEnabled = check(config.notes.enabled === true, 'osc-out-notes-enabled');
  noteGroup.append(field('Send note on/off', notesEnabled), field('OSC address', notesAddress));
  const errors = el('p', 'osc-error');
  errors.dataset.testid = 'osc-out-errors'; errors.setAttribute('role', 'alert');
  const actions = el('div', 'osc-actions');
  const cancel = el('button', 'btn', 'Cancel') as HTMLButtonElement;
  cancel.type = 'button'; cancel.addEventListener('click', () => modal.close());
  const submit = el('button', 'btn primary', 'Save and apply') as HTMLButtonElement;
  submit.type = 'button'; submit.dataset.testid = 'osc-out-save';
  submit.addEventListener('click', async () => {
    errors.textContent = '';
    if (unsupportedVersion) { errors.textContent = 'Unsupported OSC Out config version.'; return; }
    if (forbiddenDestination) {
      errors.textContent = 'OSC Out v1 does not support host or lan configuration fields; the patch is unchanged.';
      return;
    }
    const integer = (input: HTMLInputElement, min: number, max: number): number | null => {
      const n = Number(input.value);
      return input.value.trim() && Number.isInteger(n) && n >= min && n <= max ? n : null;
    };
    const p = integer(port, 1, 65535), hz = integer(rate, 1, 500);
    if (p === null || hz === null) {
      errors.textContent = 'Port must be 1–65535 and send rate must be 1–500 Hz (integers).'; return;
    }
    const nextRows: OscOutRow[] = [];
    for (const [i, c] of fields.entries()) {
      const db = Number(c.deadband.value);
      if (!addressOK(c.address.value)) {
        errors.textContent = `row${i + 1}: address must be 2–63 printable ASCII bytes, start with /, and avoid OSC wildcard characters.`; return;
      }
      if (!['cv', 'gate'].includes(c.kind.value) || !c.deadband.value.trim() ||
          !Number.isFinite(db) || db < 0 || db > 1) {
        errors.textContent = `row${i + 1}: kind or deadband is invalid.`; return;
      }
      const row: OscOutRow = { ...config.rows[i], address: c.address.value,
        kind: c.kind.value as OscOutRow['kind'], deadband: db };
      delete row.enabled;
      if (!c.enabled.checked) row.enabled = false;
      nextRows.push(row);
    }
    if (!addressOK(notesAddress.value)) {
      errors.textContent = 'notes: invalid OSC address (2–63 printable ASCII bytes, no wildcards).'; return;
    }
    const notes = { ...config.notes, address: notesAddress.value };
    delete notes.enabled;
    if (notesEnabled.checked) notes.enabled = true;
    const next: OscOutConfig = { ...config, port: p, rate_hz: hz, rows: nextRows, notes };
    submit.disabled = true;
    try {
      if (await save(next)) modal.close();
      else errors.textContent = 'Engine refused OSC Out configuration; the previous patch is unchanged.';
    } catch (e) {
      errors.textContent = e instanceof Error ? e.message : 'Could not apply OSC Out.';
    } finally { submit.disabled = false; }
  });
  actions.append(cancel, submit);
  body.append(intro, hint, warning, field('Destination UDP port', port),
    field('Send rate per CV row (Hz)', rate), rows, noteGroup, errors, actions);
  modal.open('OSC Out · loopback sender');
}
