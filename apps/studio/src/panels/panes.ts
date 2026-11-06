// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleDescriptor, ModuleParam } from '../modules/types';
import type { PatchDoc } from '../patch/PatchDoc';
import type { Ticker } from '../runtime/Ticker';
import type { PaneKind } from './layouts';
import { Meter } from './widgets/Meter';
import { Scope } from './widgets/Scope';
import { WavePreview } from './widgets/WavePreview';
import { EnvelopeCurve } from './widgets/EnvelopeCurve';
import { FilterDisplay } from './widgets/FilterDisplay';
import { ChannelMeters } from './widgets/ChannelMeters';
import { ModelGrid } from './widgets/ModelGrid';
import { StepsPane } from './widgets/StepsPane';

/** Everything a display pane may read or write. Values go through the engine via `setValue`. */
export interface PaneContext {
  moduleId: number;
  ticker: Ticker;
  descriptor: ModuleDescriptor;
  doc: PatchDoc;
  param(id: string): ModuleParam | undefined;
  getValue(id: string): number;
  setValue(id: string, v: number, commit?: boolean): void;
  /** Free-form options from `panels/<id>.json` → `paneOptions[kind]`. */
  options: Record<string, unknown>;
}

export interface Pane {
  root: HTMLElement;
  destroy(): void;
  /** Optional element the card places in its header (e.g. the KNOBS/GRAPH switch). */
  headerEl?: HTMLElement;
  /** Optional: repaint after a value changed outside the pane. */
  refresh?(): void;
}

/** Builds a display pane. No module-specific code: what to read comes from the pane options. */
export function buildPane(kind: PaneKind, c: PaneContext): Pane | null {
  const { moduleId: id, ticker } = c;
  const v = (pid: string, fb: number) => c.doc.param(id, pid) ?? fb;
  switch (kind) {
    case 'wave':
      return new WavePreview({
        moduleId: id, ticker,
        shape: () => {
          const sync = v('sync', 0) >= 0.5;
          return {
            depth: v('depth', 1), offset: v('offset', 0),
            rateLabel: sync ? `${trim(v('beats', 1))} beats / cycle` : `${trim(v('rate', 1))} Hz`,
          };
        },
      });
    case 'envelope': {
      const opt = (k: string, ...fb: string[]) => {
        const o = c.options[k];
        return typeof o === 'string' ? o : fb.find(f => c.param(f));
      };
      const attack = opt('attack', 'attack', 'rise'), decay = opt('decay', 'decay', 'fall');
      if (!attack || !decay) return null;
      const sustain = opt('sustain', 'sustain'), release = opt('release', 'release');
      return new EnvelopeCurve({
        moduleId: id, ticker,
        ids: { attack, decay, ...(sustain && release ? { sustain, release } : {}) },
        get: pid => c.getValue(pid),
        set: (pid, val, commit) => c.setValue(pid, val, commit),
        range: pid => { const p = c.param(pid); return { min: p?.minimum ?? 0, max: p?.maximum ?? 1 }; },
      });
    }
    case 'filter': {
      const str = (k: string, fb: string) => (typeof c.options[k] === 'string' ? c.options[k] as string : fb);
      const cutoff = str('cutoff', 'cutoff'), resonance = str('resonance', 'resonance');
      if (!c.param(cutoff) || !c.param(resonance)) return null;
      const mode = typeof c.options.mode === 'string' && c.param(c.options.mode) ? c.options.mode : undefined;
      return new FilterDisplay({
        moduleId: id, ticker, cutoff, resonance, mode, graph: c.options.graph === true,
        get: pid => c.getValue(pid),
        set: (pid, val, commit) => c.setValue(pid, val, commit),
        range: pid => { const p = c.param(pid); return { min: p?.minimum ?? 0, max: p?.maximum ?? 1 }; },
      });
    }
    case 'meter': return new Meter(id, ticker);
    case 'scope': return new Scope(id, ticker);
    case 'channels': return new ChannelMeters(c);
    case 'models': return new ModelGrid(c);
    case 'steps':
      return new StepsPane({
        moduleId: id, ticker, steps: 8,
        param: pid => c.param(pid),
        getValue: pid => c.getValue(pid),
        setValue: (pid, val, commit) => c.setValue(pid, val, commit),
      });
    default: return null;
  }
}

const trim = (n: number) => (Math.round(n * 100) / 100).toString();

