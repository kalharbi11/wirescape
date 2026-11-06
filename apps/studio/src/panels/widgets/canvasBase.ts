// SPDX-License-Identifier: GPL-3.0-only
import { el } from '../../app/dom';

/** Design tokens the displays read. Resolved once per pane, never per frame. */
export interface Palette {
  ink: string; mute: string; hair: string; hair2: string;
  rust: string; rust2: string; card: string; graph: string; mono: string;
}

const FALLBACK: Palette = {
  ink: '#161616', mute: '#8a8a86', hair: '#e4e4e0', hair2: '#efefec',
  rust: '#c2501f', rust2: 'rgba(194,80,31,.14)', card: '#ffffff', graph: '#6b6b68',
  mono: '"IBM Plex Mono", ui-monospace, monospace',
};

/**
 * Live displays draw on a small canvas, never hundreds of SVG nodes.
 *
 * Cards sit in a CSS-zoomed world, so sizes come from layout (`offsetWidth`),
 * which ignores transforms. They are read on construction and on
 * ResizeObserver callbacks only.
 */
export class CanvasPane {
  readonly root: HTMLElement;
  readonly canvas: HTMLCanvasElement;
  readonly ctx: CanvasRenderingContext2D;
  /** Called after the backing store was resized; owners invalidate caches here. */
  onResize: (() => void) | null = null;
  private w = 0;
  private h = 0;
  private _dpr = 1;
  private ro: ResizeObserver;
  private static panes = new Set<CanvasPane>();
  private static zoom = 1;
  private static zoomTimer: number | undefined;

  /** One debounced resize pass for a CSS-transformed patch world. */
  static setZoom(zoom: number, force = false): void {
    if (!force && zoom === this.zoom) return;
    this.zoom = zoom;
    clearTimeout(this.zoomTimer);
    this.zoomTimer = window.setTimeout(() => {
      for (const pane of this.panes) pane.resize();
    }, 90);
  }
  private pal: Palette | null = null;

  constructor(cls: string, private readonly maxDpr = 4) {
    this.root = el('div', `cv-pane ${cls}`);
    this.canvas = el('canvas') as HTMLCanvasElement;
    this.root.append(this.canvas);
    this.ctx = this.canvas.getContext('2d', { alpha: true })!;
    this.ro = new ResizeObserver(() => this.resize());
    this.ro.observe(this.root);
    CanvasPane.panes.add(this);
  }

  get width(): number { return this.w }
  get height(): number { return this.h }
  get dpr(): number { return this._dpr }

  /** Match the scaled screen pixels, with a 2x Pi budget cap. */
  resize(): void {
    const w = this.root.offsetWidth, h = this.root.offsetHeight;
    if (!w || !h) return;
    const cap = document.documentElement.dataset.profile === 'pi' ? 2 : this.maxDpr;
    const dpr = Math.min(cap, (window.devicePixelRatio || 1) * CanvasPane.zoom);
    if (w === this.w && h === this.h && dpr === this._dpr) return;
    this.w = w; this.h = h; this._dpr = dpr;
    this.canvas.width = Math.round(w * dpr);
    this.canvas.height = Math.round(h * dpr);
    this.canvas.style.width = `${w}px`;
    this.canvas.style.height = `${h}px`;
    this.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    this.onResize?.();
  }

  clear(): void { this.ctx.clearRect(0, 0, this.w, this.h); }
  destroy(): void { this.ro.disconnect(); CanvasPane.panes.delete(this); }

  /** Token colours, read from CSS once the pane is attached, then cached. */
  palette(): Palette {
    if (this.pal) return this.pal;
    if (!this.root.isConnected) return FALLBACK;
    const cs = getComputedStyle(this.root);
    const g = (n: keyof Palette, v: string) => cs.getPropertyValue(`--${n}`).trim() || v;
    this.pal = {
      ink: g('ink', FALLBACK.ink), mute: g('mute', FALLBACK.mute), hair: g('hair', FALLBACK.hair),
      hair2: g('hair2', FALLBACK.hair2), rust: g('rust', FALLBACK.rust), rust2: g('rust2', FALLBACK.rust2),
      card: g('card', FALLBACK.card), graph: g('graph', FALLBACK.graph), mono: g('mono', FALLBACK.mono),
    };
    return this.pal;
  }
}

/** An offscreen layer at the pane's pixel density; draw in CSS px. */
export function makeLayer(w: number, h: number, dpr: number): { cv: HTMLCanvasElement; ctx: CanvasRenderingContext2D } {
  const cv = document.createElement('canvas');
  cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr);
  const ctx = cv.getContext('2d')!;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  return { cv, ctx };
}

/** The kit's fading dot grid: 1 px dots every 8 px, alpha falling to 0 at the edges. */
export function dotGrid(w: number, h: number, dpr: number, color: string): HTMLCanvasElement {
  const { cv, ctx } = makeLayer(w, h, dpr);
  ctx.fillStyle = color;
  for (let x = 4; x < w; x += 8) {
    for (let y = 3; y < h; y += 8) {
      const d = Math.hypot((x - w / 2) / (w / 2), (y - h / 2) / (h / 2));
      const a = Math.max(0, 1 - d) * 0.55;
      if (a < 0.03) continue;
      ctx.globalAlpha = a;
      ctx.fillRect(x, y, 1, 1);
    }
  }
  return cv;
}
