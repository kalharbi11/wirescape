// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';

export interface InfoSource {
  name: string;
  color: string;
  sample(): number;
  contribution(): string;
}
export interface ParamInfoSpec { sources(): InfoSource[]; sum(): string; result(): string }
export interface ModuleInfoSpec {
  connections(): string[];
  modulated(): { param: string; sources: string; live(): string }[];
}

const NS = 'http://www.w3.org/2000/svg';
const N = 40;          // sparkline samples
const SPARK_EVERY = 3; // redraw every 3rd frame
const TEXT_MS = 66;    // live text <= 15 Hz

function el<K extends keyof HTMLElementTagNameMap>(tag: K, cls?: string, text?: string): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}
function svg<K extends keyof SVGElementTagNameMap>(tag: K, attrs: Record<string, string | number>, parent?: Element): SVGElementTagNameMap[K] {
  const e = document.createElementNS(NS, tag);
  for (const k in attrs) e.setAttribute(k, String(attrs[k]));
  parent?.appendChild(e);
  return e;
}
const clamp1 = (v: number) => (v < -1 ? -1 : v > 1 ? 1 : v);

interface Panel { box: HTMLElement; tick(frame: number, now: number): void }

/** Shared open/pin/hover behaviour. */
function wire(
  btn: HTMLElement, host: HTMLElement, ticker: Ticker,
  build: () => Panel,
): { destroy(): void } {
  let panel: Panel | null = null;
  let open = false, pinned = false, unbind: (() => void) | null = null, frame = 0;
  let fade: Animation | null = null;

  const show = () => {
    if (open) return;
    open = true;
    if (!panel) { panel = build(); host.appendChild(panel.box); }
    panel.box.classList.add('open');
    fade?.cancel();
    fade = panel.box.animate(
      [{ opacity: 0, transform: 'translateY(3px)' }, { opacity: 1, transform: 'none' }],
      { duration: 120, easing: 'ease-out' });
    btn.classList.add('on');
    frame = 0;
    const p = panel;
    unbind = ticker.bindGlobal(() => p.tick(frame++, performance.now()));
  };
  const hide = () => {
    if (!open || pinned) return;
    open = false;
    unbind?.(); unbind = null;
    fade?.cancel(); fade = null;
    panel?.box.classList.remove('open');
    btn.classList.remove('on');
  };
  const onKey = (e: KeyboardEvent) => {
    if (e.key === 'Escape' && pinned) { pinned = false; hide(); }
  };
  const stop = (e: Event) => e.stopPropagation();
  const onClick = (e: MouseEvent) => {
    e.stopPropagation();
    pinned = !pinned;
    if (pinned) { show(); document.addEventListener('keydown', onKey); }
    else { document.removeEventListener('keydown', onKey); hide(); }
  };
  btn.addEventListener('pointerenter', show);
  btn.addEventListener('pointerleave', hide);
  btn.addEventListener('pointerdown', stop);
  btn.addEventListener('click', onClick);

  return {
    destroy() {
      pinned = false; hide();
      document.removeEventListener('keydown', onKey);
      btn.removeEventListener('pointerenter', show);
      btn.removeEventListener('pointerleave', hide);
      btn.removeEventListener('pointerdown', stop);
      btn.removeEventListener('click', onClick);
      btn.remove(); panel?.box.remove(); panel = null;
    },
  };
}

function makeButton(testid: string, cls: string): HTMLButtonElement {
  const b = el('button', cls, 'i');
  b.type = 'button';
  b.dataset.testid = testid;
  b.setAttribute('aria-label', 'Info');
  return b;
}

export function attachParamInfo(host: HTMLElement, spec: ParamInfoSpec, ticker: Ticker, testid: string): { destroy(): void } {
  const btn = makeButton(testid, 'ib');
  host.appendChild(btn);

  const build = (): Panel => {
    const srcs = spec.sources();
    const n = srcs.length, RH = 18, H = Math.max(n * RH, 30), mid = H / 2;
    const box = el('div', 'ibox p');
    box.dataset.testid = testid + '-box';
    box.addEventListener('pointerdown', (e) => e.stopPropagation());
    const rows = el('div', 'ibr');
    const R = srcs.map((s) => {
      const v = el('span', 'iv');
      const sp = svg('svg', { width: 44, height: 12 });
      const pl = svg('polyline', { fill: 'none', stroke: s.color, 'stroke-width': 1 }, sp);
      const nm = el('b', undefined, s.name); nm.style.color = s.color;
      const row = el('div', 'irow'); row.append(nm, sp as unknown as Node as HTMLElement, v);
      rows.appendChild(row);
      return { s, v, pl, buf: new Float32Array(N) };
    });
    const lines = svg('svg', { width: 46, height: H });
    srcs.forEach((s, i) => {
      const y = (H - n * RH) / 2 + i * RH + RH / 2;
      svg('path', { d: `M0 ${y}C16 ${y},16 ${mid},30 ${mid}`, fill: 'none', stroke: s.color, 'stroke-width': .9 }, lines);
    });
    svg('circle', { cx: 35, cy: mid, r: 5, fill: 'var(--card)', stroke: 'var(--ink)', 'stroke-width': .8 }, lines);
    svg('path', { d: `M32.5 ${mid}H37.5M35 ${mid - 2.5}V${mid + 2.5}`, stroke: 'var(--ink)', 'stroke-width': .9, fill: 'none' }, lines);
    svg('path', { d: `M40 ${mid}H46`, stroke: 'var(--ink)', 'stroke-width': .9, fill: 'none' }, lines);
    const res = el('div', 'ires');
    const resText = el('span');
    res.append(el('small', undefined, 'SUM'), resText);
    box.append(rows, lines as unknown as HTMLElement, res);

    let i0 = 0, lastText = -1e9;
    return {
      box,
      tick(f, now) {
        for (const r of R) r.buf[i0] = clamp1(r.s.sample());
        i0 = (i0 + 1) % N;
        if (f % SPARK_EVERY === 0) {
          for (const r of R) {
            let pts = '';
            for (let i = 0; i < N; i++) pts += (i * 1.1).toFixed(1) + ',' + (6 - r.buf[(i0 + i) % N] * 5).toFixed(1) + ' ';
            r.pl.setAttribute('points', pts);
          }
        }
        if (now - lastText >= TEXT_MS) {
          lastText = now;
          for (const r of R) r.v.textContent = r.s.contribution();
          resText.textContent = spec.sum() + ' → ' + spec.result();
        }
      },
    };
  };
  return wire(btn, host, ticker, build);
}

export function attachModuleInfo(card: HTMLElement, headerSlot: HTMLElement, spec: ModuleInfoSpec, ticker: Ticker, testid: string): { destroy(): void } {
  const btn = makeButton(testid, 'ib mod');
  headerSlot.appendChild(btn);

  const build = (): Panel => {
    const box = el('div', 'ibox m');
    box.dataset.testid = testid + '-box';
    box.addEventListener('pointerdown', (e) => e.stopPropagation());
    const list = el('div', 'ibr');
    list.appendChild(el('div', 'ihd', 'CONNECTIONS'));
    for (const c of spec.connections()) {
      const m = /^(\S+)\s+([←→])\s+(.*)$/.exec(c);
      const row = el('div', 'irow');
      if (m) row.append(el('b', undefined, m[1].toUpperCase()), el('span', 'iv', m[2] + ' ' + m[3]));
      else row.append(el('span', 'iv', c));
      list.appendChild(row);
    }
    list.appendChild(el('div', 'ihd', 'MODULATED'));
    const live = spec.modulated().map((m) => {
      const v = el('span', 'iv');
      const row = el('div', 'irow');
      row.append(el('b', undefined, m.param.toUpperCase()), el('span', 'iv mute', m.sources), v);
      list.appendChild(row);
      return { m, v };
    });
    box.appendChild(list);
    let lastText = -1e9;
    return {
      box,
      tick(_f, now) {
        if (now - lastText < TEXT_MS) return;
        lastText = now;
        for (const r of live) r.v.textContent = r.m.live();
      },
    };
  };
  return wire(btn, card, ticker, build);
}
