// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';
import type { Telemetry } from '../../engine/EngineClient';
import { el } from '../../app/dom';
import {
  CONDS, DIVS, GATES, GATE_LABEL, LENGTHS, PEN, SPEED_LABEL, condLabel, masterCycle, noteName, parseNote, readConfig, sameCond,
  setKind, setLength, sourceOf, stepAt, stepFor, trackCycle, trackDiv, trackLength, upgrade,
  type GrooveConfig, type Kind, type Step,
} from '../../groove/pattern';
import { ROOTS, SCALES, type Scale, clearBar, copyBar, inScale, movePitch, pasteBar, rollNotes } from '../../groove/editorHelpers';
import {
  MAX_KEYS, PICKER_GROUPS, chordName, inScale as chordInScale, invert, moveNotes, notesOf,
  qualityKeys, scaleStack, setNotes, stepLabel, type ScaleCtx,
} from '../../groove/chords';
import '../../styles/groove.css';

/** What the Groove editor may ask of the app. Nothing here starts audio on its own. */
export interface GrooveHost {
  config(id: number): unknown;
  /** Whether the Groove's clock input has an authored signal cable. */
  clockPatched(id: number): boolean;
  setConfig(id: number, config: GrooveConfig): void;
  /** Current-generation measured stop (G3 `canApplyGroove`); false when the engine cannot say. */
  canApply(id: number): boolean;
  isDraft(id: number): boolean;
  appliedRaw(id: number): unknown;
  /** Stopped Apply through the whole-graph load; resolves false on refusal (draft kept). */
  apply(id: number): Promise<boolean>;
  revert(id: number): void;
  /** Rising-edge transport request (`run`, `stop`, `reset`, `t1_run`, …); never touches the document. */
  command(id: number, param: string): void;
  undo(): void; redo(): void; canUndo(): boolean; canRedo(): boolean;
  onChange(cb: () => void): () => void;
  /** Scale helper saved in the patch `ui.groove` (UI-only; one undo step per change). */
  scale(id: number): { root: number; scale: string } | undefined;
  setScale(id: number, root: number, scale: string): void;
}

interface View { t: number; sel: [number, number]; selected: [boolean, boolean]; lane: 'trig' | 'note'; pen: [number, number];
  mode: 'pads' | 'roll'; scale: Scale; root: number; look: 'graphite' | 'oxblood' | 'led'; octave: number }
/** Per-module view state survives card rebuilds (undo, load). */
const views = new Map<number, View>();
const ICON_PLAY = '<svg width="12" height="12" viewBox="0 0 12 12"><path d="M3 1.5v9l7.5-4.5z"/></svg>';
const ICON_STOP = '<svg width="11" height="11" viewBox="0 0 11 11"><rect x="1.5" y="1.5" width="8" height="8" rx="1"/></svg>';
const clamp = (v: number, a: number, b: number) => Math.max(a, Math.min(b, v));

/**
 * Groove editor, Pads view (G2a, #498). Edits are an authored draft in the
 * document; only Apply sends them, and only on a measured stop. The playhead
 * and the "skipped this pass" dim come from telemetry `playhead` / `groove`
 * maps alone, and are hidden when the engine does not send them.
 * Seams for G2b: `lane` (value/condition lanes), `stepPitch` (scale helper), the view switch (Roll).
 */
export class GroovePane {
  readonly root: HTMLElement;
  private readonly grid = el('div', 'g-grid');
  private readonly insp = el('div', 'g-insp');
  private readonly status = el('span', 'g-status');
  private readonly tracksRow = el('div', 'g-row g-tracks');
  private readonly lanesRow = el('div', 'g-row g-lanes');
  private readonly draftEl = el('span', 'g-draft');
  private readonly play: HTMLButtonElement;
  private readonly apply: HTMLButtonElement;
  private readonly stopApply: HTMLButtonElement;
  private readonly revert: HTMLButtonElement;
  private readonly undoBtn: HTMLButtonElement;
  private readonly redoBtn: HTMLButtonElement;
  private readonly bpm = el('input', 'g-bpm') as HTMLInputElement;
  private readonly extBadge = el('span', 'g-draft', 'EXT');
  private readonly view: View;
  private cfg: GrooveConfig | null = null;
  private applied: GrooveConfig | null = null;
  private last: Telemetry | null = null;
  private stopRequested = false;
  private pendingApply = false;
  private startedBlind = false;
  private applying = false;
  private pop: HTMLElement | null = null;
  private wheelAcc = 0;
  private clipboard: Step[] | null = null;
  private readonly toastEl = el('span', 'g-toast');
  private toastTimer: number | undefined;
  private readonly held = new Map<string, number>();
  private heldNotes: number[] = [];
  private lastRollPress: { i: number; key: number; at: number } | null = null;
  private entry: { track: number; step: number } | null = null;
  private unbind: (() => void)[] = [];

  constructor(private readonly id: number, ticker: Ticker, private readonly host: GrooveHost,
    private readonly layoutChanged: () => void = () => undefined) {
    this.view = views.get(id) ?? { t: 0, sel: [0, 0], selected: [false, false], lane: 'trig', pen: [PEN[0]!, PEN[1]!],
      mode: 'pads', scale: 'Chromatic', root: 0, look: 'graphite', octave: 4 };
    views.set(id, this.view);
    this.play = this.button('g-pbtn', 'play', 'Play / stop at loop end');
    this.apply = this.button('soft', 'apply', '', 'Apply');
    this.stopApply = this.button('', 'stop-apply', 'Stop at the loop end, then apply', 'Stop & apply');
    this.revert = this.button('', 'revert', 'Back to the pattern that is playing', 'Revert');
    this.undoBtn = this.button('', 'undo', 'Undo', '↶');
    this.redoBtn = this.button('', 'redo', 'Redo', '↷');
    this.root = el('div', 'groove-pane');
    this.root.tabIndex = 0;
    this.root.addEventListener('keydown', e => this.keyDown(e));
    const released = (e: KeyboardEvent) => this.keyUp(e);
    window.addEventListener('keyup', released);
    this.unbind.push(() => window.removeEventListener('keyup', released));
    // A key released while the window or pane lacks focus never reaches keyUp:
    // drop the partial chord instead of letting it join the next key press.
    const drop = () => this.dropHeld();
    const hidden = () => { if (document.hidden) this.dropHeld(); };
    window.addEventListener('blur', drop);
    document.addEventListener('visibilitychange', hidden);
    this.unbind.push(() => { window.removeEventListener('blur', drop); document.removeEventListener('visibilitychange', hidden); });
    this.root.addEventListener('focusout', e => {
      const to = e.relatedTarget as Node | null;
      if (to && !this.root.contains(to)) this.dropHeld();
    });
    this.root.dataset.look = this.view.look;
    this.root.dataset.testid = `groove-${id}`;
    this.status.dataset.testid = `groove-status-${id}`;
    this.draftEl.dataset.testid = `groove-draft-${id}`;
    this.bpm.type = 'number'; this.bpm.min = '20'; this.bpm.max = '300'; this.bpm.step = '0.1';
    this.bpm.dataset.testid = `groove-bpm-${id}`;
    this.extBadge.dataset.testid = `groove-ext-${id}`;
    this.extBadge.title = 'Following the external clock input';
    this.extBadge.hidden = true;
    this.bpm.setAttribute('aria-label', 'Tempo (BPM)');
    this.bpm.addEventListener('change', () => {
      const v = Number(this.bpm.value);
      if (Number.isFinite(v)) this.edit(c => { c.bpm = Math.round(clamp(v, 20, 300) * 10) / 10; });
    });
    const reset = this.button('', 'reset', 'Reset to step 1', 'Reset');
    reset.addEventListener('click', () => host.command(id, 'reset'));
    this.play.addEventListener('click', () => void this.togglePlay());
    this.apply.addEventListener('click', () => void this.doApply());
    this.stopApply.addEventListener('click', () => { this.pendingApply = true; this.requestStop(); });
    this.revert.addEventListener('click', () => { this.pendingApply = false; host.revert(id); });
    this.undoBtn.addEventListener('click', () => host.undo());
    this.redoBtn.addEventListener('click', () => host.redo());
    const transport = el('div', 'g-row g-transport');
    transport.append(this.play, reset, this.status, el('span', 'g-sp'), this.draftEl, this.apply, this.stopApply,
      this.revert, el('span', 'g-lbl', 'BPM'), this.bpm, this.undoBtn, this.redoBtn);
    const head = el('div', 'g-row g-head');
    head.append(el('span', 'g-identity', 'GRV  Groove'), this.extBadge, el('span', 'g-sp'));
    const viewSwitch = el('span', 'g-seg underline');
    for (const mode of ['pads', 'roll'] as const) {
      const b = this.button(this.view.mode === mode ? 'on' : '', 'view-' + mode, '', mode);
      b.addEventListener('click', () => {
        this.view.mode = mode;
        this.closePop(); this.refresh(); this.layoutChanged();
      });
      viewSwitch.append(b);
    }
    head.append(viewSwitch);
    this.root.append(head, transport, this.tracksRow, this.lanesRow, this.grid, this.insp, this.toastEl);
    this.grid.addEventListener('pointerdown', e => {
      if ((e.target as HTMLElement).closest('.g-roll-cell')) this.rollDown(e);
      else this.padDown(e);
    });
    this.grid.addEventListener('dblclick', e => this.rollDelete(e));
    this.grid.addEventListener('contextmenu', e => {
      if ((e.target as HTMLElement).closest('.g-roll-cell')) {
        e.preventDefault(); this.rollDelete(e);
      }
    });
    this.grid.addEventListener('wheel', e => this.wheel(e), { passive: false });
    this.unbind.push(host.onChange(() => this.refresh()));
    this.unbind.push(ticker.bind(id, t => { this.last = t; this.paintLive(); }));
    this.refresh();
  }

  // ---- data ---------------------------------------------------------------

  private get t(): number { return this.view.t; }
  private len(t = this.t): number { return this.cfg ? trackLength(this.cfg, t) : 16; }
  private step(i: number, t = this.t): Step { return this.cfg ? stepAt(this.cfg, t, i) : { kind: 'rest' }; }
  private scaleCtx(): ScaleCtx { return { root: this.view.root, intervals: SCALES[this.view.scale] }; }
  private moveStep(c: GrooveConfig, i: number, n: number, octave: boolean): void {
    const src = sourceOf(c, this.t, i);
    if (src < 0) return;
    const step = stepFor(c, this.t, src), keys = notesOf(step), root = step.key ?? keys[0]!;
    if (keys.length < 2) {
      this.setPitch(c, i, this.stepPitch(root, n, octave));
    } else {
      const moved = moveNotes(keys, root, n, this.scaleCtx(), octave);
      if (moved) { setNotes(step, moved.keys, moved.root); this.view.pen[this.t] = moved.root; }
    }
  }
  /** Pitch move by `n` units: chromatic now; G2b's scale helper replaces this. */
  private stepPitch(key: number, n: number, octave: boolean): number {
    return octave ? clamp(key + n * 12, 0, 127) : movePitch(key, n, this.view.root, this.view.scale);
  }

  private edit(fn: (c: GrooveConfig) => void, base?: GrooveConfig): void {
    const c = base ? JSON.parse(JSON.stringify(base)) as GrooveConfig : readConfig(this.host.config(this.id));
    if (!c) return;
    upgrade(c);
    fn(c);
    this.host.setConfig(this.id, c);
  }
  private setPitch(c: GrooveConfig, i: number, key: number): void {
    let j = sourceOf(c, this.t, i);
    if (j < 0) { setKind(stepFor(c, this.t, i), 'note', key); j = i; }
    const s = stepFor(c, this.t, j);
    const root = s.key ?? 60, newRoot = clamp(key, 0, 127);
    const shifted = notesOf(s).map(k => k + newRoot - root);
    if (shifted.some(k => k < 0 || k > 127)) return;
    setNotes(s, shifted, newRoot);
    this.view.pen[this.t] = newRoot;
  }
  private select(i: number): void {
    this.view.sel[this.t] = clamp(i, 0, this.len() - 1);
    this.view.selected[this.t] = true;
    this.closePop();
    this.renderGrid(); this.renderInspector();
  }

  // ---- transport ----------------------------------------------------------

  private measured(): [number, number] | undefined { return this.last?.playhead?.[this.id]; }
  /**
   * Play/Stop button state only. Without telemetry it falls back to `startedBlind`;
   * the playhead and the Apply gate (`host.canApply`) never read it.
   */
  private seenScale: string | undefined;
  private saveScale(): void {
    this.host.setScale(this.id, this.view.root, this.view.scale);
    this.seenScale = JSON.stringify(this.host.scale(this.id) ?? null);
    this.renderGrid();
  }
  private running(): boolean {
    const ph = this.measured();
    return ph ? ph[0] >= 0 || ph[1] >= 0 : this.startedBlind;
  }
  private requestStop(): void {
    this.stopRequested = true;
    this.host.command(this.id, 'stop');
    this.startedBlind = false;
    this.paintLive();
  }
  private async togglePlay(): Promise<void> {
    if (this.applying) return;
    if (this.running()) { this.requestStop(); return; }
    // Play applies a dirty draft first; a refused draft is kept and never started.
    if (this.host.isDraft(this.id) && !(await this.doApply())) return;
    this.stopRequested = false;
    this.startedBlind = !this.measured();
    this.host.command(this.id, 'run');
    this.paintLive();
  }
  private async doApply(): Promise<boolean> {
    if (this.applying || !this.host.canApply(this.id)) return false;
    this.applying = true; this.pendingApply = false;
    this.paintLive();
    try { return await this.host.apply(this.id); }
    finally { this.applying = false; this.refresh(); }
  }

  // ---- rendering ----------------------------------------------------------

  refresh(): void {
    this.cfg = readConfig(this.host.config(this.id));
    // Adopt the saved scale only when the patch's copy changed (load, undo, redo);
    // the pane's own edits are written through `saveScale`.
    const saved = this.host.scale(this.id), seen = JSON.stringify(saved ?? null);
    if (seen !== this.seenScale) {
      this.seenScale = seen;
      this.view.root = saved?.root ?? 0;
      this.view.scale = saved && saved.scale in SCALES ? saved.scale as Scale : 'Chromatic';
    }
    this.applied = readConfig(this.host.appliedRaw(this.id));
    if (this.view.sel[this.t] >= this.len()) this.view.sel[this.t] = this.len() - 1;
    if (document.activeElement !== this.bpm) this.bpm.value = String(this.cfg?.bpm ?? 120);
    this.bpm.disabled = !this.cfg;
    this.root.dataset.look = this.view.look;
    const viewSwitch = this.root.querySelector('.g-head .g-seg');
    for (const mode of ['pads', 'roll']) viewSwitch?.querySelector('[data-testid="groove-view-' + mode + '-' + this.id + '"]')?.classList.toggle('on', this.view.mode === mode);
    this.renderTracks(); this.renderLanes(); this.renderGrid(); this.renderInspector();
    this.paintLive();
  }

  private button(cls: string, name: string, title: string, text = ''): HTMLButtonElement {
    const b = el('button', cls, text) as HTMLButtonElement;
    b.type = 'button'; b.dataset.testid = `groove-${name}-${this.id}`;
    if (title) b.title = title;
    return b;
  }

  private renderTracks(): void {
    this.tracksRow.replaceChildren();
    for (const t of [0, 1]) {
      const track = this.cfg?.tracks[t];
      const tab = el('div', `g-ttab${t === this.t ? ' on' : ''}`);
      tab.dataset.testid = `groove-track-${this.id}-${t + 1}`;
      const dot = this.button(`g-sd${track?.armed === false ? '' : ' armed'}`, `arm-${t + 1}`,
        'Stopped: arm or disarm the track · playing: start or stop it at its loop end');
      dot.addEventListener('click', e => {
        e.stopPropagation();
        const obs = this.last?.groove?.[this.id]?.[t];
        if (this.running()) this.host.command(this.id, `t${t + 1}_${obs?.playing ? 'stop' : 'run'}`);
        else this.edit(c => { c.tracks[t]!.armed = c.tracks[t]!.armed === false; });
      });
      const pos = el('span', 'g-pos', '');
      pos.dataset.testid = `groove-pos-${this.id}-${t + 1}`;
      const mute = this.button(`g-tg${track?.mute ? ' on' : ''}`, `mute-${t + 1}`, 'Mute (part of the pattern draft)');
      mute.innerHTML = '<span class="g-led"></span>M';
      mute.addEventListener('click', e => { e.stopPropagation(); this.edit(c => { c.tracks[t]!.mute = !c.tracks[t]!.mute; }); });
      tab.title = this.cfg ? `Cycle: ${trackCycle(this.cfg, t)} sixteenths` : '';
      const speed = el('span', 'g-speed', this.cfg && trackDiv(this.cfg, t) !== 1 ? SPEED_LABEL[trackDiv(this.cfg, t)] + '×' : '');
      speed.dataset.testid = `groove-speed-badge-${this.id}-${t + 1}`;
      tab.append(dot, el('span', 'g-tn', `T${t + 1}`), el('span', 'g-nm', t ? 'Bass' : 'Lead'), speed, pos, mute);
      tab.addEventListener('click', () => { this.view.t = t; this.closePop(); this.refresh(); });
      this.tracksRow.append(tab);
    }
    this.tracksRow.append(el('span', 'g-sp'), el('span', 'g-lbl', 'Steps'));
    const seg = el('span', 'g-seg');
    for (const n of LENGTHS) {
      const b = this.button(n === this.len() ? 'on' : '', `len-${n}`, `${n} steps`, String(n));
      b.disabled = !this.cfg;
      b.addEventListener('click', () => { this.edit(c => setLength(c, this.t, n)); this.layoutChanged(); });
      seg.append(b);
    }
    this.tracksRow.append(seg, el('span', 'g-lbl', 'Speed'));
    const speeds = el('span', 'g-seg');
    const current = this.cfg ? trackDiv(this.cfg, this.t) : 1;
    for (const div of DIVS) {
      const b = this.button(div === current ? 'on' : '', `div-${div}`,
        `Track speed ${SPEED_LABEL[div]}${div === 1 ? '' : '×'}`, SPEED_LABEL[div] + (div === 1 ? '' : '×'));
      b.disabled = !this.cfg;
      b.addEventListener('click', () => this.edit(c => {
        upgrade(c);
        if (div === 1) delete c.tracks[this.t]!.div;
        else c.tracks[this.t]!.div = div;
      }));
      speeds.append(b);
    }
    const loop = el('span', 'g-loop-preview', this.cfg ? `Loop ${masterCycle(this.cfg)} sixteenths` : '');
    loop.dataset.testid = `groove-loop-${this.id}`;
    this.tracksRow.append(speeds, loop);
  }

  /**
   * The scale and look selectors are built once and kept across refreshes, so a
   * re-render while the user is choosing never drops the change.
   */
  private selectEls?: { root: HTMLSelectElement; scale: HTMLSelectElement; look: HTMLSelectElement };
  private selects(): { root: HTMLSelectElement; scale: HTMLSelectElement; look: HTMLSelectElement } {
    if (this.selectEls) return this.selectEls;
    const root = el('select', 'g-select') as HTMLSelectElement;
    root.dataset.testid = 'groove-scale-root-' + this.id;
    root.setAttribute('aria-label', 'Scale root');
    ROOTS.forEach((name, i) => {
      const opt = el('option', '', name) as HTMLOptionElement;
      opt.value = String(i); root.append(opt);
    });
    root.addEventListener('change', () => { this.view.root = Number(root.value); this.saveScale(); });
    const scale = el('select', 'g-select') as HTMLSelectElement;
    scale.dataset.testid = 'groove-scale-' + this.id;
    scale.setAttribute('aria-label', 'Scale helper (display and pitch gestures only)');
    (Object.keys(SCALES) as Scale[]).forEach(name => {
      const opt = el('option', '', name) as HTMLOptionElement;
      opt.value = name; scale.append(opt);
    });
    scale.addEventListener('change', () => { this.view.scale = scale.value as Scale; this.saveScale(); });
    const look = el('select', 'g-select') as HTMLSelectElement;
    look.dataset.testid = 'groove-look-' + this.id;
    look.setAttribute('aria-label', 'Trig look');
    for (const name of ['graphite', 'oxblood', 'led'] as const) {
      const opt = el('option', '', name === 'led' ? 'LED' : name[0]!.toUpperCase() + name.slice(1)) as HTMLOptionElement;
      opt.value = name; look.append(opt);
    }
    look.addEventListener('change', () => { this.view.look = look.value as View['look']; this.root.dataset.look = this.view.look; });
    return (this.selectEls = { root, scale, look });
  }

  private renderLanes(): void {
    const seg = el('span', 'g-seg underline');
    for (const [lane, label] of [['trig', 'Trig'], ['note', 'Note']] as const) {
      const b = this.button(lane === this.view.lane ? 'on' : '', 'lane-' + lane, '', label);
      b.addEventListener('click', () => { this.view.lane = lane; this.refresh(); });
      seg.append(b);
    }
    const { root, scale, look } = this.selects();
    root.value = String(this.view.root);
    scale.value = this.view.scale;
    look.value = this.view.look;
    const copy = this.button('', 'copy-bar', 'Copy the selected 16-step bar', 'Copy');
    copy.disabled = !this.cfg;
    copy.addEventListener('click', () => { if (this.cfg) { this.clipboard = copyBar(this.cfg, this.t, this.view.sel[this.t]); this.renderLanes(); } });
    const paste = this.button('', 'paste-bar', 'Paste into selected bar as one undo step', 'Paste');
    paste.disabled = !this.cfg || !this.clipboard;
    paste.addEventListener('click', () => {
      if (this.clipboard) this.edit(c => pasteBar(c, this.t, this.view.sel[this.t], this.clipboard!));
    });
    const clear = this.button('', 'clear-bar', 'Clear the selected bar as one undo step', 'Clear');
    clear.disabled = !this.cfg;
    clear.addEventListener('click', () => this.edit(c => clearBar(c, this.t, this.view.sel[this.t])));
    this.lanesRow.replaceChildren(seg, el('span', 'g-lbl', 'Scale'), root, scale, el('span', 'g-sp'),
      copy, paste, clear, look);
    if (!this.cfg) this.lanesRow.append(el('span', 'g-note',
      'This Groove pattern is newer or malformed; it is kept as saved and is read-only here.'));
  }

  private renderGrid(): void {
    this.grid.replaceChildren();
    if (!this.cfg) return;
    if (this.view.mode === 'roll') { this.renderRoll(); return; }
    const c = this.cfg, t = this.t, L = this.len(), sel = this.view.sel[t];
    for (let b = 0; b < Math.ceil(L / 16); b++) {
      const bar = el('div', 'g-bar');
      bar.dataset.bar = String(b);
      const pads = el('div', 'g-pads');
      for (let g = 0; g < 4; g++) {
        const grp = el('div', 'g-pgrp');
        for (let k = 0; k < 4; k++) {
          const i = b * 16 + g * 4 + k;
          if (i >= L) break;
          grp.append(this.pad(c, i, k === 3, sel));
        }
        if (grp.childElementCount) pads.append(grp);
      }
      const div = trackDiv(c, t);
      const barFrom = b * div + 1, barTo = Math.min(b * div + div, L * div / 16);
      bar.append(el('span', 'g-barn', div === 1 || barFrom === barTo ? String(barFrom) : `${barFrom}–${barTo}`), pads);
      this.grid.append(bar);
    }
  }

  private pad(c: GrooveConfig, i: number, groupEnd: boolean, sel: number): HTMLElement {
    const t = this.t, L = this.len(), s = stepAt(c, t, i), src = sourceOf(c, t, i);
    const real = s.kind === 'note' || (s.kind === 'tie' && src >= 0);
    const nextTie = i + 1 < L && stepAt(c, t, i + 1).kind === 'tie' && real;
    const cell = el('div', 'g-pcell');
    const pad = el('div', `g-pad ${real ? s.kind : 'rest'}`);
    pad.dataset.i = String(i);
    pad.dataset.testid = `groove-pad-${this.id}-${i + 1}`;
    pad.dataset.kind = s.kind;
    pad.setAttribute('role', 'button');
    pad.setAttribute('aria-label', `Step ${i + 1}: ${s.kind === 'note' ? stepLabel(s) : s.kind}`);
    if (s.kind === 'note') {
      const keys = notesOf(s);
      const label = el('span', 'g-nn', keys.length > 1 ? stepLabel(s, true) : noteName(s.key ?? 60));
      if (keys.length > 1) {
        label.classList.add('chd');
        if (label.textContent!.length > 5) label.classList.add('long');
        const ladder = el('span', 'cue-ch');
        const lowest = Math.min(...keys);
        for (const k of keys) {
          const tick = el('i');
          tick.style.bottom = `${clamp((k - lowest) / 24, 0, 1) * 100}%`;
          ladder.append(tick);
        }
        pad.append(ladder);
      }
      pad.append(label);
      if (!inScale(s.key ?? 60, this.view.root, this.view.scale)) pad.classList.add('off-scale');
      const vel = el('span', 'cue-vel'); vel.style.width = `${Math.round((s.velocity ?? 0.8) * 100)}%`;
      pad.append(vel);
      if ((s.gate ?? 1) < 1) {
        const len = el('span', 'cue-len');
        for (const q of GATES) len.append(el('i', q <= (s.gate ?? 1) ? 'on' : ''));
        pad.append(len);
      }
      if ((s.mod1 ?? 0) > 0 || (s.mod2 ?? 0) > 0) {
        const mod = el('span', 'cue-mod');
        for (const [m, cls] of [[s.mod1 ?? 0, 'a'], [s.mod2 ?? 0, 'b']] as const)
          if (m > 0) { const bar = el('i', cls); bar.style.height = `${Math.max(12, m * 100)}%`; mod.append(bar); }
        pad.append(mod);
      }
      if (s.cond) { pad.classList.add('cond'); pad.append(el('span', 'cue-cond', condLabel(s.cond))); }
    }
    if (nextTie || (real && s.kind === 'tie')) pad.append(el('span', 'band'));
    if (nextTie) pad.classList.add('tied', groupEnd ? 'gend' : 'mid');
    if (s.kind === 'tie' && real && !nextTie) pad.classList.add('last');
    if (this.view.lane === 'note' && real) {
      const key = stepAt(c, t, src).key ?? 60;
      const dot = el('span', 'contour'); dot.style.bottom = `${clamp((key - 36) / 48, 0, 1) * 100}%`;
      pad.append(dot);
    }
    if (i === sel) pad.classList.add('sel');
    if (this.applied && JSON.stringify(stepAt(this.applied, t, i)) !== JSON.stringify(s)) pad.classList.add('chg');
    cell.append(pad, el('span', 'g-slot'));
    return cell;
  }

  private toast(message: string): void {
    this.toastEl.textContent = message;
    if (this.toastTimer !== undefined) window.clearTimeout(this.toastTimer);
    this.toastTimer = window.setTimeout(() => { this.toastEl.textContent = ''; this.toastTimer = undefined; }, 2200);
  }

  /** A note is edited on the shared step; removing its last voice makes a rest. */
  private deleteRollNote(i: number, key: number): void {
    if (!this.cfg) return;
    const t = this.t, src = sourceOf(this.cfg, t, i);
    if (src < 0 || !notesOf(stepAt(this.cfg, t, src)).includes(key)) return;
    // Deleting a sustained cell ends that tie; it does not destroy the origin chord.
    if (stepAt(this.cfg, t, i).kind === 'tie') {
      this.edit(c => setKind(stepFor(c, t, i), 'rest'));
      return;
    }
    this.edit(c => {
      const step = stepFor(c, t, src), remaining = notesOf(step).filter(k => k !== key);
      if (remaining.length) setNotes(step, remaining, step.key);
      else setKind(step, 'rest');
    });
  }

  private enterRollNote(i: number, key: number): void {
    if (!this.cfg) return;
    const t = this.t, step = stepAt(this.cfg, t, i);
    if (step.kind === 'note') {
      const keys = notesOf(step);
      if (keys.includes(key)) return;
      if (keys.length >= MAX_KEYS) { this.toast('5 notes per step'); return; }
      this.edit(c => {
        const st = stepFor(c, t, i);
        setNotes(st, [...notesOf(st), key], st.key);
      });
    } else {
      this.edit(c => {
        const st = stepFor(c, t, i);
        setKind(st, 'note', key);
        setNotes(st, [key]);
      });
    }
  }

  private moveRollNote(c: GrooveConfig, i: number, from: number, to: number): void {
    const src = sourceOf(c, this.t, i);
    if (src < 0 || to < 0 || to > 127) return;
    const st = stepFor(c, this.t, src), keys = notesOf(st);
    if (!keys.includes(from) || (to !== from && keys.includes(to))) return;
    setNotes(st, keys.map(k => k === from ? to : k), st.key === from ? to : st.key);
  }

  /** Roll projected into MIDI-key rows: each chord note has its own cell. */
  private renderRoll(): void {
    if (!this.cfg) return;
    const cfg = this.cfg, t = this.t, length = this.len();
    const notes = rollNotes(cfg, t);
    const appliedNotes = this.applied ? rollNotes(this.applied, t) : null;
    const keys = new Set<number>(notes.keys());
    for (let k = this.view.pen[t] - 6; k <= this.view.pen[t] + 6; k++) {
      if (k >= 0 && k <= 127) keys.add(k);
    }
    const roll = el('div', 'g-roll');
    roll.dataset.testid = 'groove-roll-' + this.id;
    roll.setAttribute('aria-label', 'Groove note roll, horizontally scrollable');
    const pitches = Array.from(keys).sort((a, b) => b - a);
    for (const key of pitches) {
      const row = el('div', 'g-rrow');
      const name = el('span', 'g-rlabel', noteName(key));
      if (inScale(key, this.view.root, this.view.scale)) name.classList.add('in-scale');
      const cells = el('div', 'g-rcells');
      cells.style.gridTemplateColumns = 'repeat(' + length + ', 24px)';
      for (let i = 0; i < length; i++) {
        const kind = notes.get(key)?.get(i) ?? 'rest';
        const cell = el('div', 'g-roll-cell ' + kind);
        cell.dataset.i = String(i);
        cell.dataset.key = String(key);
        cell.dataset.kind = kind;
        cell.dataset.testid = 'groove-roll-' + this.id + '-' + (i + 1) + '-' + key;
        cell.setAttribute('role', 'button');
        cell.setAttribute('aria-label', 'Step ' + (i + 1) + ', ' + noteName(key) + ': ' + kind);
        // One tab stop; arrow keys move through the Roll without entering 800+ tabs.
        cell.tabIndex = i === this.view.sel[t] && key === this.view.pen[t] ? 0 : -1;
        cell.addEventListener('keydown', e => {
          const focus = (step: number, pitch: number) => this.grid.querySelector<HTMLElement>(
            `.g-roll-cell[data-i="${step}"][data-key="${pitch}"]`)?.focus();
          const vertical = e.key === 'ArrowUp' || e.key === 'ArrowDown';
          const horizontal = e.key === 'ArrowLeft' || e.key === 'ArrowRight';
          if (vertical || horizontal) {
            e.preventDefault(); e.stopPropagation();
            const step = horizontal ? clamp(i + (e.key === 'ArrowRight' ? 1 : -1), 0, length - 1) : i;
            const index = pitches.indexOf(key);
            const pitch = vertical ? pitches[clamp(index + (e.key === 'ArrowDown' ? 1 : -1), 0, pitches.length - 1)]! : key;
            this.view.pen[t] = pitch;
            this.select(step);
            focus(step, pitch);
          } else if (e.key === 'Enter' || e.key === ' ') {
            e.preventDefault(); e.stopPropagation();
            this.view.pen[t] = key;
            if (kind === 'rest') this.enterRollNote(i, key);
            this.select(i); focus(i, key);
          } else if (e.key === 'Delete' || e.key === 'Backspace') {
            e.preventDefault(); e.stopPropagation();
            this.deleteRollNote(i, key);
            this.view.pen[t] = key; this.select(i); focus(i, key);
          }
        });
        if (i === this.view.sel[t]) cell.classList.add('sel');
        if ((i * trackDiv(cfg, t)) % 16 === 0) cell.classList.add('bar-edge');
        if (kind === 'note') {
          const st = stepAt(cfg, t, i), condition = st.cond, pitches = notesOf(st);
          if (condition) cell.append(el('span', 'g-rcond', condLabel(condition)));
          if (pitches.length > 1 && key === Math.max(...pitches))
            cell.append(el('span', 'g-rch', stepLabel(st)));
        }
        if (appliedNotes && kind !== (appliedNotes.get(key)?.get(i) ?? 'rest'))
          cell.classList.add('chg');
        cells.append(cell);
      }
      row.append(name, cells);
      roll.append(row);
    }
    // Roll value lanes share the step axis and scroll with note rows.
    // Every click is a single document/history edit. Only the active note has a velocity.
    for (const lane of ['velocity', 'gate', 'mod1', 'mod2'] as const) {
      const row = el('div', 'g-rrow g-vrow');
      const label = el('span', 'g-rlabel', { velocity: 'VEL', gate: 'GATE', mod1: 'MOD 1', mod2: 'MOD 2' }[lane]);
      const cells = el('div', 'g-rcells');
      cells.style.gridTemplateColumns = 'repeat(' + length + ', 24px)';
      for (let i = 0; i < length; i++) {
        const st = stepAt(cfg, t, i), value = lane === 'velocity' ? st.velocity ?? 0.8 :
          lane === 'gate' ? st.gate ?? 1 : st[lane] ?? 0;
        const disabled = lane === 'velocity' && st.kind !== 'note';
        const cell = el('div', 'g-vcell');
        cell.dataset.i = String(i);
        cell.dataset.testid = 'groove-roll-' + lane + '-' + this.id + '-' + (i + 1);
        cell.setAttribute('role', 'slider');
        cell.setAttribute('aria-label', lane + ', step ' + (i + 1));
        cell.setAttribute('aria-valuemin', '0');
        cell.setAttribute('aria-valuemax', '1');
        cell.setAttribute('aria-valuenow', String(value));
        cell.tabIndex = !disabled && i === this.view.sel[t] ? 0 : -1;
        if (disabled) { cell.classList.add('disabled'); cell.setAttribute('aria-disabled', 'true'); }
        if ((i * trackDiv(cfg, t)) % 16 === 0) cell.classList.add('bar-edge');
        const stem = el('span', 'g-vstem');
        stem.style.height = (value * 75) + '%';
        const dot = el('span', 'g-vdot');
        stem.append(dot); cell.append(stem);
        const editValue = (fraction: number) => {
          this.view.sel[t] = i; this.view.selected[t] = true;
          const f = clamp(fraction, 0, 1);
          this.edit(c => {
            const step = stepFor(c, t, i);
            if (lane === 'velocity') step.velocity = Math.round(f * 100) / 100;
            else if (lane === 'gate') {
              const g = GATES[Math.max(0, Math.min(3, Math.round(f * 4 - 1)))]!;
              if (g === 1) delete step.gate; else step.gate = g;
            } else step[lane] = Math.round(f * 100) / 100;
          });
          this.renderGrid(); this.renderInspector();
          this.grid.querySelector<HTMLElement>('[data-testid="groove-roll-' + lane +
            '-' + this.id + '-' + (i + 1) + '"]')?.focus();
        };
        cell.addEventListener('pointerdown', e => {
          e.preventDefault(); e.stopPropagation();
          if (disabled) return;
          const rect = cell.getBoundingClientRect();
          editValue(1 - (e.clientY - rect.top) / rect.height);
        });
        cell.addEventListener('keydown', e => {
          if (disabled) return;
          const delta = e.key === 'ArrowUp' || e.key === 'ArrowRight' ? 0.05 :
            e.key === 'ArrowDown' || e.key === 'ArrowLeft' ? -0.05 : null;
          if (delta === null && e.key !== 'Home' && e.key !== 'End') return;
          e.preventDefault(); e.stopPropagation();
          editValue(e.key === 'Home' ? 0 : e.key === 'End' ? 1 : value + delta!);
        });
        cells.append(cell);
      }
      row.append(label, cells); roll.append(row);
    }
    this.grid.append(roll);
  }

  private rollDelete(e: MouseEvent): void {
    const cell = (e.target as HTMLElement).closest<HTMLElement>('.g-roll-cell');
    if (!cell || !this.cfg || this.view.mode !== 'roll') return;
    const i = Number(cell.dataset.i), key = Number(cell.dataset.key), t = this.t;
    const src = sourceOf(this.cfg, t, i);
    if (src < 0 || !notesOf(stepAt(this.cfg, t, src)).includes(key)) return;
    e.preventDefault(); e.stopPropagation();
    this.deleteRollNote(i, key);
  }

  /** Empty cells enter a note; dragging right extends ties; dragging up/down changes pitch. */
  private rollDown(e: PointerEvent): void {
    const cell = (e.target as HTMLElement).closest<HTMLElement>('.g-roll-cell');
    if (!cell || e.button !== 0 || !this.cfg) return;
    e.preventDefault(); e.stopPropagation();
    cell.focus();
    const t = this.t, i = Number(cell.dataset.i), key = Number(cell.dataset.key);
    const orig = JSON.parse(JSON.stringify(this.cfg)) as GrooveConfig;
    const current = rollNotes(orig, t).get(key)?.get(i);
    this.view.sel[t] = i;
    this.view.selected[t] = true;
    this.renderInspector();
    let mode: 'tie' | 'pitch' | null = null, last = key;
    const move = (ev: PointerEvent) => {
      const dx = ev.clientX - e.clientX, dy = ev.clientY - e.clientY;
      const hit = (document.elementFromPoint(ev.clientX, ev.clientY) as HTMLElement | null)?.closest<HTMLElement>('.g-roll-cell');
      if (!mode && Math.abs(dx) > 6 && Math.abs(dx) > Math.abs(dy) && hit && Number(hit.dataset.i) > i) mode = 'tie';
      if (!mode && Math.abs(dy) > 6 && Math.abs(dy) > Math.abs(dx) && current) mode = 'pitch';
      if (mode === 'tie' && hit) {
        const end = Math.min(this.len() - 1, Number(hit.dataset.i));
        if (end <= i || end === last) return;
        last = end;
        this.edit(cfg => {
          const st = stepFor(cfg, t, i);
          if (st.kind !== 'note') { setKind(st, 'note', key); setNotes(st, [key]); }
          for (let j = i + 1; j <= end; j++) setKind(stepFor(cfg, t, j), 'tie');
        }, orig);
      } else if (mode === 'pitch') {
        const next = this.stepPitch(key, Math.round(-dy / 24), ev.shiftKey);
        if (next === last) return;
        last = next;
        this.edit(cfg => this.moveRollNote(cfg, i, key, next), orig);
      }
    };
    const up = (ev: PointerEvent) => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      window.removeEventListener('pointercancel', up);
      // Two clicks (no drag) on the same existing note delete it. Counted here because
      // the first click re-renders the grid, so the browser's dblclick never fires.
      // A pointercancel (touch scroll, gesture takeover) is never a click.
      const click = !mode && ev.type === 'pointerup';
      const now = performance.now(), prev = this.lastRollPress;
      this.lastRollPress = click && current ? { i, key, at: now } : null;
      if (click && current && prev && prev.i === i && prev.key === key && now - prev.at < 400) {
        this.lastRollPress = null;
        this.deleteRollNote(i, key);
        return;
      }
      if (!mode && !current) this.enterRollNote(i, key);
      else if (!mode) this.select(i);
      this.grid.querySelector<HTMLElement>(
        `.g-roll-cell[data-i="${i}"][data-key="${key}"]`)?.focus();
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    window.addEventListener('pointercancel', up);
  }

  /** Keyboard shortcuts operate only while the Groove pane owns focus. */
  private keyDown(e: KeyboardEvent): void {
    const target = e.target as HTMLElement;
    if (target.matches('input, select, textarea') || target.isContentEditable || !this.cfg) return;
    const k = e.key.toLowerCase(), t = this.t, i = this.view.sel[t];
    if ((e.ctrlKey || e.metaKey) && k === 'z') {
      e.preventDefault(); e.stopPropagation();
      if (e.shiftKey) this.host.redo(); else this.host.undo();
      return;
    }
    if (e.ctrlKey || e.metaKey || e.altKey) return;
    const piano: Record<string, number> = {
      a: 0, w: 1, s: 2, e: 3, d: 4, f: 5, t: 6, g: 7,
      y: 8, h: 9, u: 10, j: 11, k: 12,
    };
    let handled = true;
    if (e.code === 'Space') void this.togglePlay();
    else if (e.key === 'ArrowLeft') this.select(i - 1);
    else if (e.key === 'ArrowRight') this.select(i + 1);
    else if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      const src = sourceOf(this.cfg, t, i);
      if (src < 0) this.edit(cfg => this.setPitch(cfg, i, this.stepPitch(this.view.pen[t], e.key === 'ArrowUp' ? 1 : -1, e.shiftKey)));
      else this.edit(cfg => this.moveStep(cfg, i, e.key === 'ArrowUp' ? 1 : -1, e.shiftKey));
    } else if (k === 'z') { this.view.octave = clamp(this.view.octave - 1, -1, 9); }
    else if (k === 'x') { this.view.octave = clamp(this.view.octave + 1, -1, 9); }
    else if (k in piano) {
      if (!e.repeat && !this.held.has(k)) {
        if (!this.entry) { this.entry = { track: t, step: i }; this.heldNotes = []; }
        const pitch = clamp((this.view.octave + 1) * 12 + piano[k]!, 0, 127);
        // Count every physical key for release, even when its pitch exceeds the five-voice limit.
        this.held.set(k, pitch);
        if (this.heldNotes.includes(pitch) || this.heldNotes.length < MAX_KEYS) {
          if (!this.heldNotes.includes(pitch)) this.heldNotes.push(pitch);
        } else this.toast('5 notes per step');
      }
    }
    else if (k === '-') this.edit(cfg => setKind(stepFor(cfg, t, i), 'tie'));
    else if (e.key === 'Backspace' || e.key === 'Delete') this.edit(cfg => setKind(stepFor(cfg, t, i), 'rest'));
    else if (k === '1' || k === '2') { this.view.t = Number(k) - 1; this.refresh(); }
    else handled = false;
    if (handled) { e.preventDefault(); e.stopPropagation(); }
  }

  /** Forget held keys without writing anything. */
  private dropHeld(): void { this.held.clear(); this.entry = null; this.heldNotes = []; }
  /** Commit one History entry only when every simultaneously pressed piano key is released. */
  private keyUp(e: KeyboardEvent): void {
    const key = e.key.toLowerCase();
    if (!this.held.delete(key) || this.held.size || !this.entry || !this.heldNotes.length) return;
    const { track, step } = this.entry, keys = [...this.heldNotes], root = keys[0]!;
    this.entry = null; this.heldNotes = [];
    this.view.t = track;
    this.view.pen[track] = root;
    // Preserve G2b's one-key cursor semantics; a held chord advances on release.
    this.view.sel[track] = keys.length > 1 ? (step + 1) % this.len(track) : step;
    this.view.selected[track] = true;
    this.edit(c => {
      const st = stepFor(c, track, step);
      setKind(st, 'note', root);
      setNotes(st, keys, root);
    });
  }

  private renderInspector(): void {
    this.insp.replaceChildren();
    if (!this.cfg) return;
    const t = this.t, i = this.view.sel[t], s = this.step(i), src = sourceOf(this.cfg, t, i);
    const head = el('div', 'g-istep', `STEP ${String(i + 1).padStart(2, '0')}`);
    head.append(el('small', '', `bar ${Math.floor(i * trackDiv(this.cfg!, t) / 16) + 1} · T${t + 1}`));
    const field = (label: string, ...kids: HTMLElement[]) => {
      const f = el('div', 'g-f'); f.append(el('span', 'g-lbl', label), ...kids); return f;
    };
    const kind = el('span', 'g-seg');
    for (const k of ['rest', 'note', 'tie'] as Kind[]) {
      const b = this.button(s.kind === k ? 'on' : '', `kind-${k}`, '', k[0]!.toUpperCase() + k.slice(1));
      b.addEventListener('click', () => this.edit(c => setKind(stepFor(c, t, i), k, this.view.pen[t])));
      kind.append(b);
    }
    const sourceStep = src >= 0 ? this.step(src) : s;
    const isChord = src >= 0 && notesOf(sourceStep).length > 1;
    const notes = el('span', 'g-seg g-note-kind');
    for (const mode of ['Single', 'Chord'] as const) {
      const b = this.button((mode === 'Chord') === isChord ? 'on' : '', 'notes-' + mode.toLowerCase(), '', mode);
      b.disabled = src < 0;
      b.addEventListener('click', () => {
        if (src < 0) return;
        if (mode === 'Single' && isChord) this.edit(c => {
          const st = stepFor(c, t, src);
          setNotes(st, [st.key ?? 60]);
        });
        else if (mode === 'Chord' && !isChord) this.edit(c => {
          const st = stepFor(c, t, src), root = st.key ?? 60;
          setNotes(st, this.view.scale === 'Chromatic'
            ? qualityKeys(root, 'maj') : scaleStack(root, 3, this.scaleCtx()), root);
        });
      });
      notes.append(b);
    }
    const chordControls = el('span', 'g-chord-controls');
    if (isChord) {
      const picker = this.button('g-ccbtn', 'chord-picker', 'Chord voicing and quality', stepLabel(sourceStep) + ' ▾');
      picker.addEventListener('click', e => { e.stopPropagation(); this.openChord(picker, src); });
      chordControls.append(picker);
      for (const [sign, delta] of [['◁', -1], ['▷', 1]] as const) {
        const b = this.button('', delta < 0 ? 'invert-down' : 'invert-up', 'Invert chord', sign);
        b.addEventListener('click', () => this.edit(c => {
          const st = stepFor(c, t, src);
          const result = invert(notesOf(st), st.key ?? 60, delta);
          if (result) setNotes(st, result.keys, result.root);
        }));
        chordControls.append(b);
      }
    }
    const note = el('input', 'g-in big') as HTMLInputElement;
    note.dataset.testid = `groove-note-${this.id}`;
    note.value = src >= 0 ? (s.kind === 'tie' ? '⌒ ' : '') + noteName(this.step(src).key ?? 60) : '—';
    note.disabled = src < 0;
    note.addEventListener('change', () => {
      const k = parseNote(note.value.replace('⌒', ''));
      if (k === null) { this.renderInspector(); return; }
      this.edit(c => this.setPitch(c, i, k));
    });
    const vel = el('input', 'g-in') as HTMLInputElement;
    vel.type = 'number'; vel.min = '0'; vel.max = '100';
    vel.dataset.testid = `groove-vel-${this.id}`;
    vel.disabled = s.kind !== 'note';
    vel.value = s.kind === 'note' ? String(Math.round((s.velocity ?? 0.8) * 100)) : '';
    vel.addEventListener('change', () => {
      const n = Number(vel.value);
      if (Number.isFinite(n)) this.edit(c => { stepFor(c, t, i).velocity = Math.round(clamp(n, 0, 100)) / 100; });
    });
    const gate = el('span', 'g-seg');
    for (const q of GATES) {
      const b = this.button((s.gate ?? 1) === q ? 'on' : '', `gate-${q}`, `Gate ${q} of a step`, GATE_LABEL[q]);
      b.addEventListener('click', () => this.edit(c => {
        const st = stepFor(c, t, i);
        if (q === 1) delete st.gate; else st.gate = q;
      }));
      gate.append(b);
    }
    const modField = (key: 'mod1' | 'mod2'): HTMLElement => {
      const input = el('input', 'g-in') as HTMLInputElement;
      input.type = 'number'; input.min = '0'; input.max = '100'; input.step = '1';
      input.dataset.testid = 'groove-' + key + '-' + this.id;
      input.value = String(Math.round((s[key] ?? 0) * 100));
      input.addEventListener('change', () => {
        const n = Number(input.value);
        if (Number.isFinite(n)) this.edit(c => {
          stepFor(c, t, i)[key] = Math.round(clamp(n, 0, 100)) / 100;
        });
      });
      return input;
    };
    const cond = this.button('g-ccbtn', 'cond', 'Trig condition (note trigs only)', condLabel(s.cond, true));
    cond.disabled = s.kind !== 'note';
    cond.addEventListener('click', e => { e.stopPropagation(); this.openCond(cond, i); });
    this.insp.append(head, field('Trig', kind), field(isChord ? 'Root' : 'Note', note),
      field('Notes', notes, chordControls), field('Velocity %', vel),
      field('Length', gate), field('Mod 1 %', modField('mod1')), field('Mod 2 %', modField('mod2')),
      field('Condition', cond));
  }

  private openChord(anchor: HTMLElement, i: number): void {
    this.closePop();
    const t = this.t, s = this.step(i), root = s.key ?? 60, scale = this.scaleCtx();
    const pop = el('div', 'g-pop g-chord-pop');
    pop.dataset.testid = `groove-chord-pop-${this.id}`;
    const group = (name: string) => pop.append(el('div', 'g-ph', name));
    const option = (value: string, keys: number[], label?: string) => {
      const b = this.button(keys.every(k => chordInScale(k, scale)) ? '' : 'out',
        'chord-opt-' + value, keys.map(noteName).join(' '), label ?? chordName(keys, root) ?? noteName(root));
      b.addEventListener('click', e => {
        e.stopPropagation();
        this.closePop();
        this.edit(c => {
          const st = stepFor(c, t, i);
          setNotes(st, keys, value === 'single' ? st.key : keys[0]);
        });
      });
      pop.append(b);
    };
    if (this.view.scale !== 'Chromatic') {
      group(`In ${ROOTS[this.view.root]} ${this.view.scale}`);
      option('scale3', scaleStack(root, 3, scale));
      option('scale4', scaleStack(root, 4, scale));
    }
    for (const [name, qualities] of PICKER_GROUPS) {
      group(name);
      for (const quality of qualities) option(quality, qualityKeys(root, quality));
    }
    group('One note');
    option('single', [root], noteName(root));
    pop.addEventListener('pointerdown', e => e.stopPropagation());
    anchor.after(pop);
    this.pop = pop;
  }

  private openCond(anchor: HTMLElement, i: number): void {
    this.closePop();
    const t = this.t, current = this.step(i).cond;
    const pop = el('div', 'g-pop');
    pop.dataset.testid = `groove-cond-pop-${this.id}`;
    const head = (text: string) => pop.append(el('div', 'g-ph', text));
    CONDS.forEach((c, n) => {
      if (n === 0) head('Always');
      if (n === 1) head('Chance');
      if (n === 6) head(this.cfg && trackCycle(this.cfg, t) === 16
        ? 'A:B · plays on loop A of every B (loops = bars here)'
        : 'A:B · plays on loop A of every B loops');
      if (n === 13) head('Loop');
      const b = this.button(sameCond(c, current) ? 'on' : '', `cond-opt-${n}`, condLabel(c, true), condLabel(c, true));
      b.addEventListener('click', ev => {
        ev.stopPropagation(); this.closePop();
        this.edit(cfg => { const st = stepFor(cfg, t, i); if (c) st.cond = { ...c }; else delete st.cond; });
      });
      pop.append(b);
    });
    pop.addEventListener('pointerdown', e => e.stopPropagation());
    anchor.after(pop);
    this.pop = pop;
    const close = (e: PointerEvent) => { if (!pop.contains(e.target as Node)) this.closePop(); };
    setTimeout(() => window.addEventListener('pointerdown', close, { once: true }), 0);
  }
  private closePop(): void { this.pop?.remove(); this.pop = null; }

  // ---- gestures -----------------------------------------------------------

  private padAt(x: number, y: number): number {
    const p = (document.elementFromPoint(x, y) as HTMLElement | null)?.closest<HTMLElement>('.g-pad');
    return p && this.grid.contains(p) ? Number(p.dataset.i) : -1;
  }

  /** Select; second click toggles; vertical drag = pitch (Shift octave); horizontal drag = tie. */
  private padDown(e: PointerEvent): void {
    const pad = (e.target as HTMLElement).closest<HTMLElement>('.g-pad');
    if (!pad || e.button !== 0 || !this.cfg) return;
    e.preventDefault(); e.stopPropagation();
    const t = this.t, i0 = Number(pad.dataset.i), wasSel = this.view.selected[t] && this.view.sel[t] === i0;
    const orig = JSON.parse(JSON.stringify(this.cfg)) as GrooveConfig;
    const src = sourceOf(orig, t, i0), key0 = src >= 0 ? stepAt(orig, t, src).key ?? 60 : null;
    let mode: 'pitch' | 'tie' | null = null, lastKey = 0, lastTie = i0;
    this.root.focus();
    if (!wasSel) this.select(i0);
    const move = (ev: PointerEvent) => {
      const dx = ev.clientX - e.clientX, dy = ev.clientY - e.clientY;
      if (!mode) {
        if (Math.abs(dy) > 6 && Math.abs(dy) > Math.abs(dx) && key0 !== null) mode = 'pitch';
        else if (Math.abs(dx) > Math.abs(dy) && this.padAt(ev.clientX, ev.clientY) > i0) mode = 'tie';
      }
      if (mode === 'pitch' && key0 !== null) {
        const n = Math.round(-dy / 14);
        if (n !== lastKey) { lastKey = n; this.edit(c => this.moveStep(c, i0, n, ev.shiftKey), orig); }
      } else if (mode === 'tie') {
        const i = this.padAt(ev.clientX, ev.clientY);
        if (i < 0 || i === lastTie) return;
        lastTie = i;
        this.edit(c => {
          if (i <= i0) return;
          const first = stepFor(c, t, i0);
          if (first.kind !== 'note') setKind(first, 'note', this.view.pen[t]);
          for (let j = i0 + 1; j <= i; j++) setKind(stepFor(c, t, j), 'tie');
        }, orig);
      }
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      window.removeEventListener('pointercancel', up);
      if (mode || !wasSel) return;
      this.edit(c => {
        const s = stepFor(c, t, i0);
        if (s.kind === 'rest' || (s.kind === 'tie' && sourceOf(c, t, i0) < 0)) setKind(s, 'note', this.view.pen[t]);
        else setKind(s, 'rest');
      });
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    window.addEventListener('pointercancel', up);
  }

  private wheel(e: WheelEvent): void {
    const pad = (e.target as HTMLElement).closest<HTMLElement>('.g-pad');
    if (!pad || !this.cfg) return;
    const t = this.t, i = Number(pad.dataset.i), src = sourceOf(this.cfg, t, i);
    if (src < 0) return;
    e.preventDefault(); e.stopPropagation();
    this.wheelAcc += e.deltaY || e.deltaX;
    if (Math.abs(this.wheelAcc) < 40) return;
    const n = this.wheelAcc < 0 ? 1 : -1;
    this.wheelAcc = 0;
    if (this.view.sel[t] !== i || !this.view.selected[t]) this.select(i);
    this.edit(c => this.moveStep(c, i, n, e.shiftKey));
  }

  // ---- live (telemetry only) ------------------------------------------------

  private paintLive(): void {
    const ext = this.host.clockPatched(this.id);
    this.extBadge.hidden = !ext;
    this.bpm.disabled = !this.cfg || ext;
    this.bpm.title = ext ? 'Following the clock input; unplug to edit BPM' : '';
    const ph = this.measured(), obs = this.last?.groove?.[this.id];
    const running = this.running(), canApply = this.host.canApply(this.id), draft = this.host.isDraft(this.id);
    if (!running) this.stopRequested = false;
    if (this.pendingApply && canApply && !this.applying) void this.doApply();
    this.play.innerHTML = running ? ICON_STOP : ICON_PLAY;
    this.play.classList.toggle('on', running);
    this.play.classList.toggle('q', running && this.stopRequested);
    const s = ph?.[this.t] ?? -1;
    const liveConfig = this.applied ?? this.cfg;
    const sixteenth = s * (liveConfig ? trackDiv(liveConfig, this.t) : 1);
    this.status.textContent = this.applying ? 'LOADING…'
      : running && this.stopRequested ? 'STOPS at loop end'
      : running && s >= 0 ? `PLAYING ${Math.floor(sixteenth / 16) + 1}.${Math.floor((sixteenth % 16) / 4) + 1}.${(sixteenth % 4) + 1}`
      : running ? 'PLAYING' : 'STOPPED';
    this.draftEl.textContent = draft ? 'Draft · not playing yet' : '';
    this.draftEl.hidden = !draft;
    this.apply.disabled = !draft || !canApply || this.applying;
    this.apply.title = canApply ? 'Load this pattern (the Groove starts stopped)'
      : 'Apply waits for a measured stop: stop the Groove first';
    this.stopApply.hidden = !draft || !running;
    this.stopApply.classList.toggle('soft', this.pendingApply);
    this.revert.disabled = !draft || this.applying;
    this.undoBtn.disabled = !this.host.canUndo();
    this.redoBtn.disabled = !this.host.canRedo();
    for (const t of [0, 1]) {
      const pos = this.tracksRow.querySelector<HTMLElement>(`[data-testid="groove-pos-${this.id}-${t + 1}"]`);
      const step = ph?.[t] ?? -1;
      if (pos) pos.textContent = ph ? `${step >= 0 ? String(step + 1).padStart(2, '0') : '--'}/${this.len(t)}` : '';
      pos?.parentElement?.querySelector('.g-sd')?.classList.toggle('run', !!obs?.[t]?.playing);
    }
    for (const n of this.grid.querySelectorAll('.play,.skip')) n.classList.remove('play', 'skip');
    for (const n of this.grid.querySelectorAll('.g-bar.cur')) n.classList.remove('cur');
    if (!ph || s < 0) return;
    if (this.view.mode === 'roll') {
      // The measured playhead spans every pitch and value lane, even on rests.
      for (const cell of this.grid.querySelectorAll<HTMLElement>(
        `.g-roll-cell[data-i="${s}"],.g-vcell[data-i="${s}"]`
      )) cell.classList.add('play');
      if (obs?.[this.t]?.fired === false) {
        const sounding = this.grid.querySelector<HTMLElement>(
          `.g-roll-cell.note[data-i="${s}"],.g-roll-cell.tie[data-i="${s}"]`);
        sounding?.classList.add('skip');
      }
      return;
    }
    const pad = this.grid.querySelector<HTMLElement>(`.g-pad[data-i="${s}"]`);
    if (!pad) return;
    pad.classList.add('play');
    if (pad.classList.contains('g-pad')) pad.nextElementSibling?.classList.add('play');
    pad.closest('.g-bar')?.classList.add('cur');
    // Skipped this pass: the measured condition result for the current trig failed.
    if (obs?.[this.t]?.fired === false && (pad.classList.contains('note') || pad.classList.contains('tie')))
      pad.classList.add('skip');
  }

  destroy(): void {
    this.closePop();
    if (this.toastTimer !== undefined) window.clearTimeout(this.toastTimer);
    for (const u of this.unbind) u();
  }
}
