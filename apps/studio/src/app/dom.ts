// SPDX-License-Identifier: GPL-3.0-only
export const NS_SVG = 'http://www.w3.org/2000/svg';

export function el<K extends keyof HTMLElementTagNameMap>(
  tag: K, cls?: string, text?: string,
): HTMLElementTagNameMap[K] {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

export function svgEl<K extends keyof SVGElementTagNameMap>(
  tag: K, attrs: Record<string, string> = {},
): SVGElementTagNameMap[K] {
  const e = document.createElementNS(NS_SVG, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  return e;
}

/** Set `data-testid`, which every interactive element must carry. */
export function testid<T extends HTMLElement | SVGElement>(e: T, id: string): T {
  e.setAttribute('data-testid', id);
  return e;
}

export const $ = <T extends Element = HTMLElement>(sel: string, root: ParentNode = document) =>
  root.querySelector<T>(sel);
export const $$ = <T extends Element = HTMLElement>(sel: string, root: ParentNode = document) =>
  [...root.querySelectorAll<T>(sel)];

/** True when one-shot motion should be skipped: the Motion preference is off, or the OS asks for reduced motion (DESIGN §8). */
export const motionOff = (): boolean =>
  document.body.classList.contains('motion-off') || matchMedia('(prefers-reduced-motion: reduce)').matches;
