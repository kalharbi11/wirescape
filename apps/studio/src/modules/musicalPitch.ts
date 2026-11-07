// SPDX-License-Identifier: GPL-3.0-only
/** Equal temperament, A4 = 440 Hz and MIDI note 60 = C4. Presentation only. */
const NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
export const midiToHz = (midi: number): number => 440 * 2 ** ((midi - 69) / 12);
export const hzToMidi = (hz: number): number => hz > 0 ? 69 + 12 * Math.log2(hz / 440) : NaN;
export const C0_HZ = midiToHz(12);
export const G9_HZ = midiToHz(127);

/** Keep cents signed relative to the nearest equal-tempered note. */
export function noteAndCents(midi: number): string {
  if (!Number.isFinite(midi)) return '—';
  const nearest = Math.round(midi);
  const name = NAMES[((nearest % 12) + 12) % 12];
  const octave = Math.floor(nearest / 12) - 1;
  const cents = Math.round((midi - nearest) * 100);
  return `${name}${octave} ${cents > 0 ? '+' : cents < 0 ? '−' : '+'}${Math.abs(cents)} ct`;
}

/** Enough digits to show the chosen frequency without claiming telemetry. */
export function formatHz(hz: number): string {
  if (!Number.isFinite(hz) || hz <= 0) return '—';
  const n = hz >= 1000 ? hz / 1000 : hz;
  const decimals = hz >= 1000 ? 2 : hz >= 100 ? 1 : 2;
  const text = n.toFixed(decimals).replace(/\\.?0+$/, '');
  return `${text} ${hz >= 1000 ? 'kHz' : 'Hz'}`;
}

export const formatBaseHz = (hz: number): string =>
  `${formatHz(hz)} · ${noteAndCents(hzToMidi(hz))}`;

/** The native VCO treats frequency / 220 as a pitch-CV tuning ratio. */
export function formatVcoTune(frequency: number): string {
  if (!(frequency > 0)) return '—';
  const semitones = 12 * Math.log2(frequency / 220);
  const nearest = Math.round(semitones);
  const cents = Math.round((semitones - nearest) * 100);
  const sign = (n: number) => n > 0 ? `+${n}` : n < 0 ? `−${-n}` : '0';
  return `${sign(nearest)} st ${sign(cents)} ct`;
}

/** Active Tune readout accepts semitone + cents, and bare semitones. */
export function parseVcoTune(text: string): number | null {
  const input = text.trim().replace(/[−–]/g, '-');
  const match = /^([+-]?\\d+(?:\\.\\d+)?)\\s*(?:st)?(?:\\s+([+-]?\\d+(?:\\.\\d+)?)\\s*(?:ct|cent|cents))?$/i.exec(input);
  if (!match) return null;
  const semitones = Number(match[1]) + Number(match[2] ?? 0) / 100;
  const hz = 220 * 2 ** (semitones / 12);
  return Number.isFinite(hz) ? hz : null;
}
