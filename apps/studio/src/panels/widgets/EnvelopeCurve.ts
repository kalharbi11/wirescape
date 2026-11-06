import type { Ticker } from '../../runtime/Ticker';
import { CanvasPane } from './canvasBase';
import { el } from '../../app/dom';

export interface EnvelopeIds { attack: string; decay: string; sustain?: string; release?: string }

export interface EnvelopeOptions {
  moduleId: number;
  ticker: Ticker;
  /** Param ids to edit. Without sustain+release the curve is a rise/fall pair. */
  ids: EnvelopeIds;
  get(id: string): number;
  set(id: string, v: number, commit: boolean): void;
  range(id: string): { min: number; max: number };
}

type Stage = 'I' | 'A' | 'D' | 'S' | 'R';

const TOP = 10, BOT = 16;

/**
 * Envelope card: baseline with segment ticks, a 1 px curve, hollow handles you
 * can drag, a dotted sustain guide, A D S R letters (the active one in rust)
 * and a rust voice dot driven by telemetry.
 */
export class EnvelopeCurve {
  readonly root: HTMLElement;
  private pane: CanvasPane;
  private unbind: () => void;
  private level = 0;
  private stage: Stage = 'I';
  private sig = '';
  private geo = { xA: 0, xD: 0, xS: 0, xR: 0 };
  private drag = -1;
  private local: Record<string, number> = {};
  private readonly adsr: boolean;
  private readonly letters: string[];

  constructor(private readonly o: EnvelopeOptions) {
    this.adsr = !!(o.ids.sustain && o.ids.release);
    this.letters = this.adsr ? ['A', 'D', 'S', 'R'] : ['R', 'F'];
    this.root = el('div', 'env-pane');
    this.root.dataset.testid = `envelope-${o.moduleId}`;
    this.pane = new CanvasPane('env-canvas');
    this.pane.onResize = () => { this.sig = ''; };
    this.root.append(this.pane.root);
    this.bindPointer();
    this.unbind = o.ticker.bind(o.moduleId, t => {
      const v = t.envelope[o.moduleId];
      if (v !== undefined) this.track(v);
      this.draw();
    });
  }

  private val(id: string): number { return id in this.local ? this.local[id]! : this.o.get(id); }

  /** Infer the stage from the level's direction; telemetry carries no stage. */
  private track(v: number): void {
    const prev = this.level, sus = this.adsr ? this.norm(this.o.ids.sustain!) : 0;
    this.level = v;
    let st = this.stage;
    if (v < 0.002) st = 'I';
    else if (st === 'I') st = 'A';
    else if (v > prev + 1e-4) st = 'A';
    else if (v < prev - 1e-4) {
      if (st === 'A') st = 'D';
      else if (st === 'S') st = 'R';
      else if (st === 'D' && this.adsr && v < sus - 0.03) st = 'R';
    } else if (st === 'D' && this.adsr && Math.abs(v - sus) < 0.03) st = 'S';
    else if (st === 'A' && v > 0.98) st = 'D';
    this.stage = st;
  }

  private norm(id: string): number {
    const r = this.o.range(id);
    return r.max > r.min ? (this.val(id) - r.min) / (r.max - r.min) : 0;
  }
  /** Time params map log-wise onto 0..1 so milliseconds and seconds both stay draggable. */
  private tNorm(id: string): number {
    const { min, max } = this.o.range(id), t = Math.max(min, this.val(id));
    return Math.min(1, Math.max(0, Math.log(t / min) / Math.log(max / min)));
  }
  private tFrom(id: string, n: number): number {
    const { min, max } = this.o.range(id);
    return min * Math.pow(max / min, Math.min(1, Math.max(0, n)));
  }

  private geometry(W: number) {
    const i = this.o.ids;
    const nA = this.tNorm(i.attack), nD = this.tNorm(i.decay);
    if (!this.adsr) {
      const xA = (0.02 + nA * 0.42) * W;
      return { xA, xD: xA, xS: xA, xR: xA + (0.02 + nD * 0.46) * W };
    }
    const xA = (0.02 + nA * 0.26) * W, xD = xA + (0.02 + nD * 0.24) * W, xS = xD + 0.16 * W;
    return { xA, xD, xS, xR: xS + (0.02 + this.tNorm(i.release!) * 0.26) * W };
  }

  private draw(): void {
    const pane = this.pane;
    if (!pane.width) { pane.resize(); if (!pane.width) return; }
    const i = this.o.ids;
    const sig = [this.val(i.attack), this.val(i.decay), i.sustain ? this.val(i.sustain) : 0,
      i.release ? this.val(i.release) : 0, Math.round(this.level * 400), this.stage].join('|');
    if (sig === this.sig) return;
    this.sig = sig;

    const w = pane.width, h = pane.height, ctx = pane.ctx, pal = pane.palette();
    const hh = h - TOP - BOT, Y = (l: number) => TOP + (1 - l) * hh;
    const g = this.geo = this.geometry(w);
    const sv = this.adsr ? this.norm(i.sustain!) : 0;
    pane.clear();

    ctx.lineWidth = 0.6; ctx.strokeStyle = pal.graph;
    ctx.beginPath(); ctx.moveTo(0, Y(0) + 0.5); ctx.lineTo(w, Y(0) + 0.5); ctx.stroke();
    ctx.lineWidth = 0.5; ctx.strokeStyle = pal.hair;
    ctx.beginPath();
    for (const x of this.adsr ? [g.xA, g.xD, g.xS, g.xR] : [g.xA, g.xR]) {
      ctx.moveTo(Math.round(x) + 0.5, Y(0) - 3); ctx.lineTo(Math.round(x) + 0.5, Y(0) + 3);
    }
    ctx.stroke();
    if (this.adsr) {
      ctx.setLineDash([1, 3]);
      ctx.beginPath(); ctx.moveTo(g.xD, Y(sv) + 0.5); ctx.lineTo(g.xS + 20, Y(sv) + 0.5); ctx.stroke();
      ctx.setLineDash([]);
    }

    // The curve: linear attack, exponential decay / release as in the kit.
    ctx.beginPath(); ctx.moveTo(0, Y(0)); ctx.lineTo(g.xA, Y(1));
    if (this.adsr) {
      for (let q = 0.05; q <= 1.001; q += 0.05) ctx.lineTo(g.xA + (g.xD - g.xA) * q, Y(sv + (1 - sv) * Math.exp(-5 * q)));
      ctx.lineTo(g.xS, Y(sv));
    }
    const x0 = this.adsr ? g.xS : g.xA;
    for (let q = 0.05; q <= 1.001; q += 0.05) ctx.lineTo(x0 + (g.xR - x0) * q, Y((this.adsr ? sv : 1) * Math.exp(-5 * q)));
    ctx.strokeStyle = pal.ink; ctx.lineWidth = 1; ctx.lineJoin = 'round'; ctx.stroke();

    // Letters.
    const xs = this.adsr
      ? [g.xA / 2, (g.xA + g.xD) / 2, (g.xD + g.xS) / 2, (g.xS + g.xR) / 2]
      : [g.xA / 2, (g.xA + g.xR) / 2];
    const active = this.adsr ? 'ADSR'.indexOf(this.stage) : this.stage === 'A' ? 0 : this.stage === 'I' ? -1 : 1;
    ctx.font = `500 8px ${pal.mono}`; ctx.textAlign = 'center';
    this.letters.forEach((t, k) => {
      ctx.fillStyle = k === active ? pal.rust : pal.mute;
      ctx.fillText(t, xs[k]!, h - 3);
    });

    // Voice dot.
    if (this.stage !== 'I' && this.level > 0.002) {
      const lv = this.level, sus = this.adsr ? sv : 0;
      let x: number;
      const q5 = (r: number) => Math.min(1, Math.max(0, -Math.log(Math.max(1e-4, r)) / 5));
      switch (this.stage) {
        case 'A': x = g.xA * Math.min(1, lv); break;
        case 'D': x = g.xA + (g.xD - g.xA) * q5((lv - sus) / Math.max(1e-3, 1 - sus)); break;
        case 'S': x = (g.xD + g.xS) / 2; break;
        default: x = this.adsr ? g.xS + (g.xR - g.xS) * q5(lv / Math.max(1e-3, sus)) : g.xA + (g.xR - g.xA) * q5(lv);
      }
      ctx.fillStyle = pal.rust;
      ctx.beginPath(); ctx.arc(x, Y(lv), 2.6, 0, Math.PI * 2); ctx.fill();
    }

    // Hollow handles.
    ctx.fillStyle = pal.card; ctx.strokeStyle = pal.ink; ctx.lineWidth = 1;
    for (const [x, y] of this.handles(Y, sv)) {
      ctx.beginPath(); ctx.arc(x, y, 3.2, 0, Math.PI * 2); ctx.fill(); ctx.stroke();
    }
  }

  private handles(Y: (l: number) => number, sv: number): [number, number][] {
    const g = this.geo;
    return this.adsr ? [[g.xA, Y(1)], [g.xD, Y(sv)], [g.xR, Y(0)]] : [[g.xA, Y(1)], [g.xR, Y(0)]];
  }

  private bindPointer(): void {
    const cv = this.pane.canvas;
    cv.style.touchAction = 'none'; cv.style.cursor = 'crosshair';
    let scale = 1, left = 0, top = 0;
    const pos = (e: PointerEvent) => ({ x: (e.clientX - left) / scale, y: (e.clientY - top) / scale });
    const Y = (l: number) => TOP + (1 - l) * (this.pane.height - TOP - BOT);
    const edit = (id: string, v: number, commit: boolean) => {
      const { min, max } = this.o.range(id);
      const c = Math.min(max, Math.max(min, v));
      this.local[id] = c; this.o.set(id, c, commit);
    };
    cv.addEventListener('pointerdown', e => {
      if (!this.pane.width) return;
      const r = cv.getBoundingClientRect();
      scale = r.width / this.pane.width || 1; left = r.left; top = r.top;
      const p = pos(e), sv = this.adsr ? this.norm(this.o.ids.sustain!) : 0;
      let best = 14; this.drag = -1;
      this.handles(Y, sv).forEach(([hx, hy], k) => {
        const d = Math.hypot(hx - p.x, hy - p.y);
        if (d < best) { best = d; this.drag = k; }
      });
      if (this.drag >= 0) { cv.setPointerCapture(e.pointerId); e.stopPropagation(); e.preventDefault(); }
    });
    cv.addEventListener('pointermove', e => {
      if (this.drag < 0) return;
      const { x, y } = pos(e), W = this.pane.width, g = this.geo, i = this.o.ids;
      const t = (dx: number, span: number) => (dx / W - 0.02) / span;
      if (this.adsr) {
        if (this.drag === 0) edit(i.attack, this.tFrom(i.attack, t(x, 0.26)), false);
        else if (this.drag === 1) {
          edit(i.decay, this.tFrom(i.decay, t(x - g.xA, 0.24)), false);
          const r = this.o.range(i.sustain!);
          edit(i.sustain!, r.min + (r.max - r.min) * Math.min(1, Math.max(0, (Y(0) - y) / (Y(0) - Y(1)))), false);
        } else edit(i.release!, this.tFrom(i.release!, t(x - g.xS, 0.26)), false);
      } else if (this.drag === 0) edit(i.attack, this.tFrom(i.attack, t(x, 0.42)), false);
      else edit(i.decay, this.tFrom(i.decay, t(x - g.xA, 0.46)), false);
      this.sig = ''; this.draw();
    });
    const end = () => {
      if (this.drag < 0) return;
      this.drag = -1;
      for (const id of Object.keys(this.local)) this.o.set(id, this.local[id]!, true);
      this.local = {}; this.sig = '';
    };
    cv.addEventListener('pointerup', end);
    cv.addEventListener('pointercancel', end);
  }

  refresh(): void { this.sig = ''; }
  destroy(): void { this.unbind(); this.pane.destroy(); }
}
