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
    const [off, on] = p.choices!;
    this.sw.addEventListener('click', () => {
      const next = this.isOn() ? off!.value : on!.value;
      o.setValue(next);
      this.refresh();
    });
    this.root.append(this.sw);
    this.refresh();
  }

  private isOn(): boolean {
    const p = this.o.param;
    return Math.abs(this.o.getValue() - p.choices![1]!.value) < 1e-6;
  }

  refresh(): void {
    const on = this.isOn();
    this.sw.classList.toggle('on', on);
    this.sw.setAttribute('aria-pressed', String(on));
    this.stateEl.textContent = this.o.param.choices![on ? 1 : 0]!.label;
    this.sw.title = `${this.o.param.name}: ${this.o.param.choices![on ? 1 : 0]!.label}`;
  }
  destroy(): void { /* no subscriptions */ }
}
