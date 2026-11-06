// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';
import { el } from '../../app/dom';

const FALL = 0.9;      // fraction of the range per second
const HOLD = 1;        // seconds the peak line holds
const CLIP = 0.97;
const CLIP_HOLD = 1.2; // seconds the clip bar stays lit

interface Chan {
  col: HTMLElement; bar: HTMLElement; peak: HTMLElement; clip: HTMLElement;
  level: number; pk: number; pkAt: number; clipUntil: number;
  sBar: number; sPeak: number; sClip: boolean;
}

/**
 * Stereo output meter, horizontal (owner, 3.5 review): L and R bars run the
 * width of the card over a dB scale (levels are linear amplitude, so the
 * ticks sit at 10^(dB/20)). Bars rise instantly and fall at ~0.9 of the range
 * per second; a peak line holds for 1 s; a rust clip mark lights at the end.
 * Motion is transform-only (scaleX / translateX); no layout properties.
 */
export class Meter {
  readonly root: HTMLElement;
  private chans: Chan[] = [];
  private unbind: () => void;
  private lastTime = -1;

  constructor(moduleId: number, ticker: Ticker, labels: [string, string] = ['L', 'R']) {
    this.root = el('div', 'meter-pane');
    this.root.dataset.testid = `meter-${moduleId}`;
    for (let i = 0; i < 2; i++) {
      const col = el('div', 'meter-row');
      const clip = el('i', 'meter-clip');
      const track = el('div', 'meter-track');
      const bar = el('i', 'meter-bar');
      const peak = el('i', 'meter-peak');
      track.append(bar, peak);
      col.append(el('span', 'meter-lab', labels[i]), track, clip);
      this.root.append(col);
      this.chans.push({ col, bar, peak, clip, level: 0, pk: 0, pkAt: 0, clipUntil: 0, sBar: -1, sPeak: -1, sClip: false });
    }
    const scale = el('div', 'meter-scale');
    for (const db of [-24, -12, -6, -3, 0]) {
      const t = el('span', 'meter-tick', db === 0 ? '0 dB' : String(db));
      t.style.left = `${(Math.pow(10, db / 20) * 100).toFixed(1)}%`;
      scale.append(t);
    }
    this.root.append(scale);
    this.unbind = ticker.bind(moduleId, t => {
      const m = t.meters[moduleId];
      const dt = this.lastTime < 0 || t.time < this.lastTime ? 0 : t.time - this.lastTime;
      this.lastTime = t.time;
      this.chans.forEach((c, i) => this.step(c, m ? m[i] ?? 0 : 0, t.time, dt));
    });
  }

  private step(c: Chan, v: number, time: number, dt: number): void {
    c.level = v >= c.level ? v : Math.max(v, c.level - FALL * dt);
    if (v >= c.pk) { c.pk = v; c.pkAt = time; }
    else if (time - c.pkAt > HOLD) c.pk = Math.max(c.level, c.pk - FALL * dt);
    if (v > CLIP) c.clipUntil = time + CLIP_HOLD;
    const bar = Math.round(Math.min(1, c.level) * 1000) / 1000;
    const pk = Math.round(Math.min(1, c.pk) * 1000) / 1000;
    const clip = time < c.clipUntil;
    if (bar !== c.sBar) { c.sBar = bar; c.bar.style.transform = `scaleX(${bar})`; }
    if (pk !== c.sPeak) {
      c.sPeak = pk;
      c.peak.style.transform = `translateX(${pk * 100}%)`;
      c.peak.style.opacity = pk > 0.01 ? '1' : '0';
    }
    if (clip !== c.sClip) { c.sClip = clip; c.clip.classList.toggle('on', clip); }
  }

  destroy(): void { this.unbind(); }
}

