import type { ModuleParam } from '../../modules/types';
import { formatValue, normToValue, quantize, valueToNorm , parseTyped, displayValue } from '../../modules/registry';
import { el } from '../../app/dom';

export interface NumberBoxOptions {
  param: ModuleParam;
  moduleId: number;
  getValue(): number;
  /** `commit` is false during a drag and true when the gesture ends. */
  setValue(v: number, commit: boolean): void;
}

const PER_PX = 0.004;
const clamp01 = (n: number) => (n < 0 ? 0 : n > 1 ? 1 : n);

/** Plain dotted number: label + value with a dotted underline. */
export class NumberBox {
  readonly root: HTMLElement;
  private value: HTMLElement;

  constructor(private readonly o: NumberBoxOptions) {
    const p = o.param;
    this.root = el('div', 'control-number');
    this.root.dataset.param = p.id;
    this.root.dataset.testid = `number-${o.moduleId}-${p.id}`;
    this.root.tabIndex = 0;
    this.root.setAttribute('role', 'spinbutton');
    this.root.setAttribute('aria-label', p.name);
    this.root.setAttribute('aria-valuemin', String(p.minimum));
    this.root.setAttribute('aria-valuemax', String(p.maximum));

    this.value = this.makeValue();
    this.root.append(el('span', 'control-label nb-label', p.name), this.value);

    this.root.addEventListener('pointerdown', e => this.onDown(e));
    this.root.addEventListener('dblclick', e => {
      e.preventDefault();
      o.setValue(p.default, true);
      this.refresh();
    });
    this.root.addEventListener('wheel', e => {
      e.preventDefault(); e.stopPropagation();
      this.step(e.deltaY > 0 ? -1 : 1, e.shiftKey);
    }, { passive: false });
    this.root.addEventListener('keydown', e => {
      if (e.target !== this.root) return;
      const dir = e.key === 'ArrowUp' || e.key === 'ArrowRight' ? 1
        : e.key === 'ArrowDown' || e.key === 'ArrowLeft' ? -1 : 0;
      if (!dir) return;
      e.preventDefault();
      this.step(dir, e.shiftKey);
    });
    this.refresh();
  }

  private makeValue(): HTMLElement {
    const v = el('span', 'control-readout nb-value');
    v.dataset.testid = `readout-${this.o.moduleId}-${this.o.param.id}`;
    v.title = 'Drag to change · click to type · double-click resets';
    return v;
  }

  refresh(): void {
    const f = formatValue(this.o.param, this.o.getValue());
    const text = f.unit === "%" ? `${f.text}%` : f.unit ? `${f.text} ${f.unit}` : f.text;
    this.value.textContent = text;
    this.root.setAttribute('aria-valuenow', String(this.o.getValue()));
    this.root.setAttribute('aria-valuetext', text);
  }

  destroy(): void { /* no subscriptions */ }

  private step(dir: number, fine: boolean): void {
    const p = this.o.param;
    const v = this.o.getValue();
    let next: number;
    if (p.step && p.step > 0) next = quantize(p, v + dir * p.step);
    else next = normToValue(p, valueToNorm(p, v) + dir * (fine ? 0.002 : 0.01));
    this.o.setValue(next, true);
    this.refresh();
  }

  private onDown(e: PointerEvent): void {
    if (e.button !== 0 || e.target instanceof HTMLInputElement) return;
    e.stopPropagation();
    const p = this.o.param;
    const startX = e.clientX;
    const startNorm = valueToNorm(p, this.o.getValue());
    let dragged = false;
    this.root.setPointerCapture(e.pointerId);

    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== e.pointerId) return;
      const dx = ev.clientX - startX;
      if (!dragged && Math.abs(dx) < 3) return;
      dragged = true;
      this.root.classList.add('dragging');
      const n = clamp01(startNorm + dx * PER_PX * (ev.shiftKey ? 0.2 : 1));
      this.o.setValue(normToValue(p, n), false);
      this.refresh();
    };
    const up = () => {
      this.root.removeEventListener('pointermove', move);
      this.root.removeEventListener('pointerup', up);
      this.root.removeEventListener('pointercancel', up);
      this.root.classList.remove('dragging');
      if (dragged) { this.o.setValue(this.o.getValue(), true); this.refresh(); }
      else this.promptValue();
    };
    this.root.addEventListener('pointermove', move);
    this.root.addEventListener('pointerup', up);
    this.root.addEventListener('pointercancel', up);
  }

  private promptValue(): void {
    const p = this.o.param;
    const input = el('input', 'readout-input') as HTMLInputElement;
    input.value = displayValue(p, this.o.getValue());
    input.dataset.testid = `readout-input-${this.o.moduleId}-${p.id}`;
    input.setAttribute('aria-label', `${p.name} value`);
    const shown = this.value;
    shown.replaceWith(input);
    input.focus(); input.select();
    let done = false;
    const finish = (apply: boolean) => {
      if (done) return;
      done = true;
      if (apply) {
        const v = parseTyped(p, input.value, this.o.getValue());
        if (v !== null) this.o.setValue(v, true);
      }
      input.replaceWith(shown);
      this.refresh();
    };
    input.addEventListener('keydown', e => {
      e.stopPropagation();
      if (e.key === 'Enter') { e.preventDefault(); finish(true); }
      if (e.key === 'Escape') { e.preventDefault(); finish(false); }
    });
    input.addEventListener('blur', () => finish(true));
  }
}
