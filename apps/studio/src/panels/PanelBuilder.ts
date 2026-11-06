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

export interface PanelHost {
  doc: PatchDoc;
  ticker: Ticker;
  registry: ModuleRegistry;
  /** Write a parameter through the engine; rolls back if it refuses. */
  commitParam(moduleId: number, paramId: string, value: number, commit: boolean): void;
  commitModAmount(c: Connection, amount: number, commit: boolean): void;
  listMidiDevices(): Promise<Device[]>;
  setModuleEndpoint(moduleId: number, endpoint: string): void;
  describeSource(c: Connection): string;
  /** The cable's user-picked colour, as a CSS colour. */
  cableColor(c: Connection): string;
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

  constructor(
    private readonly host: PanelHost,
    private readonly moduleId: number,
    private readonly d: ModuleDescriptor,
    ov: PanelOverride | undefined,
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
      });
      if (!pane) continue;
      this.panes.push(pane);
      displays.append(pane.root);
      if (pane.headerEl) this.headerEls.push(pane.headerEl);
    }
    if (this.panes.length) this.root.append(displays);

    const controls = el('div', 'panel-controls');
    for (const s of this.layout.sections) {
      if (s.label) controls.append(el('div', 'section-label', s.label));
      const row = el('div', 'panel-row');
      if (s.columns) row.style.maxWidth = `${s.columns * 92}px`;
      for (const id of s.params) {
        const p = findParam(d, id);
        if (!p) continue;
        const w = this.buildWidget(p, !!s.hero);
        if (w) { this.widgets.set(id, w); row.append(w.root); }
      }
      controls.append(row);
    }
    if (d.id === 'midi.in') {
      const row = el('div', 'control-row select-row');
      const sel = el('select', 'input-select') as HTMLSelectElement;
      sel.dataset.testid = `midi-endpoint-${moduleId}`;
      sel.setAttribute('aria-label', 'MIDI input device');
      const current = () => {
        const cfg = host.doc.modules.get(moduleId)?.config;
        return cfg && typeof cfg === 'object' && 'endpoint' in cfg
          && typeof cfg.endpoint === 'string' ? cfg.endpoint : 'all_enabled';
      };
      const refresh = async () => {
        let devices: Device[] = [];
        try { devices = await host.listMidiDevices(); } catch { /* keep saved choice */ }
        if (!sel.isConnected) return;
        const endpoint = current();
        const items = [{ id: 'all_enabled', name: 'All enabled devices' },
          ...devices.map(d => ({ id: d.id, name: d.name }))];
        if (!items.some(d => d.id === endpoint)) {
          items.push({ id: endpoint, name: `${endpoint} (not connected)` });
        }
        sel.replaceChildren(...items.map(item => {
          const option = document.createElement('option');
          option.value = item.id; option.textContent = item.name;
          return option;
        }));
        sel.value = endpoint;
      };
      sel.addEventListener('change', () => host.setModuleEndpoint(moduleId, sel.value));
      row.append(el('div', 'control-caption', 'MIDI input'), sel);
      controls.append(row);
      void refresh();
      this.widgets.set('config.endpoint', { root: row, refresh: () => { void refresh(); },
        destroy() { /* contained by the card */ } });
    }
    this.root.append(controls);
  }

  /** Re-read every value from the document (after undo, load, or refusal). */
  refresh(): void {
    for (const w of this.widgets.values()) w.refresh();
    for (const p of this.panes) p.refresh?.();
  }
  /** Repaint one parameter's widget, if it has one. */
  refreshParam(id: string): void {
    this.widgets.get(id)?.refresh();
    for (const p of this.panes) p.refresh?.();
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
        return new Knob({
          param: named, moduleId, tier, ticker: t,
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
