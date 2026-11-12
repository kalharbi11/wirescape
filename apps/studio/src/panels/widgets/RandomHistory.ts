// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';
import { el } from '../../app/dom';
import { CanvasPane, dotGrid } from './canvasBase';

const CAPACITY = 96;
const INSET = 3;

/**
 * Measured Random Source output history, not an invented LFO phase.
 * Each telemetry tick appends at most one pair of values to fixed-size rings.
 * Step is drawn as a hold; smooth is joined continuously. Missing values are
 * gaps, never synthetic zeroes or a sine reference.
 */
export class RandomHistory {
  readonly root: HTMLElement;
  private readonly pane = new CanvasPane('wave-canvas');
  private readonly readout = el('span', 'disp-readout', 'Waiting for output telemetry');
  private readonly step = new Float32Array(CAPACITY);
  private readonly smooth = new Float32Array(CAPACITY);
  private readonly unbind: () => void;
  private grid: HTMLCanvasElement | null = null;
  private cursor = 0;
  private count = 0;
  private lastTick = -1;
  private lastText = -1;
  private stepChanges = 0;
  private previousStep = NaN;

  constructor(moduleId: number, ticker: Ticker) {
    this.root = el('div', 'wave-pane random-history');
    this.root.dataset.testid = `random-history-${moduleId}`;
    this.root.dataset.samples = '0';
    this.root.dataset.stepChanges = '0';
    this.pane.onResize = () => { this.grid = null; };
    this.pane.root.append(this.readout);
    this.root.append(this.pane.root);
    this.unbind = ticker.bind(moduleId, t => {
      if (t.tick === this.lastTick) return;
      this.lastTick = t.tick;
      const step = t.outputs?.[`${moduleId}:step`];
      const smooth = t.outputs?.[`${moduleId}:smooth`];
      const a = typeof step === 'number' && Number.isFinite(step) ? step : NaN;
      const b = typeof smooth === 'number' && Number.isFinite(smooth) ? smooth : NaN;
      if (!Number.isFinite(a) && !Number.isFinite(b)) {
        this.readout.textContent = 'Connect step or smooth to view live values';
        return;
      }
      if (Number.isFinite(a) && Number.isFinite(this.previousStep) && a !== this.previousStep)
        this.stepChanges++;
      this.previousStep = a;
      this.step[this.cursor] = a;
      this.smooth[this.cursor] = b;
      this.cursor = (this.cursor + 1) % CAPACITY;
      this.count = Math.min(CAPACITY, this.count + 1);
      this.root.dataset.samples = String(this.count);
      this.root.dataset.stepChanges = String(this.stepChanges);
      this.draw();
      if (this.lastText < 0 || t.time < this.lastText || t.time - this.lastText >= 1 / 15) {
        this.lastText = t.time;
        const fmt = (v: number) => Number.isFinite(v) ? (v >= 0 ? '+' : '') + v.toFixed(2) : '—';
        this.readout.textContent = `STEP ${fmt(a)}  SMOOTH ${fmt(b)}`;
      }
    });
  }

  private draw(): void {
    const p = this.pane;
    if (!p.width) { p.resize(); if (!p.width) return; }
    const { ctx } = p, w = p.width, h = p.height, pal = p.palette();
    this.grid ??= dotGrid(w, h, p.dpr, pal.mute);
    p.clear();
    ctx.drawImage(this.grid, 0, 0, w, h);
    const n = this.count;
    if (n < 2) return;
    const start = (this.cursor - n + CAPACITY) % CAPACITY;
    const xAt = (i: number) => INSET + i * (w - 2 * INSET) / (CAPACITY - 1);
    const yAt = (v: number) => h / 2 - Math.max(-1, Math.min(1, v)) * h * 0.34;
    const trace = (values: Float32Array, stepped: boolean, color: string) => {
      ctx.beginPath();
      let previous = NaN;
      for (let i = 0; i < n; i++) {
        const v = values[(start + i) % CAPACITY]!;
        if (!Number.isFinite(v)) { previous = NaN; continue; }
        const x = xAt(CAPACITY - n + i), y = yAt(v);
        if (!Number.isFinite(previous)) ctx.moveTo(x, y);
        else {
          if (stepped) ctx.lineTo(x, yAt(previous));
          ctx.lineTo(x, y);
        }
        previous = v;
      }
      ctx.strokeStyle = color;
      ctx.lineWidth = 1.3;
      ctx.lineJoin = 'round';
      ctx.stroke();
    };
    trace(this.step, true, pal.ink);
    trace(this.smooth, false, pal.rust);
  }

  destroy(): void { this.unbind(); this.pane.destroy(); }
}
