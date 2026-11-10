// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleDescriptor, ModuleParam } from '../modules/types';
import type { Device } from '../engine/EngineClient';
import type { Connection } from '../patch/types';
import type { PatchDoc } from '../patch/PatchDoc';
import type { Ticker } from '../runtime/Ticker';
import type { ModuleRegistry } from '../modules/registry';
import { findParam, formatValue } from '../modules/registry';
import { resolveLayout, type PanelOverride, type ResolvedLayout } from './layouts';
import { el } from '../app/dom';
import { Knob } from './widgets/Knob';
import { Segmented } from './widgets/Segmented';
import { Selector } from './widgets/Selector';
import { Toggle } from './widgets/Toggle';
import { NumberBox } from './widgets/NumberBox';
import { Fader } from './widgets/Fader';
import { buildPane, type Pane } from './panes';
import type { GrooveHost } from './widgets/GroovePane';

export interface PanelHost {
  doc: PatchDoc;
  ticker: Ticker;
  registry: ModuleRegistry;
  /** Write a parameter through the engine; rolls back if it refuses. */
  commitParam(moduleId: number, paramId: string, value: number, commit: boolean): void;
  commitModAmount(c: Connection, amount: number, commit: boolean): void;
  listMidiDevices(): Promise<Device[]>;
  setModuleEndpoint(moduleId: number, endpoint: string, systemName?: string): void;
  describeSource(c: Connection): string;
  /** The cable's user-picked colour, as a CSS colour. */
  cableColor(c: Connection): string;
  /** Groove draft/Apply services (#498); absent hosts get no Groove editor. */
  groove?: GrooveHost;
}

interface Widget { root: HTMLElement; refresh(): void; destroy(): void }

/**
 * Builds a module's panel from its descriptor. No module-specific code
 * lives here: a new entry in the catalogue gets a working panel with no
 * change to this file. Displays first, then the hero, primaries in a row,
 * trims and selectors last (DESIGN §7).
 */
export class Panel {
  readonly root: HTMLElement;
  readonly layout: ResolvedLayout;
  /** Elements the card puts in its header (e.g. the KNOBS/GRAPH switch). */
  readonly headerEls: HTMLElement[] = [];
  private widgets = new Map<string, Widget>();
  private panes: Pane[] = [];
  private sections: { row: HTMLElement; label?: HTMLElement; params: string[] }[] = [];

  constructor(
    private readonly host: PanelHost,
    private readonly moduleId: number,
    private readonly d: ModuleDescriptor,
    ov: PanelOverride | undefined,
    private readonly onLayoutChanged?: () => void,
  ) {
    this.layout = resolveLayout(d, ov);
    this.root = el('div', `panel-core shape-${this.layout.shape}`);

    const displays = el('div', 'panel-displays');
    for (const kind of this.layout.panes) {
      const pane = buildPane(kind, {
        moduleId, ticker: host.ticker, descriptor: d, doc: host.doc,
        param: pid => findParam(d, pid),
        getValue: pid => this.value(pid),
        setValue: (pid, v, commit = true) => host.commitParam(moduleId, pid, v, commit),
        options: this.layout.paneOptions[kind] ?? {},
        groove: host.groove,
        layoutChanged: () => requestAnimationFrame(() => { if (this.root.isConnected) this.onLayoutChanged?.(); }),
      });
      if (!pane) continue;
      this.panes.push(pane);
      displays.append(pane.root);
      if (pane.headerEl) this.headerEls.push(pane.headerEl);
    }
    if (this.panes.length) this.root.append(displays);

    const controls = el('div', 'panel-controls');
    for (const s of this.layout.sections) {
      const label = s.label ? el('div', 'section-label', s.label) : undefined;
      if (label) controls.append(label);
      const row = el('div', 'panel-row');
      if (s.columns) row.style.maxWidth = `${s.columns * 92}px`;
      for (const id of s.params) {
        const p = findParam(d, id);
        if (!p) continue;
        const w = this.buildWidget(p, !!s.hero);
        if (w) { this.widgets.set(id, w); row.append(w.root); }
      }
      controls.append(row);
      this.sections.push({ row, label, params: s.params });
    }
    if (d.id === 'midi.in') {
      const row = el('div', 'control-row select-row');
      const sel = el('select', 'input-select') as HTMLSelectElement;
      sel.dataset.testid = `midi-endpoint-${moduleId}`;
      sel.setAttribute('aria-label', 'MIDI input device');
      const sysOf = (d: Device) => d.system_name ?? (d.id.includes('|') ? d.id.slice(d.id.indexOf('|') + 1) : '');
      const ALL = 'all_enabled';
      const SAVED = '\u0000saved';
      let devices: Device[] = [];
      const saved = () => {
        const cfg = host.doc.modules.get(moduleId)?.config as Record<string, unknown> | undefined;
        const endpoint = cfg && typeof cfg.endpoint === 'string' ? cfg.endpoint : ALL;
        const systemName = cfg && typeof cfg.system_name === 'string' ? cfg.system_name : '';
        return { endpoint, systemName };
      };
      const refresh = async () => {
        try { devices = await host.listMidiDevices(); } catch { /* keep saved choice */ }
        if (!sel.isConnected) return;
        const { endpoint, systemName } = saved();
        const items = [{ value: ALL, label: 'All enabled devices' },
          ...devices.map(d => ({ value: d.id, label: d.name }))];
        let selected = ALL;
        if (endpoint !== ALL) {
          const match = devices.find(d => d.name === endpoint && sysOf(d) === systemName);
          if (match) selected = match.id;
          else {
            selected = SAVED;
            items.push({ value: SAVED, label: `${endpoint} (not connected)` });
          }
        }
        sel.replaceChildren(...items.map(item => {
          const option = document.createElement('option');
          option.value = item.value; option.textContent = item.label;
          return option;
        }));
        sel.value = selected;
      };
      sel.addEventListener('change', () => {
        if (sel.value === SAVED) return;
        if (sel.value === ALL) { host.setModuleEndpoint(moduleId, ALL); return; }
        const d = devices.find(x => x.id === sel.value);
        if (d) host.setModuleEndpoint(moduleId, d.name, sysOf(d));
      });
      row.append(el('div', 'control-caption', 'MIDI input'), sel);
      controls.append(row);
      void refresh();
      this.widgets.set('config.endpoint', { root: row, refresh: () => { void refresh(); },
        destroy() { /* contained by the card */ } });
    }
    this.root.append(controls);
    this.refreshVisibility();
  }

  /** Re-read every value from the document (after undo, load, or refusal). */
  refresh(): void {
    for (const w of this.widgets.values()) w.refresh();
    this.refreshVisibility();
    for (const p of this.panes) p.refresh?.();
  }
  /** Repaint one parameter's widget, if it has one. */
  refreshParam(id: string): void {
    this.widgets.get(id)?.refresh();
    this.refreshVisibility();
    for (const p of this.panes) p.refresh?.();
  }

  /** Read base document values, never transient DSP modulation. Keep widgets and rails alive. */
  private refreshVisibility(): void {
    let changed = false;
    const show = (node: HTMLElement, visible: boolean) => {
      const display = visible ? '' : 'none';
      if (node.style.display !== display) { node.style.display = display; changed = true; }
    };
    for (const p of this.d.parameters) {
      const w = this.widgets.get(p.id);
      if (!w) continue;
      const gate = p.visibleWhen;
      // Malformed/future metadata must not silently discard an otherwise usable control.
      const valid = gate && typeof gate.param === 'string' && findParam(this.d, gate.param)
        && Array.isArray(gate.values) && gate.values.every(v => typeof v === 'number' && Number.isFinite(v));
      show(w.root, !valid || gate.values.includes(this.value(gate.param)));
    }
    for (const s of this.sections) {
      const visible = s.params.some(id => {
        const w = this.widgets.get(id);
        return w && w.root.style.display !== 'none';
      });
      show(s.row, visible);
      if (s.label) show(s.label, visible);
    }
    if (changed && this.root.isConnected) this.onLayoutChanged?.();
  }

  destroy(): void {
    for (const w of this.widgets.values()) w.destroy();
    for (const p of this.panes) p.destroy();
  }

  private value(id: string): number {
    return this.host.doc.param(this.moduleId, id) ?? findParam(this.d, id)?.default ?? 0;
  }

  private buildWidget(p: ModuleParam, heroSection: boolean): Widget | null {
    const L = this.layout;
    const kind = L.widgets[p.id] ?? 'knob';
    const label = L.short[p.id] ?? p.name;
    const named: ModuleParam = { ...p, name: label };
    const moduleId = this.moduleId;
    const get = () => this.value(p.id);
    const set = (v: number, commit = true) => this.host.commitParam(moduleId, p.id, v, commit);

    switch (kind) {
      case 'segmented': return new Segmented({ param: named, moduleId, getValue: get, setValue: v => set(v) });
      case 'select': return new Selector({ param: named, moduleId, getValue: get, setValue: v => set(v) });
      case 'toggle': return new Toggle({ param: named, moduleId, getValue: get, setValue: v => set(v) });
      case 'number': {
        const nb = new NumberBox({ param: named, moduleId, getValue: get, setValue: set });
        // A number in the hero role (e.g. tempo) is drawn large, like the kit's clock (DESIGN §4.2).
        if (L.roles[p.id] === 'hero') nb.root.classList.add('big');
        return nb;
      }
      case 'fader': return new Fader({ param: named, moduleId, getValue: get, setValue: set, ticker: this.host.ticker });
      case 'readout': {
        const root = el('div', 'control-number static');
        const val = el('div', 'control-readout');
        root.append(el('div', 'control-label', label), val);
        const refresh = () => {
          const f = formatValue(p, get());
          val.textContent = f.unit === "%" ? `${f.text}%` : f.unit ? `${f.text} ${f.unit}` : f.text;
        };
        refresh();
        return { root, refresh, destroy() { /* static */ } };
      }
      default: {
        let tier = L.tiers[p.id] ?? 'M';
        if (heroSection && tier === 'S' && L.roles[p.id] !== 'trim') tier = 'M';
        const t = this.host.ticker;
        const musical = this.d.id === 'essentials.vco' && p.id === 'frequency' ? 'vco'
          : this.d.id === 'ports.macro_oscillator' && p.id === 'coarse' ? 'macro-coarse' : undefined;
        return new Knob({
          param: named, moduleId, tier, ticker: t, musical,
          pitchConnected: musical === 'vco' ? () => this.host.doc.connections.some(c =>
            c.to.module === moduleId && c.to.port === 'pitch') : undefined,
          getValue: get,
          setValue: (v, commit) => set(v, commit),
          getMods: () => this.host.doc.modsInto(moduleId, p.id),
          describeSource: c => this.host.describeSource(c),
          cableColor: c => this.host.cableColor(c),
          sourceValue: c => t.last?.outputs?.[`${c.from.module}:${c.from.port}`] ?? 0,
        });
      }
    }
  }
}
