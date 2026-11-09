// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleParam } from '../../modules/types';
import { el } from '../../app/dom';

export interface ToggleOptions {
  param: ModuleParam;
  moduleId: number;
  getValue(): number;
  setValue(v: number): void;
}

/** A two-choice on/off parameter renders as a switch rather than segments. */
export class Toggle {
  readonly root: HTMLElement;
  private readonly sw: HTMLButtonElement;
  private readonly stateEl: HTMLElement;
  private readonly values: [number, number];
  private readonly labels: [string, string];

  constructor(private readonly o: ToggleOptions) {
    const p = o.param;
    this.root = el('div', 'control-row toggle-row');
    this.root.dataset.param = p.id;
    this.root.append(el('div', 'control-caption', p.name));
    this.sw = el('button', 'toggle') as HTMLButtonElement;
    this.sw.type = 'button';
    this.sw.append(el('i', 'led'), (this.stateEl = el('span', 'toggle-state')));
    this.sw.setAttribute('aria-label', p.name);
    this.sw.dataset.testid = `toggle-${o.moduleId}-${p.id}`;
    // The WebAssembly catalogue can omit choice labels while a panel override
    // still requests a toggle. Fall back to the descriptor's numeric bounds.
    const choices = p.choices?.length === 2 ? p.choices
      : [{ value: p.minimum, label: 'Off' }, { value: p.maximum, label: 'On' }];
    this.values = [choices[0]!.value, choices[1]!.value];
    this.labels = [choices[0]!.label, choices[1]!.label];
    this.sw.addEventListener('click', () => {
      const next = this.isOn() ? this.values[0] : this.values[1];
      o.setValue(next);
      this.refresh();
    });
    this.root.append(this.sw);
    this.refresh();
  }

  private isOn(): boolean {
    return Math.abs(this.o.getValue() - this.values[1]) < 1e-6;
  }

  refresh(): void {
    const on = this.isOn();
    this.sw.classList.toggle('on', on);
    this.sw.setAttribute('aria-pressed', String(on));
    this.stateEl.textContent = this.labels[on ? 1 : 0];
    this.sw.title = `${this.o.param.name}: ${this.labels[on ? 1 : 0]}`;
  }
  destroy(): void { /* no subscriptions */ }
}
