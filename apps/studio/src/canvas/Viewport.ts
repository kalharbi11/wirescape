// SPDX-License-Identifier: GPL-3.0-only
export interface View { x: number; y: number; s: number }
export const ZOOM_MIN = 0.15;
export const ZOOM_MAX = 1.85;

export interface Box { x: number; y: number; w: number; h: number }

/**
 * Pan and zoom. The world layer gets one transform; cables and cards are
 * positioned in world units and never recomputed for a zoom change beyond
 * that single transform (Pi rule 3 is about cable *paths*, which only
 * change when a module moves — see CableLayer).
 */
export class Viewport {
  view: View = { x: 0, y: 0, s: 1 };
  private listeners = new Set<(v: View) => void>();

  constructor(
    private readonly viewportEl: HTMLElement,
    private readonly worldEl: HTMLElement,
  ) {}

  on(cb: (v: View) => void): () => void { this.listeners.add(cb); return () => this.listeners.delete(cb); }
  private emit(): void { for (const l of this.listeners) l(this.view); }

  apply(): void {
    const { x, y, s } = this.view;
    this.worldEl.style.transform = `translate(${x}px,${y}px) scale(${s})`;
    this.emit();
  }

  rect(): DOMRect { return this.viewportEl.getBoundingClientRect(); }

  /** Client coordinates -> world coordinates. */
  toWorld(clientX: number, clientY: number): { x: number; y: number } {
    const r = this.rect();
    return { x: (clientX - r.left - this.view.x) / this.view.s, y: (clientY - r.top - this.view.y) / this.view.s };
  }

  /** World coordinates -> client coordinates. */
  toClient(x: number, y: number): { x: number; y: number } {
    const r = this.rect();
    return { x: x * this.view.s + this.view.x + r.left, y: y * this.view.s + this.view.y + r.top };
  }

  setView(x: number, y: number, s: number): void {
    this.view = { x, y, s: clampZoom(s) };
    this.apply();
  }

  panBy(dx: number, dy: number): void {
    this.view.x += dx; this.view.y += dy; this.apply();
  }

  /** Zoom keeping the given client point fixed. */
  zoomAt(nextScale: number, clientX: number, clientY: number): void {
    const before = this.toWorld(clientX, clientY);
    const r = this.rect();
    this.view.s = clampZoom(nextScale);
    this.view.x = clientX - r.left - before.x * this.view.s;
    this.view.y = clientY - r.top - before.y * this.view.s;
    this.apply();
  }

  zoomBy(factor: number): void {
    const r = this.rect();
    this.zoomAt(this.view.s * factor, r.left + r.width / 2, r.top + r.height / 2);
  }

  /** Zoom to an exact level, keeping the viewport centre fixed. */
  zoomTo(scale: number): void {
    const r = this.rect();
    this.zoomAt(scale, r.left + r.width / 2, r.top + r.height / 2);
  }

  fit(boxes: Box[], padding = { x: 140, y: 190 }): void {
    if (!boxes.length) { this.setView(0, 0, 1); return; }
    const r = this.rect();
    const left = Math.min(...boxes.map(b => b.x));
    const top = Math.min(...boxes.map(b => b.y));
    const right = Math.max(...boxes.map(b => b.x + b.w));
    const bottom = Math.max(...boxes.map(b => b.y + b.h));
    const w = Math.max(1, right - left), h = Math.max(1, bottom - top);
    const padX = Math.min(padding.x, r.width * 0.1);
    const padY = Math.min(padding.y, r.height * 0.26);
    const s = clampZoom(Math.min(1.04, (r.width - padX) / w, (r.height - padY) / h));
    this.setView((r.width - w * s) / 2 - left * s, (r.height - h * s) / 2 - top * s, s);
  }

  /** The world rectangle currently on screen, for visibility culling. */
  visibleWorldBox(margin = 120): Box {
    const r = this.rect();
    const a = this.toWorld(r.left - margin, r.top - margin);
    const b = this.toWorld(r.right + margin, r.bottom + margin);
    return { x: a.x, y: a.y, w: b.x - a.x, h: b.y - a.y };
  }
}

export const clampZoom = (s: number) => Math.max(ZOOM_MIN, Math.min(ZOOM_MAX, s));
export const boxesOverlap = (a: Box, b: Box) =>
  a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
