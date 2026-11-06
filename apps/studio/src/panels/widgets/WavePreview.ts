// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';
import { CanvasPane, dotGrid, makeLayer } from './canvasBase';
import { el } from '../../app/dom';

export interface WaveOptions {
  moduleId: number;
  ticker: Ticker;
  /** Called for the current shape parameters. */
  shape(): { depth: number; offset: number; rateLabel: string };
}

const CYCLES = 2;
const INSET = 3;

/**
 * LFO card: the waveform on a fading dot grid, with a phase dot driven
 * by the engine's real phase. The static layer (grid + wave) is cached and only
 * rebuilt when the shape or size changes.
 */
export class WavePreview {
  readonly root: HTMLElement;
  private pane: CanvasPane;
  private readout: HTMLElement;
  private unbind: () => void;
  private phase = 0;
  private drawnPhase = -1;
  private layer: HTMLCanvasElement | null = null;
  private layerKey = '';
  private lastText = 0;
  private lastShape = { depth: NaN, offset: NaN };

  constructor(private readonly o: WaveOptions) {
    this.root = el('div', 'wave-pane');
    this.root.dataset.testid = `wave-${o.moduleId}`;
    this.pane = new CanvasPane('wave-canvas');
    this.pane.onResize = () => { this.layer = null; this.drawnPhase = -1; };
    this.readout = el('span', 'disp-readout', '');
    this.pane.root.append(this.readout);
    this.root.append(this.pane.root);
    this.unbind = o.ticker.bind(o.moduleId, t => {
      const p = t.phase[o.moduleId];
      if (p !== undefined) this.phase = p;
      this.draw(t.time);
    });
  }

  private draw(time: number): void {
    const pane = this.pane;
    if (!pane.width) { pane.resize(); if (!pane.width) return; }
    const s = this.o.shape();
    const changed = s.depth !== this.lastShape.depth || s.offset !== this.lastShape.offset;
    if (changed) { this.lastShape = { depth: s.depth, offset: s.offset }; this.root.title = s.rateLabel; }
    const ph = this.phase % 1;
    if (!changed && this.layer && ph === this.drawnPhase) return;
    this.drawnPhase = ph;

    const w = pane.width, h = pane.height, ctx = pane.ctx, pal = pane.palette();
    const x0 = INSET, x1 = w - INSET, cy = h / 2, A = h * 0.34;
    const amp = Math.max(0.02, Math.min(1, Math.abs(s.depth)));
    const yAt = (u: number) => cy - (s.offset + amp * Math.sin(u * CYCLES * Math.PI * 2)) * A;

    const key = `${w}x${h}:${s.depth}:${s.offset}`;
    if (!this.layer || key !== this.layerKey) {
      const L = makeLayer(w, h, pane.dpr);
      L.ctx.drawImage(dotGrid(w, h, pane.dpr, pal.mute), 0, 0, w, h);
      L.ctx.beginPath();
      for (let x = x0; x <= x1; x++) {
        const y = yAt((x - x0) / (x1 - x0));
        x > x0 ? L.ctx.lineTo(x, y) : L.ctx.moveTo(x, y);
      }
      L.ctx.strokeStyle = pal.ink; L.ctx.lineWidth = 1.1; L.ctx.lineJoin = 'round';
      L.ctx.stroke();
      this.layer = L.cv; this.layerKey = key;
    }
    pane.clear();
    ctx.drawImage(this.layer, 0, 0, w, h);

    const u = ph / CYCLES;
    const px = x0 + u * (x1 - x0), py = yAt(u);
    // No phase line (owner, 3.5 review): the dot alone rides the wave on the dot grid.
    ctx.fillStyle = pal.rust;
    ctx.beginPath(); ctx.arc(px, py, 2.6, 0, Math.PI * 2); ctx.fill();

    if (time - this.lastText >= 1 / 15 || time < this.lastText) {
      this.lastText = time;
      const v = s.offset + amp * Math.sin(ph * Math.PI * 2);
      this.readout.textContent = `φ ${Math.round(ph * 360)}° ${v >= 0 ? '+' : '−'}${Math.abs(v).toFixed(2)}`;
    }
  }

  destroy(): void { this.unbind(); this.pane.destroy(); }
}
