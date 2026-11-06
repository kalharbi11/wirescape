import { el } from '../../app/dom';
import type { PaneContext } from '../panes';
import { Fader } from './Fader';
import { Meter } from './Meter';
import './channel-meters.css';

/** A small channel bank using the shared faders and engine-supplied level pair. */
export class ChannelMeters {
  readonly root: HTMLElement;
  private readonly faders: Fader[] = [];
  private readonly meter: Meter;

  constructor(c: PaneContext) {
    this.root = el('div', 'channel-bank');
    this.root.dataset.testid = `channels-${c.moduleId}`;
    const ids = Array.isArray(c.options.parameters)
      ? c.options.parameters.filter((v): v is string => typeof v === 'string').slice(0, 2) : [];
    const labels = Array.isArray(c.options.labels)
      ? c.options.labels.filter((v): v is string => typeof v === 'string') : [];
    const row = el('div', 'channel-faders');
    ids.forEach((id, i) => {
      const p = c.param(id);
      if (!p) return;
      const fader = new Fader({
        param: { ...p, name: labels[i] ?? p.name }, moduleId: c.moduleId, ticker: c.ticker,
        getValue: () => c.getValue(id), setValue: (v, commit) => c.setValue(id, v, commit),
      });
      this.faders.push(fader);
      row.append(fader.root);
    });
    this.root.append(row, el('div', 'control-label channel-meter-label', 'POST GAIN'));
    this.meter = new Meter(c.moduleId, c.ticker, [labels[0] ?? 'A', labels[1] ?? 'B']);
    this.root.append(this.meter.root);
  }

  refresh(): void { for (const fader of this.faders) fader.refresh(); }
  destroy(): void { for (const fader of this.faders) fader.destroy(); this.meter.destroy(); }
}
