// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleParam } from '../../modules/types';
import type { Connection } from '../../patch/types';
import type { Ticker } from '../../runtime/Ticker';
import type { KnobTier } from '../layouts';
import { formatValue, formatRange, normToValue, valueToNorm, parseTyped, displayValue } from '../../modules/registry';
import { el, svgEl } from '../../app/dom';
import { attachParamInfo, type InfoSource } from './InfoBox';

/** Sweep: -135deg (minimum) to +135deg (maximum). DESIGN §4.1. */
const SWEEP = 270;
const START = -135;

/** Tier geometry, from the kit's `TIERS` (DESIGN §4.1). */
interface TierGeom {
  sz: number; r: number; cap: number; mod: number; tk0: number;
  mn: number; mj: number; lab: number; live: number; w: number;
}
const TIERS: Record<KnobTier, TierGeom> = {
  L: { sz: 144, r: 46, cap: 30, mod: 41.5, tk0: 49.5, mn: 3, mj: 6.5, lab: 62, live: 2.8, w: 2.4 },
  M: { sz: 76, r: 26, cap: 17, mod: 22.6, tk0: 28.8, mn: 0, mj: 3.4, lab: 0, live: 2.3, w: 2 },
  S: { sz: 38, r: 14, cap: 9, mod: 11.6, tk0: 0, mn: 0, mj: 0, lab: 0, live: 1.9, w: 2.4 },
};

export interface KnobOptions {
  param: ModuleParam;
  moduleId: number;
  tier: KnobTier;
  getValue(): number;
  /** `commit` is false during a drag and true when the gesture ends. */
  setValue(v: number, commit: boolean): void;
  getMods(): Connection[];
  describeSource(c: Connection): string;
  /** The source's current value (-1..1), for the info box sparkline. */
  sourceValue(c: Connection): number;
  /** The colour the user picked for this cable. */
  cableColor(c: Connection): string;
  ticker: Ticker;
}

/**
 * Knob in three tiers (L hero, M standard, S trim). The cap and pointer
 * show the value you set; one rust arc inside the track shows the combined
 * modulation range, and a rust dot on the track shows the live value
 * (DESIGN §4.1, §5). Per frame it writes one transform.
 */
export class Knob {
  readonly root: HTMLElement;
  private readonly dial: HTMLButtonElement;
  private readonly rotor: SVGGElement;
  private readonly marker: SVGGElement;
  private readonly valArc: SVGPathElement;
  private readonly modArc: SVGPathElement;
  private readonly readout: HTMLButtonElement;
  private readonly liveEl: HTMLElement;
  private readonly chips: HTMLElement | null;
  private unbind: () => void;
  private info: { destroy(): void };
  private live: number;
  private mods: Connection[] = [];
  private lastText = 0;

  constructor(private readonly o: KnobOptions) {
    const p = o.param;
    const g = TIERS[o.tier];
    this.live = o.getValue();

    this.root = el('div', `knob-unit tier-${o.tier}`);
    this.root.dataset.param = p.id;
    this.root.dataset.modulatable = String(p.modulatable);
    this.root.dataset.testid = `knob-${o.moduleId}-${p.id}`;

    const wrap = el('div', 'dial-wrap');
    wrap.style.width = wrap.style.height = `${g.sz}px`;
    const c = g.sz / 2;
    const svg = svgEl('svg', {
      class: 'knob-svg', width: String(g.sz), height: String(g.sz),
      viewBox: `0 0 ${g.sz} ${g.sz}`, 'aria-hidden': 'true',
    });
    const og = `transform-origin:${c}px ${c}px`;

    // Ticks and scale labels (L: minor + major + labels; M: major only).
    const marks = scaleMarks(p, o.tier);
    if (g.mn) {
      let d = '';
      for (let i = 0; i <= 50; i++) d += radial(c, g.tk0, g.tk0 + g.mn, ang(i / 50));
      svg.append(svgEl('path', { d, class: 'tk' }));
    }
    if (g.mj) {
      let d = '';
      for (const n of marks) d += radial(c, g.tk0, g.tk0 + g.mj, ang(n));
      svg.append(svgEl('path', { d, class: 'tk mj' }));
    }
    if (g.lab) {
      for (const n of marks) {
        const [x, y] = pol(c, g.lab, ang(n));
        const t = svgEl('text', { x: x.toFixed(1), y: y.toFixed(1), class: 'tl' });
        t.textContent = shortLabel(p, normToValue(p, n));
        svg.append(t);
      }
    }
    svg.append(svgEl('path', { d: arcPath(c, g.r, START, START + SWEEP), class: 'trk' }));
    this.valArc = svgEl('path', {
      d: arcPath(c, g.r, START, START + SWEEP), class: 'val', pathLength: '100', 'stroke-width': String(g.w),
    });
    this.modArc = svgEl('path', { d: arcPath(c, g.mod, START, START + SWEEP), class: 'mod', pathLength: '100' });
    svg.append(this.valArc, this.modArc, svgEl('circle', { cx: String(c), cy: String(c), r: String(g.r + 6), class: 'tring' }));

    // Cap + pointer: graphite for L/M, wire for S.
    this.rotor = svgEl('g', { class: 'rotor', style: og });
    if (o.tier === 'S') {
      this.rotor.append(
        svgEl('circle', { cx: String(c), cy: String(c), r: String(g.cap), class: 'cap-wire' }),
        svgEl('circle', { cx: String(c), cy: String(c), r: '1.1', class: 'cap-hub' }),
        svgEl('line', { x1: String(c), y1: String(c), x2: String(c), y2: String(c - g.r + 1.5), class: 'ptr-wire' }),
      );
    } else {
      svg.append(svgEl('circle', { cx: String(c), cy: String(c + 1), r: String(g.cap), class: 'cap-shadow' }));
      this.rotor.append(
        svgEl('circle', { cx: String(c), cy: String(c), r: String(g.cap), class: 'cap' }),
        svgEl('line', {
          x1: String(c), y1: (c - g.cap * 0.25).toFixed(2), x2: String(c), y2: String(c - g.cap + 3),
          class: 'ptr', 'stroke-width': o.tier === 'L' ? '2' : '1.6',
        }),
      );
    }
    this.marker = svgEl('g', { class: 'live-marker', style: og });
    this.marker.append(svgEl('circle', { cx: String(c), cy: String(c - g.r), r: String(g.live), class: 'live' }));
    svg.append(this.rotor, this.marker);

    // The focusable control sits over the drawing; it is what you drag.
    this.dial = el('button', 'knob') as HTMLButtonElement;
    this.dial.type = 'button';
    this.dial.setAttribute('role', 'slider');
    this.dial.setAttribute('aria-label', p.name);
    this.dial.setAttribute('aria-valuemin', String(p.minimum));
    this.dial.setAttribute('aria-valuemax', String(p.maximum));
    wrap.append(svg, this.dial);

    // Readout: name, value (the value you set), live value, source chips (L).
    const labels = el('div', 'knob-labels');
    labels.append(el('div', 'control-label', p.name));
    this.readout = el('button', 'control-readout') as HTMLButtonElement;
    this.readout.type = 'button';
    this.readout.dataset.testid = `readout-${o.moduleId}-${p.id}`;
    this.readout.title = 'Click to type a value';
    this.liveEl = el('div', 'knob-live');
    labels.append(this.readout, this.liveEl);
    this.chips = o.tier === 'L' ? el('div', 'knob-chips') : null;
    if (this.chips) labels.append(this.chips);
    this.root.append(wrap, labels);

    this.attachDrag();
    this.attachKeys();
    this.attachWheel();
    this.dial.addEventListener('dblclick', e => {
      e.preventDefault();
      this.o.setValue(p.default, true);
      this.refresh();
    });
    this.readout.addEventListener('click', () => this.promptValue());

    this.info = attachParamInfo(this.root, {
      sources: () => this.mods.map((m): InfoSource => ({
        name: this.o.describeSource(m),
        color: this.o.cableColor(m),
        sample: () => this.o.sourceValue(m),
        contribution: () => pct((m.amount ?? 0) * this.o.sourceValue(m)),
      })),
      sum: () => pct(valueToNorm(p, this.live) - valueToNorm(p, this.o.getValue())),
      result: () => text(p, this.live),
    }, o.ticker, `info-${o.moduleId}-${p.id}`);

    this.refresh();
    this.unbind = o.ticker.bind(o.moduleId, t => {
      const v = t.effective[o.moduleId]?.[p.id] ?? this.o.getValue();
      if (v === this.live) return;
      this.live = v;
      this.paintLive();
    });
  }

  destroy(): void { this.unbind(); this.info.destroy(); }

  /** Re-read base value and modulation and repaint everything. */
  refresh(): void {
    const p = this.o.param;
    const base = this.o.getValue();
    const n = valueToNorm(p, base);

    // Value arc from the natural origin (12 o'clock for bipolar) to the set value.
    const origin = p.minimum < 0 && p.maximum > 0 ? valueToNorm(p, 0) : 0;
    const a = Math.min(origin, n) * 100, l = Math.abs(n - origin) * 100;
    this.valArc.style.strokeDasharray = `0 ${a.toFixed(2)} ${l.toFixed(2)} 200`;
    this.rotor.style.transform = `rotate(${ang(n)}deg)`;

    // One combined modulation range: n ± Σ|amount| (DESIGN §5).
    this.mods = this.o.getMods();
    const sum = this.mods.reduce((s, m) => s + Math.abs(m.amount ?? 0), 0);
    const lo = clamp01(n - sum), hi = clamp01(n + sum);
    this.modArc.style.strokeDasharray = `0 ${(lo * 100).toFixed(2)} ${((hi - lo) * 100).toFixed(2)} 200`;
    const modded = this.mods.length > 0;
    this.root.classList.toggle('has-mod', modded);
    this.root.dataset.modCount = String(this.mods.length);
    if (this.chips) {
      this.chips.replaceChildren(...this.mods.map(m => el('i', '', this.o.describeSource(m).split(' · ')[0])));
    }
    if (!modded) { this.liveEl.textContent = ''; delete this.root.dataset.live; }

    this.live = base;
    this.paintBase(base);
    this.paintLive();
    this.updateTooltip(base);
  }

  /** Per-frame path: one transform; the live text at most ~14 Hz (Pi rules 1, 5). */
  private paintLive(): void {
    if (!this.mods.length) return;
    const p = this.o.param;
    this.marker.style.transform = `rotate(${ang(valueToNorm(p, this.live))}deg)`;
    this.root.dataset.live = String(round(this.live));
    const now = performance.now();
    if (now - this.lastText > 70) {
      this.lastText = now;
      this.liveEl.textContent = `→ ${text(p, this.live)}`;
    }
  }

  private paintBase(base: number): void {
    this.readout.textContent = text(this.o.param, base);
    this.dial.setAttribute('aria-valuenow', String(round(base)));
    this.dial.setAttribute('aria-valuetext', this.readout.textContent);
  }

  private updateTooltip(base: number): void {
    const p = this.o.param;
    const lines = [`${p.name}  ${text(p, base)}`, `Range ${formatRange(p)}`];
    if (this.mods.length) {
      lines.push(`Modulated by ${this.mods.map(c => `${this.o.describeSource(c)} (${pct(c.amount ?? 0)})`).join(', ')}`);
    } else if (p.modulatable) {
      lines.push('Drop a cable here to modulate');
    }
    lines.push('Drag to change · shift for fine · double-click resets');
    this.dial.title = lines.join('\n');
  }

  // ---- gestures ---------------------------------------------------------

  private attachDrag(): void {
    const p = this.o.param;
    this.dial.addEventListener('pointerdown', e => {
      if (e.button !== 0) return;
      e.preventDefault(); e.stopPropagation();
      this.dial.setPointerCapture(e.pointerId);
      this.dial.focus({ preventScroll: true });
      const startY = e.clientY;
      const startNorm = valueToNorm(p, this.o.getValue());
      this.root.classList.add('dragging');

      const move = (ev: PointerEvent) => {
        if (ev.pointerId !== e.pointerId) return;
        // 180 px of travel covers the full range; shift is ~4.6x finer (≈830 px).
        const scale = (ev.shiftKey ? 0.217 : 1) / 180;
        const n = clamp01(startNorm + (startY - ev.clientY) * scale);
        this.o.setValue(normToValue(p, n), false);
        this.refresh();
      };
      const up = () => {
        this.dial.removeEventListener('pointermove', move);
        this.dial.removeEventListener('pointerup', up);
        this.dial.removeEventListener('pointercancel', up);
        this.root.classList.remove('dragging');
        this.o.setValue(this.o.getValue(), true);
      };
      this.dial.addEventListener('pointermove', move);
      this.dial.addEventListener('pointerup', up);
      this.dial.addEventListener('pointercancel', up);
    });
  }

  private attachKeys(): void {
    const p = this.o.param;
    this.dial.addEventListener('keydown', e => {
      const keys = ['ArrowUp', 'ArrowRight', 'ArrowDown', 'ArrowLeft', 'Home', 'End', 'PageUp', 'PageDown'];
      if (!keys.includes(e.key)) return;
      e.preventDefault();
      const n = valueToNorm(p, this.o.getValue());
      const fine = e.shiftKey ? 0.002 : 0.01;
      let next = n;
      switch (e.key) {
        case 'ArrowUp': case 'ArrowRight': next = n + fine; break;
        case 'ArrowDown': case 'ArrowLeft': next = n - fine; break;
        case 'PageUp': next = n + 0.1; break;
        case 'PageDown': next = n - 0.1; break;
        case 'Home': next = 0; break;
        case 'End': next = 1; break;
      }
      this.o.setValue(normToValue(p, clamp01(next)), true);
      this.refresh();
    });
  }

  private attachWheel(): void {
    const p = this.o.param;
    this.dial.addEventListener('wheel', e => {
      e.preventDefault(); e.stopPropagation();
      const n = valueToNorm(p, this.o.getValue());
      const delta = (e.deltaY > 0 ? -1 : 1) * (e.shiftKey ? 0.004 : 0.02);
      this.o.setValue(normToValue(p, clamp01(n + delta)), true);
      this.refresh();
    }, { passive: false });
  }

  private promptValue(): void {
    const p = this.o.param;
    const input = el('input', 'readout-input') as HTMLInputElement;
    input.value = displayValue(p, this.o.getValue());
    input.dataset.testid = `readout-input-${this.o.moduleId}-${p.id}`;
    input.setAttribute('aria-label', `${p.name} value`);
    this.readout.replaceWith(input);
    input.focus(); input.select();
    let done = false;
    const finish = (apply: boolean) => {
      if (done) return;
      done = true;
      if (apply) {
        const v = parseTyped(p, input.value, this.o.getValue());
        if (v !== null) this.o.setValue(v, true);
      }
      input.replaceWith(this.readout);
      this.refresh();
    };
    input.addEventListener('keydown', e => {
      if (e.key === 'Enter') { e.preventDefault(); finish(true); }
      if (e.key === 'Escape') { e.preventDefault(); finish(false); }
    });
    input.addEventListener('blur', () => finish(true));
  }
}

/* ---- geometry and labels ----------------------------------------------- */

const ang = (n: number) => START + clamp01(n) * SWEEP;

function pol(c: number, r: number, deg: number): [number, number] {
  const a = ((deg - 90) * Math.PI) / 180;
  return [c + r * Math.cos(a), c + r * Math.sin(a)];
}

function arcPath(c: number, r: number, a0: number, a1: number): string {
  const [x0, y0] = pol(c, r, a0), [x1, y1] = pol(c, r, a1);
  return `M${x0.toFixed(2)} ${y0.toFixed(2)}A${r} ${r} 0 ${a1 - a0 > 180 ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

function radial(c: number, r0: number, r1: number, a: number): string {
  const [x0, y0] = pol(c, r0, a), [x1, y1] = pol(c, r1, a);
  return `M${x0.toFixed(2)} ${y0.toFixed(2)}L${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

/** Major marks, in travel 0..1: decades for log Hz, otherwise ends + centre (L: tenths). */
function scaleMarks(p: ModuleParam, tier: KnobTier): number[] {
  if (p.taper === 'log' && p.minimum > 0) {
    const out = [0];
    for (let d = Math.pow(10, Math.ceil(Math.log10(p.minimum))); d < p.maximum; d *= 10) {
      const n = valueToNorm(p, d);
      if (n > 0.04 && n < 0.96) out.push(n);
    }
    out.push(1);
    return out;
  }
  if (tier === 'L') return [0, 0.25, 0.5, 0.75, 1];
  return [0, 0.5, 1];
}

/** Tidy scale label: 20, 100, 1k, 20k. */
function shortLabel(p: ModuleParam, raw: number): string {
  const v = Math.abs(raw) >= 10 ? Math.round(raw) : raw;
  const a = Math.abs(v);
  if (p.unit === 'Hz' && a >= 1000) return `${+(v / 1000).toFixed(a >= 10000 ? 0 : 1)}k`;
  if (p.unit === '' && p.maximum <= 2 && p.minimum >= -1) return String(Math.round(v * 100));
  if (p.unit === 's' && a < 1) return `${Math.round(v * 1000)}m`;
  return String(+v.toFixed(a < 10 ? 1 : 0));
}

function text(p: ModuleParam, v: number): string {
  const f = formatValue(p, v);
  return f.unit === "%" ? `${f.text}%` : f.unit ? `${f.text} ${f.unit}` : f.text;
}

const clamp01 = (n: number) => (n < 0 ? 0 : n > 1 ? 1 : n);
const round = (n: number) => Math.round(n * 1e6) / 1e6;
const pct = (n: number) => {
  const r = Math.round(n * 100);
  return r === 0 ? '0%' : `${r > 0 ? '+' : '−'}${Math.abs(r)}%`;
};
