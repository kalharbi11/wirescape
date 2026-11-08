// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleParam } from '../../modules/types';
import type { Ticker } from '../../runtime/Ticker';
import { formatValue, normToValue, quantize, valueToNorm, parseTyped, displayValue } from '../../modules/registry';
import { el } from '../../app/dom';

export interface FaderOptions {
  param: ModuleParam;
  moduleId: number;
  getValue(): number;
  setValue(v: number, commit: boolean): void;
  ticker?: Ticker;
  /** Track height in px. */
  height?: number;
}

const TRAVEL = 180;
const clamp01 = (n: number) => (n < 0 ? 0 : n > 1 ? 1 : n);

/** Vertical fader: hair track, grey fill, 16x8 thumb, label + value below. */
export class Fader {
  readonly root: HTMLElement;
  private readonly track: HTMLElement;
  private readonly fill: HTMLElement;
  private readonly thumb: HTMLElement;
  private readonly ghost: HTMLElement;
  private readonly valueEl: HTMLElement;
  private readonly h: number;
  private unbind: () => void = () => {};
  private live: number | undefined;
  private liveBase = NaN;

  constructor(private readonly o: FaderOptions) {
    const p = o.param;
    this.h = o.height ?? 90;
    this.root = el('div', 'control-fader');
    this.root.dataset.param = p.id;
    this.root.dataset.testid = `fader-${o.moduleId}-${p.id}`;

    this.track = el('div', 'fader-track');
    this.track.style.height = `${this.h}px`;
    this.fill = el('div', 'fader-fill');
    this.ghost = el('div', 'fader-live');
    this.thumb = el('div', 'fader-thumb');
    this.thumb.tabIndex = 0;
    this.thumb.setAttribute('role', 'slider');
    this.thumb.setAttribute('aria-label', p.name);
    this.thumb.setAttribute('aria-orientation', 'vertical');
    this.thumb.setAttribute('aria-valuemin', String(p.minimum));
    this.thumb.setAttribute('aria-valuemax', String(p.maximum));
    this.thumb.dataset.testid = `fader-thumb-${o.moduleId}-${p.id}`;
    this.track.append(this.fill, this.ghost, this.thumb);

    this.valueEl = el('div', 'fader-value');
    this.root.append(this.track, el('div', 'control-label fader-label', p.name), this.valueEl);
    // OPEN-8: click the value to type one (same parser as knobs and number boxes).
    this.valueEl.title = 'Click to type a value';
    this.valueEl.addEventListener('click', () => this.promptValue());

    this.track.addEventListener('pointerdown', e => this.onDown(e));
    this.track.addEventListener('dblclick', e => {
      e.preventDefault();
      o.setValue(p.default, true);
      this.refresh();
    });
    this.track.addEventListener('wheel', e => {
      e.preventDefault(); e.stopPropagation();
      this.nudge(e.deltaY > 0 ? -1 : 1, e.shiftKey);
    }, { passive: false });
    this.thumb.addEventListener('keydown', e => {
      const dir = e.key === 'ArrowUp' || e.key === 'ArrowRight' ? 1
        : e.key === 'ArrowDown' || e.key === 'ArrowLeft' ? -1 : 0;
      if (dir) { e.preventDefault(); this.nudge(dir, e.shiftKey); }
      else if (e.key === 'Home') { e.preventDefault(); this.o.setValue(p.minimum, true); this.refresh(); }
      else if (e.key === 'End') { e.preventDefault(); this.o.setValue(p.maximum, true); this.refresh(); }
    });

    this.refresh();
    if (o.ticker) {
      this.unbind = o.ticker.bind(o.moduleId, t => {
        const v = t.effective[o.moduleId]?.[p.id];
        this.paintLive(v ?? this.o.getValue());
      });
    }
  }

  destroy(): void { this.unbind(); }

  refresh(): void {
    const p = this.o.param;
    const v = this.o.getValue();
    const n = valueToNorm(p, v);
    const y = (1 - n) * this.h;
    this.thumb.style.transform = `translateY(${y}px)`;
    this.fill.style.transform = `scaleY(${n})`;
    const f = formatValue(p, v);
    const text = f.unit === "%" ? `${f.text}%` : f.unit ? `${f.text} ${f.unit}` : f.text;
    this.valueEl.textContent = text;
    this.valueEl.dataset.testid = `readout-${this.o.moduleId}-${p.id}`;
    this.thumb.setAttribute('aria-valuenow', String(v));
    this.thumb.setAttribute('aria-valuetext', text);
    this.paintLive(this.root.classList.contains('modded') ? this.live ?? v : v);
  }

  private paintLive(v: number): void {
    const base = this.o.getValue();
    if (v === this.live && base === this.liveBase) return;
    this.live = v;
    this.liveBase = base;
    // The engine also reports unmodulated parameters. Rust is reserved for
    // an effective value that actually differs from the thumb's set value.
    this.root.classList.toggle('modded', Math.abs(v - base) > 1e-9);
    this.ghost.style.transform = `translateY(${(1 - valueToNorm(this.o.param, v)) * this.h}px)`;
  }

  private nudge(dir: number, fine: boolean): void {
    const p = this.o.param;
    const v = this.o.getValue();
    const next = p.step && p.step > 0
      ? quantize(p, v + dir * p.step)
      : normToValue(p, valueToNorm(p, v) + dir * (fine ? 0.002 : 0.01));
    this.o.setValue(next, true);
    this.refresh();
  }

  private onDown(e: PointerEvent): void {
    if (e.button !== 0) return;
    e.preventDefault(); e.stopPropagation();
    const p = this.o.param;
    this.thumb.focus();
    this.track.setPointerCapture(e.pointerId);
    const startY = e.clientY;
    const startNorm = valueToNorm(p, this.o.getValue());
    this.root.classList.add('dragging');
    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== e.pointerId) return;
      const scale = (ev.shiftKey ? 0.2 : 1) / TRAVEL;
      this.o.setValue(normToValue(p, clamp01(startNorm + (startY - ev.clientY) * scale)), false);
      this.refresh();
    };
    const up = () => {
      this.track.removeEventListener('pointermove', move);
      this.track.removeEventListener('pointerup', up);
      this.track.removeEventListener('pointercancel', up);
      this.root.classList.remove('dragging');
      this.o.setValue(this.o.getValue(), true);
    };
    this.track.addEventListener('pointermove', move);
    this.track.addEventListener('pointerup', up);
    this.track.addEventListener('pointercancel', up);
  }

  private promptValue(): void {
    const p = this.o.param;
    const input = el('input', 'readout-input') as HTMLInputElement;
    input.value = displayValue(p, this.o.getValue());
    input.dataset.testid = `readout-input-${this.o.moduleId}-${p.id}`;
    input.setAttribute('aria-label', `${p.name} value`);
    this.valueEl.replaceWith(input);
    input.focus(); input.select();
    let done = false;
    const finish = (apply: boolean) => {
      if (done) return;
      done = true;
      if (apply) {
        const v = parseTyped(p, input.value, this.o.getValue());
        if (v !== null) this.o.setValue(v, true);
      }
      input.replaceWith(this.valueEl);
      this.refresh();
    };
    input.addEventListener('keydown', e => {
      e.stopPropagation();
      if (e.key === 'Enter') { e.preventDefault(); finish(true); }
      if (e.key === 'Escape') { e.preventDefault(); finish(false); }
    });
    input.addEventListener('blur', () => finish(true));
    input.addEventListener('pointerdown', e => e.stopPropagation());
  }
}
