// SPDX-License-Identifier: GPL-3.0-only
import { el } from './dom';
import { Modal } from './Modal';

/** The authored osc.in schema. Row ids are positional (row1 … row8). */
export interface OscRow {
  pattern: string; argument: number; kind: 'cv' | 'gate' | 'trigger';
  input_min: number; input_max: number; output_min: number; output_max: number;
  smoothing_ms: number; timeout_ms: number; stale: 'hold' | 'zero';
  /** Optional (#612): absent means enabled; writers emit it only when false. */
  enabled?: boolean;
}
/** `version: 2` is written only when a row is off (#612): older engines refuse it rather than listen. */
export interface OscConfig { version?: 2; port: number; lan: boolean; rows: OscRow[] }

const defaultRow = (n: number): OscRow => ({
  pattern: `/pico1/knob/${n}`, argument: 0, kind: 'cv',
  input_min: 0, input_max: 1, output_min: 0, output_max: 1,
  smoothing_ms: 5, timeout_ms: 1000, stale: 'hold',
});
export const picoDefaults = (): OscConfig =>
  ({ port: 9000, lan: false, rows: Array.from({ length: 8 }, (_, i) => defaultRow(i + 1)) });

const object = (v: unknown): Record<string, unknown> =>
  v !== null && typeof v === 'object' && !Array.isArray(v) ? v as Record<string, unknown> : {};
const numericKeys = ['argument', 'input_min', 'input_max', 'output_min',
  'output_max', 'smoothing_ms', 'timeout_ms'] as const;
const rowKeys = ['pattern', 'kind', ...numericKeys, 'stale'] as const;

/** Display defaults for omitted rows, without rewriting the source patch on open. */
export function readOscConfig(raw: unknown): { config: OscConfig; unsupported: string[] } {
  const source = object(raw);
  const config = picoDefaults();
  if (typeof source.port === 'number') config.port = source.port;
  if (typeof source.lan === 'boolean') config.lan = source.lan;
  if (Array.isArray(source.rows)) {
    source.rows.slice(0, 8).forEach((value, i) => {
      const row = object(value);
      const base = config.rows[i];
      for (const key of rowKeys) {
        if (key in row) (base as unknown as Record<string, unknown>)[key] = row[key];
      }
      if (row.enabled === false) base.enabled = false;
    });
  }
  const unsupported = Object.keys(source).filter(k => !['version', 'port', 'lan', 'rows'].includes(k));
  if (Array.isArray(source.rows)) source.rows.forEach((v, i) => {
    for (const key of Object.keys(object(v))) if (key !== 'enabled' && !rowKeys.includes(key as typeof rowKeys[number]))
      unsupported.push(`rows[${i}].${key}`);
  });
  return { config, unsupported };
}

const choices = (items: { value: string; name: string }[], selected: string): HTMLSelectElement => {
  const control = el('select', 'input-select') as HTMLSelectElement;
  for (const { value, name } of items) {
    const option = document.createElement('option');
    option.value = value; option.textContent = name; control.append(option);
  }
  control.value = selected;
  return control;
};

/** Render an isolated draft; only Save mutates the patch and initiates a replan. */
export function showOscEditor(modal: Modal, raw: unknown,
  save: (value: OscConfig) => Promise<boolean>): void {
  const { config, unsupported } = readOscConfig(raw);
  const body = modal.body;
  body.replaceChildren();
  const intro = el('p', 'osc-help',
    'OSC is UDP input. LAN requests still require the owner launcher --confirm-lan; this toggle alone never opens the network.');
  const warning = el('p', 'osc-warning');
  warning.dataset.testid = 'osc-unknown-warning';
  if (unsupported.length) warning.textContent = `engine will refuse: ${unsupported.join(', ')}. Saving from this editor removes these unsupported config keys only.`;
  else warning.hidden = true;
  const port = el('input', 'input-number') as HTMLInputElement;
  port.type = 'number'; port.min = '1024'; port.max = '65535'; port.step = '1';
  port.value = String(config.port); port.dataset.testid = 'osc-editor-port';
  const lan = el('input') as HTMLInputElement;
  lan.type = 'checkbox'; lan.checked = config.lan; lan.dataset.testid = 'osc-editor-lan';
  const portRow = el('label', 'osc-field');
  portRow.append(el('span', '', 'UDP port'), port);
  const lanRow = el('label', 'osc-lan');
  lanRow.append(lan, el('span', '', 'Allow other devices on my network (LAN)'));
  const presets = el('div', 'osc-presets');
  presets.append(el('strong', '', 'Presets'));
  const rows = el('div', 'osc-rows');
  const errors = el('p', 'osc-error'); errors.dataset.testid = 'osc-editor-errors';
  errors.setAttribute('role', 'alert');
  type RowControls = Record<typeof rowKeys[number], HTMLInputElement | HTMLSelectElement> & { enabled: HTMLInputElement };
  const fields: RowControls[] = [];
  const activeCount = el('p', 'osc-active');
  activeCount.dataset.testid = 'osc-editor-active';
  const refreshActive = () => {
    activeCount.textContent = `${fields.filter(c => c.enabled.checked).length} of 8 rows active`;
  };

  const field = (label: string, control: HTMLInputElement | HTMLSelectElement): HTMLElement => {
    const holder = el('label', 'osc-field');
    holder.append(el('span', '', label), control);
    return holder;
  };
  for (let i = 0; i < 8; i++) {
    const row = config.rows[i];
    const group = el('fieldset', 'osc-row');
    group.append(el('legend', '', `row${i + 1}`));
    const controls = {} as RowControls;
    const enabled = el('input') as HTMLInputElement;
    enabled.type = 'checkbox'; enabled.checked = row.enabled !== false;
    enabled.dataset.testid = `osc-row${i + 1}-enabled`;
    controls.enabled = enabled;
    const toggle = el('label', 'osc-lan');
    toggle.append(enabled, el('span', '', 'Listen (row on)'));
    const mark = () => { group.classList.toggle('osc-row-off', !enabled.checked); refreshActive(); refreshExample(); };
    enabled.addEventListener('change', mark);
    group.classList.toggle('osc-row-off', !enabled.checked);
    const pattern = el('input', 'input-text') as HTMLInputElement;
    pattern.type = 'text'; pattern.value = String(row.pattern); pattern.placeholder = `/pico1/knob/${i + 1}`;
    pattern.dataset.testid = `osc-row${i + 1}-pattern`;
    controls.pattern = pattern;
    controls.kind = choices([{ value: 'cv', name: 'CV' }, { value: 'gate', name: 'Gate' },
      { value: 'trigger', name: 'Trigger' }], row.kind);
    controls.kind.dataset.testid = `osc-row${i + 1}-kind`;
    controls.stale = choices([{ value: 'hold', name: 'Hold' }, { value: 'zero', name: 'Zero' }], row.stale);
    controls.stale.dataset.testid = `osc-row${i + 1}-stale`;
    for (const key of numericKeys) {
      const input = el('input', 'input-number') as HTMLInputElement;
      input.type = 'number'; input.step = key === 'argument' ? '1' : 'any';
      input.value = String(row[key]); input.dataset.testid = `osc-row${i + 1}-${key}`;
      controls[key] = input;
    }
    group.append(toggle, field('Address pattern (* allowed)', pattern), field('Kind', controls.kind),
      field('Argument index (0–31)', controls.argument),
      field('Input min', controls.input_min), field('Input max', controls.input_max),
      field('Output min', controls.output_min), field('Output max', controls.output_max),
      field('Smoothing (ms)', controls.smoothing_ms),
      field('Timeout (ms; 0 disables)', controls.timeout_ms), field('On timeout', controls.stale));
    fields.push(controls);
    rows.append(group);
  }

  const preset = (name: string, id: string, make: (i: number) => OscRow,
    resetConnection = false) => {
    const button = el('button', 'btn', name) as HTMLButtonElement;
    button.type = 'button'; button.dataset.testid = id;
    button.addEventListener('click', () => {
      fields.forEach((controls, i) => {
        const next = make(i);
        for (const key of rowKeys) controls[key].value = String(next[key]);
        controls.enabled.checked = next.enabled !== false;
        controls.enabled.closest('fieldset')?.classList.toggle('osc-row-off', next.enabled === false);
      });
      if (resetConnection) {
        const defaults = picoDefaults();
        port.value = String(defaults.port);
        lan.checked = defaults.lan;
      }
      refreshExample();
      refreshActive();
      errors.textContent = '';
    });
    presets.append(button);
  };
  preset('Pico knobs 1–8', 'osc-preset-pico', i => defaultRow(i + 1));
  preset('TouchOSC faders /1/fader1–8', 'osc-preset-touchosc',
    i => ({ ...defaultRow(i + 1), pattern: `/1/fader${i + 1}` }));
  preset('Reset to Pico defaults', 'osc-preset-reset', i => defaultRow(i + 1), true);
  // Clear keeps every address so a row can be switched back on (#612).
  preset('Clear (all rows off)', 'osc-preset-clear', i => {
    const keep = {} as Record<string, string>;
    for (const key of rowKeys) keep[key] = fields[i][key].value;
    return { ...(keep as unknown as OscRow), enabled: false };
  });

  const example = el('code', 'osc-example');
  example.dataset.testid = 'osc-send-example';
  const refreshExample = () => {
    const pattern = fields.find(c => c.enabled.checked && c.pattern.value.startsWith('/'))?.pattern.value ?? '/pico1/knob/1';
    const address = pattern.replaceAll('*', '1');
    example.textContent = `send ${address} 0.5 to 127.0.0.1:${port.value || '9000'} (UDP)`;
  };
  refreshExample();
  refreshActive();
  port.addEventListener('input', refreshExample);
  fields.forEach(c => c.pattern.addEventListener('input', refreshExample));
  const copy = el('button', 'btn', 'Copy example') as HTMLButtonElement;
  copy.type = 'button'; copy.dataset.testid = 'osc-copy-example';
  copy.addEventListener('click', () => {
    void navigator.clipboard?.writeText(example.textContent ?? '').catch(() => {
      errors.textContent = 'Clipboard unavailable. Select the example to copy it.';
    });
  });
  const help = el('section', 'osc-help');
  help.append(el('h3', '', 'How to send'), example, copy,
    el('p', '', 'Connect the OSC In messages jack to a Monitor events jack to inspect traffic (message list arrives in #604).'));

  const actions = el('div', 'osc-actions');
  const cancel = el('button', 'btn', 'Cancel') as HTMLButtonElement;
  cancel.type = 'button'; cancel.addEventListener('click', () => modal.close());
  const submit = el('button', 'btn primary', 'Save and apply') as HTMLButtonElement;
  submit.type = 'button'; submit.dataset.testid = 'osc-editor-save';
  submit.addEventListener('click', async () => {
    errors.textContent = '';
    const n = Number(port.value);
    if (!port.value || !Number.isInteger(n) || n < 1024 || n > 65535) {
      errors.textContent = 'UDP port must be an integer from 1024 to 65535.'; return;
    }
    let next: OscConfig = { port: n, lan: lan.checked, rows: [] };
    for (const [i, c] of fields.entries()) {
      const pattern = c.pattern.value;
      if (!pattern.startsWith('/') || new TextEncoder().encode(pattern).length >= 256 || pattern.includes('\0')) {
        errors.textContent = `row${i + 1}: address must start with / and fit in 255 UTF-8 bytes.`; return;
      }
      const values = {} as Record<typeof numericKeys[number], number>;
      for (const key of numericKeys) {
        const input = c[key] as HTMLInputElement, value = Number(input.value);
        if (!input.value.trim() || !Number.isFinite(value) ||
            (key === 'argument' && (!Number.isInteger(value) || value < 0 || value > 31)) ||
            (key === 'smoothing_ms' && (value < 0 || value > 2000)) ||
            (key === 'timeout_ms' && (value < 0 || value > 3600000))) {
          errors.textContent = `row${i + 1}: invalid ${key.replaceAll('_', ' ')}.`;
          input.setAttribute('aria-invalid', 'true'); return;
        }
        input.removeAttribute('aria-invalid');
        values[key] = value;
      }
      if (Math.fround(values.input_max) <= Math.fround(values.input_min)) {
        errors.textContent = `row${i + 1}: input max must exceed input min.`; return;
      }
      next.rows.push({ pattern, kind: c.kind.value as OscRow['kind'],
        stale: c.stale.value as OscRow['stale'], ...values, ...(c.enabled.checked ? {} : { enabled: false }) });
    }
    if (next.rows.some(r => r.enabled === false)) next = { version: 2, ...next };
    submit.disabled = true;
    try {
      if (await save(next)) modal.close();
      else errors.textContent = 'Engine refused the OSC configuration; the previous patch is unchanged.';
    } catch (e) {
      errors.textContent = e instanceof Error ? e.message : 'Could not apply the OSC configuration.';
    } finally { submit.disabled = false; }
  });
  actions.append(cancel, submit);
  body.append(intro, warning, portRow, lanRow, presets, activeCount, rows, errors, help, actions);
  modal.open('OSC In · addresses and listening');
}
