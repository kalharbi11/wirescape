// SPDX-License-Identifier: GPL-3.0-only
import type { Connection } from '../patch/types';
import { connKey } from '../patch/types';
import { svgEl } from '../app/dom';

export interface Point { x: number; y: number }

export interface CableStyle {
  /** The user-picked colour (DESIGN §9: no meaning in colour). */
  color: string;
  kind: Connection['kind'];
  feedback: boolean;
  /** Set when this cable is one half of a stereo pair. */
  stereo: boolean;
}

export interface Resolved { from: Point; to: Point; style: CableStyle }

export type Resolver = (c: Connection) => Resolved | null;

interface Entry {
  group: SVGGElement; path: SVGPathElement; hit: SVGPathElement; marker?: SVGPathElement;
  a: Point; b: Point;
  /** Control point of the sagging curve and its velocity (sag spring). */
  cp: Point; v: Point; moving: boolean;
  active: boolean;
}

/** Sag spring, DESIGN §8: k = 90, damping 0.02^dt. */
const K = 90;
const DAMP = 0.02;

/**
 * The cable layer lives *inside* the world transform, so panning and
 * zooming never recompute a path. Paths are recomputed only when something
 * moves; while a module or a cable end is dragged, the sag lags and settles
 * with a damped spring, stepped from the app's one animation loop.
 */
export class CableLayer {
  readonly root: SVGSVGElement;
  private entries = new Map<string, Entry>();
  private ghost: SVGPathElement;
  private ghostState: { a: Point; b: Point; cp: Point; v: Point } | null = null;
  private selected: string | null = null;
  /** True while the user drags something cables hang from: reroutes spring instead of snapping. */
  springy = false;
  private last = 0;
  /** Current SVG bounds in world units ("x y w h"), so bounds are only rewritten on change. */
  private bounds = '';

  constructor(
    private resolve: Resolver,
    private readonly onSelect: (c: Connection | null) => void,
    private readonly onGrabEnd: (c: Connection, e: PointerEvent) => void,
    private readonly describe: (c: Connection) => string,
  ) {
    this.root = svgEl('svg', { class: 'cable-layer', 'aria-label': 'Connections' });
    this.ghost = svgEl('path', { class: 'cable-ghost' });
    this.root.append(this.ghost);
  }

  setResolver(r: Resolver): void { this.resolve = r; }

  /** Add/remove paths so the DOM matches the connection list, then route. */
  sync(connections: Connection[], feedback: Set<string>): void {
    const live = new Set<string>();
    for (const c of connections) {
      const k = connKey(c);
      live.add(k);
      if (!this.entries.has(k)) this.create(k, c);
    }
    for (const [k, e] of this.entries) {
      if (!live.has(k)) { e.group.remove(); this.entries.delete(k); }
    }
    this.reroute(connections, feedback);
  }

  /** Recompute geometry. Call after a module moves or a cable changes. */
  reroute(connections: Connection[], feedback: Set<string>): void {
    for (const c of connections) {
      const k = connKey(c);
      const e = this.entries.get(k);
      if (!e) continue;
      const r = this.resolve(c);
      if (!r) { e.group.style.display = 'none'; continue; }
      e.group.style.display = '';
      const moved = e.a.x !== r.from.x || e.a.y !== r.from.y || e.b.x !== r.to.x || e.b.y !== r.to.y;
      e.a = r.from; e.b = r.to;
      if (!this.springy || e.cp.x === 0 && e.cp.y === 0) {
        e.cp = sagPoint(e.a, e.b); e.v = { x: 0, y: 0 }; e.moving = false;
        this.draw(e);
      } else if (moved) {
        e.moving = true;
        this.draw(e);
      }
      e.path.setAttribute('stroke', r.style.color);
      e.path.setAttribute('class', cableClass(r.style, feedback.has(k), this.selected === k, e.active));
      e.group.dataset.kind = r.style.kind;
      if (feedback.has(k)) {
        if (!e.marker) {
          e.marker = svgEl('path', { class: 'cable-feedback' });
          e.group.append(e.marker);
        }
        e.marker.setAttribute('d', feedbackMark(midQ(e.a, e.cp, e.b)));
        e.marker.setAttribute('stroke', r.style.color);
      } else if (e.marker) { e.marker.remove(); delete e.marker; }
      e.hit.setAttribute('aria-label', this.describe(c));
      const title = e.group.querySelector('title');
      if (title) title.textContent = this.describe(c);
    }
    this.fitBounds();
  }

  /** Mark a cable as carrying signal (dotted, DESIGN §9). Class change only on a flip. */
  setActive(key: string, on: boolean): void {
    const e = this.entries.get(key);
    if (!e || e.active === on) return;
    e.active = on;
    e.path.classList.toggle('active', on);
  }

  /** Step the sag springs. Called from the ticker; does nothing once everything has settled. */
  step(now: number): void {
    const dt = Math.min(0.05, this.last ? (now - this.last) / 1000 : 0.016);
    this.last = now;
    const damp = Math.pow(DAMP, dt);
    let drew = false;
    for (const e of this.entries.values()) {
      if (!e.moving) continue;
      if (springStep(e, sagPoint(e.a, e.b), dt, damp)) { this.draw(e); drew = true; }
      else { e.moving = false; }
    }
    const g = this.ghostState;
    if (g && springStep(g, sagPoint(g.a, g.b), dt, damp)) {
      this.ghost.setAttribute('d', quad(g.a, g.cp, g.b));
      drew = true;
    }
    if (drew) this.fitBounds();
  }

  /**
   * Size the SVG to cover every cable. A 1x1 `overflow: visible` SVG gives Chromium's GPU
   * rasteriser a wrong paint rectangle: cables were only repainted near cards and ended
   * mid-canvas in the desktop app (owner report, Basics 2 at 100 %, 2026-11-12). The viewBox
   * keeps paths in world units. Snapped to 64 px so springs and drags rarely rewrite it.
   */
  private fitBounds(): void {
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    const add = (p: Point) => { x0 = Math.min(x0, p.x); y0 = Math.min(y0, p.y); x1 = Math.max(x1, p.x); y1 = Math.max(y1, p.y); };
    const curve = (a: Point, cp: Point, b: Point) => { const [c1, c2] = ctrl(a, cp, b); add(a); add(b); add(c1); add(c2); };
    for (const e of this.entries.values()) if (e.group.style.display !== 'none') curve(e.a, e.cp, e.b);
    if (this.ghostState) curve(this.ghostState.a, this.ghostState.cp, this.ghostState.b);
    if (x0 === Infinity) { x0 = 0; y0 = 0; x1 = 1; y1 = 1; }
    const pad = 64, snap = (n: number) => Math.floor(n / pad) * pad;
    const x = snap(x0 - pad), y = snap(y0 - pad);
    const w = snap(x1 + 2 * pad) - x, h = snap(y1 + 2 * pad) - y;
    const next = `${x} ${y} ${w} ${h}`;
    if (next === this.bounds) return;
    this.bounds = next;
    this.root.setAttribute('viewBox', next);
    Object.assign(this.root.style, { left: `${x}px`, top: `${y}px`, width: `${w}px`, height: `${h}px` });
  }

  get settling(): boolean {
    if (this.ghostState) return true;
    for (const e of this.entries.values()) if (e.moving) return true;
    return false;
  }

  /** Midpoint of a cable, in world units (for the selected-cable tools). */
  midpoint(c: Connection): Point | null {
    const e = this.entries.get(connKey(c));
    return e ? midQ(e.a, e.cp, e.b) : null;
  }

  private draw(e: Entry): void {
    const d = quad(e.a, e.cp, e.b);
    e.path.setAttribute('d', d);
    e.hit.setAttribute('d', d);
  }

  private create(k: string, c: Connection): void {
    const group = svgEl('g', { class: 'cable-group' });
    group.dataset.key = k;
    group.dataset.testid = `cable-${k}`;
    const path = svgEl('path', { class: 'cable' });
    const hit = svgEl('path', { class: 'cable-hit' });
    const title = svgEl('title');
    hit.append(title);
    hit.addEventListener('pointerdown', e => {
      e.stopPropagation();
      // Near the destination end, grab the cable instead of selecting it.
      const ent = this.entries.get(k);
      if (ent && c.kind !== 'modulation') {
        const p = pointerWorld(e, this.root);
        if (p && dist(p, ent.b) < 22) { this.onGrabEnd(c, e); return; }
      }
      this.onSelect(c);
    });
    group.append(path, hit);
    this.root.append(group);
    this.entries.set(k, {
      group, path, hit, a: { x: 0, y: 0 }, b: { x: 0, y: 0 }, cp: { x: 0, y: 0 }, v: { x: 0, y: 0 },
      moving: false, active: false,
    });
  }

  select(c: Connection | null, connections: Connection[], feedback: Set<string>): void {
    this.selected = c ? connKey(c) : null;
    this.reroute(connections, feedback);
  }

  setGhost(from: Point | null, to: Point | null, invalid = false): void {
    if (!from || !to) { this.ghost.setAttribute('d', ''); this.ghostState = null; return; }
    if (!this.ghostState) this.ghostState = { a: from, b: to, cp: sagPoint(from, to), v: { x: 0, y: 0 } };
    this.ghostState.a = from; this.ghostState.b = to;
    this.ghost.setAttribute('d', quad(from, this.ghostState.cp, to));
    this.ghost.classList.toggle('invalid', invalid);
    this.fitBounds();
  }

  clear(): void {
    for (const e of this.entries.values()) e.group.remove();
    this.entries.clear();
  }
}

/* ---- geometry ---------------------------------------------------------- */

/** Where the curve's control point rests: below the midpoint, deeper for longer cables (kit A4). */
function sagPoint(a: Point, b: Point): Point {
  const len = Math.hypot(b.x - a.x, b.y - a.y);
  return { x: (a.x + b.x) / 2, y: (a.y + b.y) / 2 + Math.min(140, 24 + len * 0.18) };
}

function springStep(s: { cp: Point; v: Point }, tg: Point, dt: number, damp: number): boolean {
  s.v.x = (s.v.x + (tg.x - s.cp.x) * K * dt) * damp;
  s.v.y = (s.v.y + (tg.y - s.cp.y) * K * dt) * damp;
  if (Math.abs(s.v.x) + Math.abs(s.v.y) < 0.5 && Math.abs(tg.x - s.cp.x) + Math.abs(tg.y - s.cp.y) < 0.3) {
    s.cp = tg; s.v = { x: 0, y: 0 };
    return false;
  }
  s.cp = { x: s.cp.x + s.v.x * dt, y: s.cp.y + s.v.y * dt };
  return true;
}

/**
 * One cubic per cable (DESIGN §9). It leaves an output to the right and
 * enters an input from the left, nearly level at both nodes, so the end at a
 * node stays visible when the rest routes behind a card; the sag point `cp`
 * (spring-driven) sets how far the curve droops. A single cubic has no
 * midpoint kink, so near-vertical and backward cables bend smoothly instead
 * of hooking past the node.
 */
function ctrl(a: Point, cp: Point, b: Point): [Point, Point] {
  const dx = b.x - a.x, dy = Math.abs(b.y - a.y);
  // Horizontal reach of each end: longer for backward cables, so they loop out and back.
  const L = Math.min(180, Math.max(28, Math.abs(dx) * 0.4 + dy * 0.18 + (dx < 0 ? 40 : 0)));
  const mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
  // A cubic's midpoint moves 3/4 of its control offset; the droop stays under the reach,
  // so the tangent at each node stays within ~40° of level.
  const sy = Math.max(-0.85 * L, Math.min(0.85 * L, (cp.y - my) / 0.75));
  const sx = (cp.x - mx) / 0.75;
  return [{ x: a.x + L + sx, y: a.y + sy }, { x: b.x - L + sx, y: b.y + sy }];
}

const quad = (a: Point, cp: Point, b: Point) => {
  const [c1, c2] = ctrl(a, cp, b);
  const f = (n: number) => n.toFixed(1);
  return `M${f(a.x)},${f(a.y)} C${f(c1.x)},${f(c1.y)} ${f(c2.x)},${f(c2.y)} ${f(b.x)},${f(b.y)}`;
};

/** The point halfway along the curve (selected-cable tools, feedback mark). */
const midQ = (a: Point, cp: Point, b: Point): Point => {
  const [c1, c2] = ctrl(a, cp, b);
  return { x: (a.x + 3 * c1.x + 3 * c2.x + b.x) / 8, y: (a.y + 3 * c1.y + 3 * c2.y + b.y) / 8 };
};

/** Kept for callers that need a resting path between two points. */
export function route(a: Point, b: Point): string { return quad(a, sagPoint(a, b), b); }

function cableClass(s: CableStyle, feedback: boolean, selected: boolean, active: boolean): string {
  return [
    'cable', `kind-${s.kind}`, s.stereo ? 'stereo' : '',
    feedback ? 'feedback' : '', selected ? 'selected' : '', active ? 'active' : '',
  ].filter(Boolean).join(' ');
}

/** A small chevron pointing back upstream. */
function feedbackMark(p: Point): string {
  const s = 5;
  return `M${p.x + s},${p.y - s} L${p.x - s},${p.y} L${p.x + s},${p.y + s}`;
}

const dist = (a: Point, b: Point) => Math.hypot(a.x - b.x, a.y - b.y);

function pointerWorld(e: PointerEvent, svg: SVGSVGElement): Point | null {
  const ctm = svg.getScreenCTM();
  if (!ctm) return null;
  const pt = svg.createSVGPoint();
  pt.x = e.clientX; pt.y = e.clientY;
  const w = pt.matrixTransform(ctm.inverse());
  return { x: w.x, y: w.y };
}
