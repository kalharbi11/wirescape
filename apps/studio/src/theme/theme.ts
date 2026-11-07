// SPDX-License-Identifier: GPL-3.0-only
export type PerformanceProfile = 'normal' | 'pi';
export type HitProfile = 'mouse' | 'touch';
export type ThemeName = 'light' | 'dark';

export interface Preferences {
  motion: boolean;
  profile: PerformanceProfile;
  hit: HitProfile;
  uiScale: number;
  theme: ThemeName;
  showPortLabels: boolean;
  /** Switch to the Pi profile by itself when frames stay slow (phase 5). */
  autoProfile: boolean;
  /** Global musical-key shortcuts; explicit screen keys and MIDI are independent. */
  computerKeyboardPlays: boolean;
}

const KEY = 'wirescape.prefs.v1';

export const DEFAULT_PREFS: Preferences = {
  motion: true,
  profile: 'normal',
  hit: 'mouse',
  uiScale: 1,
  theme: 'light',
  showPortLabels: true,
  autoProfile: true,
  computerKeyboardPlays: true,
};

export function loadPrefs(): Preferences {
  const defaults = { ...DEFAULT_PREFS, motion: !matchMedia('(prefers-reduced-motion: reduce)').matches };
  try {
    const raw = localStorage.getItem(KEY);
    if (raw) return { ...defaults, ...JSON.parse(raw) };
  } catch { /* storage unavailable — fall through to defaults */ }
  return defaults;
}

export function savePrefs(p: Preferences): void {
  try { localStorage.setItem(KEY, JSON.stringify(p)); } catch { /* ignore */ }
}

import { setDensity } from '../panels/layouts';

export function applyPrefs(p: Preferences): void {
  setDensity(p.hit);
  const r = document.documentElement;
  r.dataset.profile = p.profile;
  r.dataset.hit = p.hit;
  r.dataset.theme = p.theme;
  r.style.setProperty('--ui-scale', String(p.uiScale));
  document.body.classList.toggle('motion-off', !p.motion);
  document.body.classList.toggle('hide-port-labels', !p.showPortLabels);
}

/**
 * Rough device-class sniff used to auto-select the Pi profile. Deliberately
 * conservative: a wrong guess only costs shadows.
 */
export function detectSlowDevice(): boolean {
  const cores = navigator.hardwareConcurrency ?? 8;
  const mem = (navigator as Navigator & { deviceMemory?: number }).deviceMemory ?? 8;
  const arm = /aarch64|arm/i.test(navigator.userAgent);
  return cores <= 4 || mem <= 4 || arm;
}

