// SPDX-License-Identifier: GPL-3.0-only
import { el } from './dom';
import { Modal } from './Modal';
import type { RawReading } from './oscCapture';

/** The authored osc.in schema. Row ids are positional (row1 … row8). */
export interface OscRow {
  pattern: string; argument: number; kind: 'cv' | 'gate' | 'trigger';
  input_min: number; input_max: number; output_min: number; output_max: number;
  smoothing_ms: number; timeout_ms: number; stale: 'hold' | 'zero';
  /** Optional (#612): absent means enabled; writers emit it only when false. */
  enabled?: boolean;
}
export interface OscConfig { version?: number; port: number; lan: boolean; rows: OscRow[] }

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
  if (source.version === 1) config.version = 1; // Preserve an explicitly authored v1.
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

const outputMode = (row: Pick<OscRow, 'output_min' | 'output_max'>): string =>
  row.output_min === 0 && row.output_max === 1 ? 'unipolar' :
  row.output_min === -1 && row.output_max === 1 ? 'bipolar' : 'custom';

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
  save: (value: OscConfig) => Promise<boolean>,
  readRaw: (row: Pick<OscRow, 'pattern' | 'argument' | 'enabled'>) => RawReading = () =>
    ({ reason: 'Patch OSC In messages → Monitor events to capture.' }),
  subscribe: (refresh: () => void) => () => void = () => () => {}): void {
  const { config, unsupported } = readOscConfig(raw);
  const authored = object(raw);
  const invalidVersion = 'version' in authored && authored.version !== 1;
  const body = modal.body;
  body.replaceChildren();
  const intro = el('p', 'osc-help',
    'OSC is UDP input. LAN requests still require the owner launcher --confirm-lan; this toggle alone never opens the network.');
  const warning = el('p', 'osc-warning');
  warning.dataset.testid = 'osc-unknown-warning';
  if (invalidVersion) warning.textContent = 'Unsupported OSC config version: the engine refuses this patch; its authored data is preserved.';
  else if (unsupported.length) warning.textContent = `ignored by this engine: ${unsupported.join(', ')}. Saving from this editor removes these unsupported config keys only.`;
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
  type RowControls = Record<typeof rowKeys[number], HTMLInputElement | HTMLSelectElement> & {
    enabled: HTMLInputElement; outputMode: HTMLSelectElement;
    refreshLayout: () => void; refreshRaw: () => void;
  };
  const fields: RowControls[] = [];
  let capturedRangeInvalid = false;
  const checkCapturedRanges = () => {
    if (!capturedRangeInvalid) return;
    const bad = fields.findIndex(c => {
      const min = c.input_min.value.trim(), max = c.input_max.value.trim();
      return !min || !max || !Number.isFinite(Number(min)) || !Number.isFinite(Number(max)) ||
        Math.fround(Number(max)) <= Math.fround(Number(min));
    });
    submit.disabled = bad >= 0;
    if (bad >= 0) errors.textContent = `row${bad + 1}: input max must exceed input min.`;
    else {
      capturedRangeInvalid = false;
      if (/^row\d+: input max must exceed input min\.$/.test(errors.textContent ?? '')) errors.textContent = '';
      fields.forEach(c => {
        c.input_min.removeAttribute('aria-invalid');
        c.input_max.removeAttribute('aria-invalid');
      });
    }
  };
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
    const mark = () => {
      group.classList.toggle('osc-row-off', !enabled.checked);
      refreshActive(); refreshExample(); controls.refreshRaw();
    };
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
    const range = el('div', 'osc-range');
    const out = el('div', 'osc-range-output');
    const mode = choices([
      { value: 'unipolar', name: 'Unipolar 0…1' },
      { value: 'bipolar', name: 'Bipolar −1…1' },
      { value: 'custom', name: 'Custom' },
    ], outputMode(row));
    mode.dataset.testid = `osc-row${i + 1}-output-mode`;
    controls.outputMode = mode;
    const modeField = field('Output', mode);
    const outputFields = el('div', 'osc-output-fields');
    outputFields.append(field('Output min', controls.output_min), field('Output max', controls.output_max));
    out.append(modeField, outputFields);
    const live = el('div', 'osc-range-capture');
    const raw = el('span', 'osc-raw', 'raw —');
    raw.dataset.testid = `osc-row${i + 1}-raw`;
    const reason = el('small', 'osc-capture-reason');
    reason.dataset.testid = `osc-row${i + 1}-capture-reason`;
    const setMax = el('button', 'btn', 'Set max') as HTMLButtonElement;
    const setMin = el('button', 'btn', 'Set min') as HTMLButtonElement;
    setMax.type = setMin.type = 'button';
    setMax.dataset.testid = `osc-row${i + 1}-set-max`;
    setMin.dataset.testid = `osc-row${i + 1}-set-min`;
    live.append(raw, setMax, setMin, reason);
    const preview = el('p', 'osc-range-preview');
    const advanced = el('details', 'osc-range-advanced') as HTMLDetailsElement;
    const summary = el('summary', '', 'Advanced');
    advanced.append(summary, field('Argument index (0–31)', controls.argument),
      field('Smoothing (ms)', controls.smoothing_ms),
      field('Timeout (ms; 0 disables)', controls.timeout_ms),
      field('On timeout', controls.stale));
    range.append(field('Raw min', controls.input_min), field('Raw max', controls.input_max),
      out, live, preview, advanced);
    const refreshPreview = () => {
      preview.textContent = `${controls.input_min.value} → ${controls.output_min.value}, ${controls.input_max.value} → ${controls.output_max.value} · outside is clamped · smoothed ${controls.smoothing_ms.value} ms`;
    };
    const captureRow = (): Pick<OscRow, 'pattern' | 'argument' | 'enabled'> => ({
      pattern: pattern.value, argument: Number(controls.argument.value),
      ...(enabled.checked ? {} : { enabled: false }),
    });
    controls.refreshRaw = () => {
      const reading = readRaw(captureRow());
      raw.textContent = reading.value === undefined ? 'raw —' : `raw ${reading.value}`;
      setMax.disabled = setMin.disabled = reading.value === undefined;
      reason.textContent = reading.reason ?? '';
      setMax.title = setMin.title = reading.reason ?? '';
    };
    const setFromMonitor = (control: HTMLInputElement | HTMLSelectElement) => {
      const reading = readRaw(captureRow());
      if (reading.value === undefined) { controls.refreshRaw(); return; }
      control.value = String(reading.value);
      refreshPreview();
      capturedRangeInvalid = true;
      checkCapturedRanges();
      controls.refreshRaw();
    };
    setMax.addEventListener('click', () => setFromMonitor(controls.input_max));
    setMin.addEventListener('click', () => setFromMonitor(controls.input_min));
    mode.addEventListener('change', () => {
      if (mode.value !== 'custom') {
        controls.output_min.value = mode.value === 'bipolar' ? '-1' : '0';
        controls.output_max.value = '1';
      }
      controls.refreshLayout();
    });
    controls.refreshLayout = () => {
      const isCv = controls.kind.value === 'cv';
      modeField.hidden = !isCv;
      outputFields.hidden = isCv && mode.value !== 'custom';
      live.hidden = preview.hidden = !isCv;
      advanced.open = !isCv;
      summary.hidden = !isCv;
      refreshPreview();
      controls.refreshRaw();
    };
    controls.kind.addEventListener('change', controls.refreshLayout);
    for (const control of [pattern, controls.argument, controls.input_min, controls.input_max,
      controls.output_min, controls.output_max, controls.smoothing_ms]) {
      control.addEventListener('input', () => {
        refreshPreview(); controls.refreshRaw();
        checkCapturedRanges();
      });
    }
    group.append(toggle, field('Address pattern (* allowed)', pattern), field('Kind', controls.kind), range);
    controls.refreshLayout();
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
        controls.outputMode.value = outputMode(next);
        controls.refreshLayout();
      });
      if (resetConnection) {
        const defaults = picoDefaults();
        port.value = String(defaults.port);
        lan.checked = defaults.lan;
      }
      refreshExample();
      refreshActive();
      capturedRangeInvalid = false;
      submit.disabled = false;
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
    el('p', '', 'Patch OSC In messages → Monitor events, move a control, then use Set max / Set min (or type its raw range).'));

  const actions = el('div', 'osc-actions');
  const cancel = el('button', 'btn', 'Cancel') as HTMLButtonElement;
  cancel.type = 'button'; cancel.addEventListener('click', () => modal.close());
  const submit = el('button', 'btn primary', 'Save and apply') as HTMLButtonElement;
  submit.type = 'button'; submit.dataset.testid = 'osc-editor-save';
  submit.addEventListener('click', async () => {
    errors.textContent = '';
    if (invalidVersion) {
      errors.textContent = 'Unsupported OSC config version; the authored patch is unchanged.'; return;
    }
    const n = Number(port.value);
    if (!port.value || !Number.isInteger(n) || n < 1024 || n > 65535) {
      errors.textContent = 'UDP port must be an integer from 1024 to 65535.'; return;
    }
    const next: OscConfig = { ...(config.version === 1 ? { version: 1 } : {}), port: n, lan: lan.checked, rows: [] };
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
  const stopRaw = subscribe(() => { if (modal.isOpen) fields.forEach(c => c.refreshRaw()); });
  modal.open('OSC In · addresses and listening', stopRaw);
}
