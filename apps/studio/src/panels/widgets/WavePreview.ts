// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';
import { CanvasPane, dotGrid } from './canvasBase';
import { lfoTraceValue } from './waveTrace';
import { el } from '../../app/dom';

export interface WaveOptions {
  moduleId: number;
  ticker: Ticker;
  /** Called for the current shape parameters. */
  shape(): { depth: number; offset: number; rateLabel: string };
}

const INSET = 3;

/**
 * LFO scope: a two-cycle scrolling trace driven only by received engine
 * phase. The live output sample updates the numeric readout only: splicing
 * it into the last trace pixel would create a misleading moving kink when
 * phase is unavailable. No local clock invents phase or signal motion.
 */
export class WavePreview {
  readonly root: HTMLElement;
  private pane: CanvasPane;
  private readout: HTMLElement;
  private unbind: () => void;
  private phase: number | null = null;
  private output: number | null = null;
  private drawnPhase: number | null = null;
  private layer: HTMLCanvasElement | null = null;
  private layerKey = '';
  private lastText = 0;
  private lastShape = { depth: NaN, offset: NaN };

  constructor(private readonly o: WaveOptions) {
    this.root = el('div', 'wave-pane');
    this.root.dataset.testid = `wave-${o.moduleId}`;
    this.pane = new CanvasPane('wave-canvas');
    this.pane.onResize = () => { this.layer = null; this.drawnPhase = null; };
    this.readout = el('span', 'disp-readout', '');
    this.pane.root.append(this.readout);
    this.root.append(this.pane.root);
    this.unbind = o.ticker.bind(o.moduleId, t => {
      const p = t.phase[o.moduleId];
      this.phase = typeof p === 'number' && Number.isFinite(p) && p >= 0 && p < 1 ? p : null;
      if (this.phase === null) delete this.root.dataset.phase;
      else this.root.dataset.phase = String(this.phase);
      const out = t.outputs?.[`${o.moduleId}:control`];
      this.output = typeof out === 'number' && Number.isFinite(out) ? out : null;
      this.draw(t.time);
    });
  }

  private draw(time: number): void {
    const pane = this.pane;
    if (!pane.width) { pane.resize(); if (!pane.width) return; }
    const s = this.o.shape();
    const changed = s.depth !== this.lastShape.depth || s.offset !== this.lastShape.offset;
    if (changed) {
      this.lastShape = { depth: s.depth, offset: s.offset };
      this.root.title = s.rateLabel;
    }
    const ph = this.phase;
    if (!changed && this.layer && ph === this.drawnPhase) {
      this.updateReadout(time);
      return;
    }
    this.drawnPhase = ph;

    const w = pane.width, h = pane.height, ctx = pane.ctx, pal = pane.palette();
    const x0 = INSET, x1 = w - INSET, cy = h / 2, A = h * 0.34;
    const key = `${w}x${h}:${pane.dpr}`;
    if (!this.layer || key !== this.layerKey) {
      this.layer = dotGrid(w, h, pane.dpr, pal.mute);
      this.layerKey = key;
    }
    pane.clear();
    ctx.drawImage(this.layer, 0, 0, w, h);

    // Only this single trace path changes per ticker frame. It is not a
    // bitmap that can be enlarged by patch zoom without a fresh backing store.
    ctx.beginPath();
    const latestPhase = ph ?? 0;
    for (let x = x0; x <= x1; x++) {
      const u = (x - x0) / (x1 - x0);
      const output = lfoTraceValue(latestPhase, u, s.depth, s.offset);
      const y = cy - Math.max(-1, Math.min(1, output)) * A;
      x === x0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.strokeStyle = pal.ink;
    ctx.lineWidth = 1.1;
    ctx.lineJoin = 'round';
    ctx.stroke();

    // The rust dot is a measured position only, never a locally advanced cursor.
    if (ph !== null) {
      const value = lfoTraceValue(ph, 1, s.depth, s.offset);
      const y = cy - Math.max(-1, Math.min(1, value)) * A;
      ctx.beginPath();
      ctx.arc(x1, y, 3, 0, Math.PI * 2);
      ctx.fillStyle = pal.rust;
      ctx.fill();
    }

    this.updateReadout(time);
  }

  /** Live amplitude may update the label, never the phase-based trace. */
  private updateReadout(time: number): void {
    if (this.phase === null) {
      this.readout.textContent = 'phase not available';
      return;
    }
    if (time - this.lastText < 1 / 15 && time >= this.lastText) return;
    this.lastText = time;
    // An output value is shown only if the engine also reported that output.
    const degrees = Math.round(this.phase * 360);
    const v = this.output;
    this.readout.textContent = v === null ? `φ ${degrees}°`
      : `φ ${degrees}° ${v >= 0 ? '+' : '−'}${Math.abs(v).toFixed(2)}`;
  }

  destroy(): void { this.unbind(); this.pane.destroy(); }
}
