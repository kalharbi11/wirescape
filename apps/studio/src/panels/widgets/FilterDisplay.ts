// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';
import { CanvasPane, makeLayer } from './canvasBase';
import { el } from '../../app/dom';

export interface FilterOptions {
  moduleId: number;
  ticker: Ticker;
  /** Param ids. */
  cutoff: string;
  resonance: string;
  /** Optional filter-mode param (0 LP, 1 BP, 2 HP, 3 notch). Default low-pass. */
  mode?: string;
  /** Offer the KNOBS / GRAPH header switch. */
  graph: boolean;
  get(id: string): number;
  set(id: string, v: number, commit: boolean): void;
  range(id: string): { min: number; max: number };
}

const FMIN = 20, FRANGE = 1000; // 20 Hz .. 20 kHz, log axis
const clamp = (v: number, a = 0, b = 1) => Math.min(b, Math.max(a, v));

/** The kit's vcfResp: gain in dB of a 2-pole filter at frequency f. */
function resp(f: number, fc: number, res: number, mode: number): number {
  const Q = 0.5 * Math.pow(36, res), x = f / fc;
  const den = Math.sqrt((1 - x * x) ** 2 + (x / Q) ** 2) + 1e-9;
  let m: number;
  switch (mode) {
    case 0: m = 1 / den; break;
    case 1: m = (x / Q) / den; break;
    case 2: m = x * x / den; break;
    default: m = Math.abs(1 - x * x) / den + 1e-3;
  }
  return 20 * Math.log10(m);
}

const fmtHz = (f: number) => f >= 1000 ? `${(f / 1000).toFixed(f >= 10000 ? 1 : 2)}k` : `${Math.round(f)}`;

/**
 * Filter response. Default is the thin hero-led strip; with `graph` the header
 * switch grows it to fill the panel core, where the peak handle edits cutoff
 * and resonance. Redraws only when cutoff / resonance (set or live) change.
 */
export class FilterDisplay {
  readonly root: HTMLElement;
  readonly headerEl?: HTMLElement;
  private pane: CanvasPane;
  private unbind: () => void;
  private graphMode = false;
  private live = { c: NaN, r: NaN };
  private sig = '';
  private dragState: { c: number; r: number } | null = null;
  private layer: HTMLCanvasElement | null = null;
  private layerKey = '';
  private btns: HTMLButtonElement[] = [];
  private ind: HTMLElement | null = null;

  constructor(private readonly o: FilterOptions) {
    this.root = el('div', 'filter-pane');
    this.root.dataset.testid = `filter-${o.moduleId}`;
    this.pane = new CanvasPane('filter-canvas');
    this.pane.onResize = () => { this.layer = null; this.sig = ''; };
    this.root.append(this.pane.root);
    if (o.graph) this.headerEl = this.buildSwitch();
    this.bindPointer();
    this.unbind = o.ticker.bind(o.moduleId, t => {
      const e = t.effective[o.moduleId];
      if (e) {
        if (e[o.cutoff] !== undefined) this.live.c = e[o.cutoff]!;
        if (e[o.resonance] !== undefined) this.live.r = e[o.resonance]!;
      }
      this.draw();
    });
  }

  private buildSwitch(): HTMLElement {
    const seg = el('div', 'fd-seg');
    seg.style.setProperty('--n', '2');
    this.ind = el('i', 'fd-seg-i');
    seg.append(this.ind);
    (['knobs', 'graph'] as const).forEach((k, i) => {
      const b = el('button', i === 0 ? 'on' : '', k.toUpperCase()) as HTMLButtonElement;
      b.type = 'button';
      b.dataset.testid = `view-${this.o.moduleId}-${k}`;
      b.addEventListener('pointerdown', e => e.stopPropagation());
      b.addEventListener('click', e => { e.stopPropagation(); this.setGraph(i === 1); });
      this.btns.push(b); seg.append(b);
    });
    return seg;
  }

  private setGraph(on: boolean): void {
    if (this.graphMode === on) return;
    const core = this.root.closest<HTMLElement>('.panel-core');
    if (on) {
      // Read the panel's size once, before its other rows are hidden.
      if (core) {
        const h = core.clientHeight;
        this.root.style.height = `${Math.max(120, h)}px`;
        core.classList.add('graph-mode');
      }
    } else {
      core?.classList.remove('graph-mode');
      this.root.style.height = '';
    }
    this.graphMode = on;
    this.root.classList.toggle('graph', on);
    this.btns.forEach((b, i) => b.classList.toggle('on', (i === 1) === on));
    if (this.ind) this.ind.style.transform = on ? 'translateX(100%)' : 'none';
    this.layer = null; this.sig = '';
    this.pane.resize();
    this.draw();
  }

  private norm(): number {
    const r = this.o.range(this.o.resonance);
    return r.max > r.min ? r.max : 1;
  }

  private draw(): void {
    const pane = this.pane;
    if (!pane.width) { pane.resize(); if (!pane.width) return; }
    const o = this.o, d = this.dragState;
    const cs = d ? d.c : o.get(o.cutoff), rs = d ? d.r : o.get(o.resonance);
    const cl = d || isNaN(this.live.c) ? cs : this.live.c;
    const rl = d || isNaN(this.live.r) ? rs : this.live.r;
    const mode = o.mode ? Math.round(o.get(o.mode)) : 0;
    const sig = `${cs}|${rs}|${cl}|${rl}|${mode}`;
    if (sig === this.sig) return;
    this.sig = sig;

    const w = pane.width, h = pane.height, ctx = pane.ctx, pal = pane.palette();
    const graph = this.graphMode, rmax = this.norm();
    const fx = (f: number) => Math.log(f / FMIN) / Math.log(FRANGE) * w;
    const dy = (db: number) => clamp(h * (1 - (db + 30) / 48), 0, h);

    const key = `${w}x${h}:${graph}`;
    if (!this.layer || key !== this.layerKey) {
      const L = makeLayer(w, h, pane.dpr), c = L.ctx;
      c.lineWidth = 0.5;
      for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
        const x = Math.round(fx(f)) + 0.5;
        c.strokeStyle = f === 100 || f === 1000 || f === 10000 ? pal.hair : pal.hair2;
        c.beginPath(); c.moveTo(x, 0); c.lineTo(x, h); c.stroke();
      }
      c.setLineDash([2, 3]); c.strokeStyle = pal.hair;
      c.beginPath(); c.moveTo(0, dy(0) + 0.5); c.lineTo(w, dy(0) + 0.5); c.stroke(); c.setLineDash([]);
      if (graph) {
        c.font = `8px ${pal.mono}`; c.fillStyle = pal.mute; c.textAlign = 'left';
        for (const [f, t] of [[100, '100'], [1000, '1k'], [10000, '10k']] as const) c.fillText(t, fx(f) + 2, h - 3);
        c.fillText('0 dB', 2, dy(0) - 3);
      }
      this.layer = L.cv; this.layerKey = key;
    }
    pane.clear();
    ctx.drawImage(this.layer, 0, 0, w, h);

    const path = (fc: number, res: number) => {
      ctx.beginPath();
      for (let i = 0; i <= 110; i++) {
        const f = FMIN * Math.pow(FRANGE, i / 110), y = dy(resp(f, fc, res, mode));
        i ? ctx.lineTo(i / 110 * w, y) : ctx.moveTo(0, y);
      }
    };
    const rS = clamp(rs / rmax), rL = clamp(rl / rmax);
    const modded = cl !== cs || rl !== rs;
    if (modded) {
      path(cs, rS); ctx.strokeStyle = pal.mute; ctx.lineWidth = 0.6; ctx.setLineDash([2, 2]); ctx.stroke(); ctx.setLineDash([]);
    }
    path(cl, rL); ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.closePath();
    ctx.fillStyle = pal.rust2; ctx.fill();
    path(cl, rL); ctx.strokeStyle = pal.ink; ctx.lineWidth = 1.3; ctx.lineJoin = 'round'; ctx.stroke();

    let x = fx(cs);
    ctx.strokeStyle = pal.graph; ctx.lineWidth = 0.6; ctx.setLineDash([1, 2]);
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); ctx.setLineDash([]);
    if (cl !== cs) {
      x = fx(cl); ctx.strokeStyle = pal.rust; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke();
    }
    if (graph) {
      const pk = dy(resp(cs, cs, rS, mode));
      ctx.fillStyle = pal.card; ctx.strokeStyle = pal.ink; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.arc(fx(cs), clamp(pk, 6, h - 6), 4.5, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
      ctx.fillStyle = pal.rust; ctx.font = `9px ${pal.mono}`; ctx.textAlign = 'right';
      ctx.fillText(`fc ${fmtHz(cl)} Hz · Q ${(0.5 * Math.pow(36, rL)).toFixed(2)}`, w - 4, 10);
    }
  }

  private bindPointer(): void {
    const cv = this.pane.canvas, o = this.o;
    let box: { l: number; t: number; s: number } | null = null;
    const set = (e: PointerEvent) => {
      if (!box) return;
      const w = this.pane.width, h = this.pane.height;
      const x = clamp((e.clientX - box.l) / box.s / w), y = clamp((e.clientY - box.t) / box.s / h);
      const cr = o.range(o.cutoff), rr = o.range(o.resonance);
      const c = clamp(FMIN * Math.pow(FRANGE, x), cr.min, cr.max);
      const r = clamp(1.15 - y * 1.3) * rr.max;
      this.dragState = { c, r };
      o.set(o.cutoff, c, false); o.set(o.resonance, r, false);
      this.draw();
    };
    cv.addEventListener('pointerdown', e => {
      if (!this.graphMode) return;
      const r = cv.getBoundingClientRect();
      box = { l: r.left, t: r.top, s: r.width / (this.pane.width || 1) || 1 };
      cv.setPointerCapture(e.pointerId); e.stopPropagation(); e.preventDefault();
      set(e);
    });
    cv.addEventListener('pointermove', e => { if (box) set(e); });
    const end = () => {
      if (!box) return;
      box = null;
      const d = this.dragState; this.dragState = null;
      if (d) { o.set(o.cutoff, d.c, true); o.set(o.resonance, d.r, true); }
      this.sig = '';
    };
    cv.addEventListener('pointerup', end);
    cv.addEventListener('pointercancel', end);
  }

  refresh(): void { this.sig = ''; }

  destroy(): void {
    this.unbind(); this.pane.destroy();
    this.root.closest('.panel-core')?.classList.remove('graph-mode');
  }
}
