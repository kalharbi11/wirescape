import type { Ticker } from '../../runtime/Ticker';
import { CanvasPane, dotGrid } from './canvasBase';
import { el } from '../../app/dom';

/** Output scope. Draws the engine's scope buffer, nothing invented here. */
export class Scope {
  readonly root: HTMLElement;
  private pane: CanvasPane;
  private unbind: () => void;
  private buf: Float32Array | null = null;
  private grid: HTMLCanvasElement | null = null;
  private drawn: Float32Array | null = null;
  private drawnSum = NaN;

  constructor(moduleId: number, ticker: Ticker) {
    this.root = el('div', 'scope-pane');
    this.root.dataset.testid = `scope-${moduleId}`;
    this.pane = new CanvasPane('scope-canvas');
    this.pane.onResize = () => { this.grid = null; this.drawn = null; };
    this.root.append(this.pane.root);
    this.unbind = ticker.bind(moduleId, t => {
      const b = t.scope[moduleId];
      if (b) this.buf = b;
      this.draw();
    });
  }

  private draw(): void {
    const pane = this.pane;
    if (!pane.width) { pane.resize(); if (!pane.width) return; }
    const b = this.buf;
    const sum = b && b.length ? b[0]! + b[b.length >> 1]! + b[b.length - 1]! : 0;
    if (this.grid && b === this.drawn && sum === this.drawnSum) return;
    this.drawn = b; this.drawnSum = sum;

    const { ctx } = pane, w = pane.width, h = pane.height, pal = pane.palette();
    this.grid ??= dotGrid(w, h, pane.dpr, pal.mute);
    pane.clear();
    ctx.drawImage(this.grid, 0, 0, w, h);
    if (!b || !b.length) return;
    const mid = h / 2;
    ctx.strokeStyle = pal.ink; ctx.lineWidth = 1; ctx.lineJoin = 'round';
    ctx.beginPath();
    for (let i = 0; i < b.length; i++) {
      const x = 3 + (i / (b.length - 1)) * (w - 6);
      const y = mid - Math.max(-1, Math.min(1, b[i]!)) * (mid - 3);
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  destroy(): void { this.unbind(); this.pane.destroy(); }
}
