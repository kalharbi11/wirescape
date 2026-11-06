// SPDX-License-Identifier: GPL-3.0-only
import { el } from '../../app/dom';
import type { PaneContext } from '../panes';
import './model-grid.css';

/** Direct selection for a bank of integer models; values come from the catalogue. */
export class ModelGrid {
  readonly root: HTMLElement;
  private readonly buttons = new Map<number, HTMLButtonElement>();
  private readonly parameter: string;

  constructor(private readonly c: PaneContext) {
    this.parameter = typeof c.options.parameter === 'string' ? c.options.parameter : 'model';
    const p = c.param(this.parameter);
    this.root = el('div', 'model-bank');
    this.root.dataset.testid = `models-${c.moduleId}`;
    if (!p) return;
    this.root.append(el('div', 'control-label', p.name));
    const grid = el('div', 'model-grid');
    grid.setAttribute('role', 'radiogroup');
    grid.setAttribute('aria-label', p.name);
    const choices = p.choices?.length ? p.choices : Array.from(
      { length: Math.min(64, Math.floor((p.maximum - p.minimum) / (p.step || 1)) + 1) },
      (_, i) => ({ value: p.minimum + i * (p.step || 1), label: String(p.minimum + i * (p.step || 1)).padStart(2, '0') }),
    );
    for (const choice of choices) {
      const b = el('button', 'model-button', choice.label);
      b.type = 'button';
      b.dataset.testid = `model-${c.moduleId}-${choice.value}`;
      b.setAttribute('role', 'radio');
      b.setAttribute('aria-label', `${p.name} ${choice.label}`);
      b.addEventListener('click', () => { c.setValue(this.parameter, choice.value); this.refresh(); });
      b.addEventListener('keydown', e => {
        const columns = getComputedStyle(grid).gridTemplateColumns.split(' ').length;
        const offsets: Record<string, number> = { ArrowLeft: -1, ArrowRight: 1, ArrowUp: -columns, ArrowDown: columns };
        const index = choices.findIndex(v => v.value === choice.value);
        let next = index;
        if (e.key in offsets) next = Math.max(0, Math.min(choices.length - 1, index + offsets[e.key]!));
        else if (e.key === 'Home') next = 0;
        else if (e.key === 'End') next = choices.length - 1;
        else return;
        e.preventDefault(); e.stopPropagation();
        const value = choices[next]!.value;
        c.setValue(this.parameter, value);
        this.refresh();
        this.buttons.get(value)?.focus();
      });
      this.buttons.set(choice.value, b);
      grid.append(b);
    }
    this.root.append(grid);
    this.refresh();
  }

  refresh(): void {
    const value = this.c.getValue(this.parameter);
    for (const [v, b] of this.buttons) {
      const selected = v === value;
      b.setAttribute('aria-checked', String(selected));
      b.tabIndex = selected ? 0 : -1;
    }
  }

  destroy(): void { /* no timers or telemetry subscriptions */ }
}
