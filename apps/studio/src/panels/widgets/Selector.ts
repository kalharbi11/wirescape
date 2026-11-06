import type { ModuleParam } from '../../modules/types';
import { el } from '../../app/dom';

export interface SelectorOptions {
  param: ModuleParam;
  moduleId: number;
  getValue(): number;
  setValue(v: number): void;
}

/** A `choices` parameter with too many options for segmented buttons. */
export class Selector {
  readonly root: HTMLElement;
  private readonly sel: HTMLSelectElement;

  constructor(private readonly o: SelectorOptions) {
    const p = o.param;
    this.root = el('div', 'control-row select-row');
    this.root.dataset.param = p.id;
    this.root.append(el('div', 'control-caption', p.name));
    this.sel = el('select', 'input-select') as HTMLSelectElement;
    this.sel.dataset.testid = `select-${o.moduleId}-${p.id}`;
    this.sel.setAttribute('aria-label', p.name);
    for (const c of p.choices ?? []) {
      const op = document.createElement('option');
      op.value = String(c.value);
      op.textContent = c.label;
      this.sel.append(op);
    }
    this.sel.addEventListener('change', () => o.setValue(Number(this.sel.value)));
    this.root.append(this.sel);
    this.refresh();
  }

  refresh(): void { this.sel.value = String(this.o.getValue()); }
  destroy(): void { /* no subscriptions */ }
}
