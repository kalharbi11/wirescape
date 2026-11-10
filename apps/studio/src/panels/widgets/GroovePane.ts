// SPDX-License-Identifier: GPL-3.0-only
import type { Ticker } from '../../runtime/Ticker';
import type { Telemetry } from '../../engine/EngineClient';
import { el } from '../../app/dom';
import {
  CONDS, GATES, GATE_LABEL, LENGTHS, PEN, condLabel, noteName, parseNote, readConfig, sameCond,
  setKind, setLength, sourceOf, stepAt, stepFor, trackLength, upgrade,
  type GrooveConfig, type Kind, type Step,
} from '../../groove/pattern';
import { ROOTS, SCALES, type Scale, barStart, clearBar, copyBar, inScale, movePitch, pasteBar, rollNotes } from '../../groove/editorHelpers';
import '../../styles/groove.css';

/** What the Groove editor may ask of the app. Nothing here starts audio on its own. */
export interface GrooveHost {
  config(id: number): unknown;
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
}

interface View { t: number; sel: [number, number]; lane: 'trig' | 'note'; pen: [number, number];
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
  private unbind: (() => void)[] = [];

  constructor(private readonly id: number, ticker: Ticker, private readonly host: GrooveHost,
    private readonly layoutChanged: () => void = () => undefined) {
    this.view = views.get(id) ?? { t: 0, sel: [0, 0], lane: 'trig', pen: [PEN[0]!, PEN[1]!],
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
    this.root.dataset.look = this.view.look;
    this.root.dataset.testid = `groove-${id}`;
    this.status.dataset.testid = `groove-status-${id}`;
    this.draftEl.dataset.testid = `groove-draft-${id}`;
    this.bpm.type = 'number'; this.bpm.min = '20'; this.bpm.max = '300'; this.bpm.step = '0.1';
    this.bpm.dataset.testid = `groove-bpm-${id}`;
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
    head.append(el('span', 'g-identity', 'GRV  Groove'), el('span', 'g-sp'));
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
    this.root.append(head, transport, this.tracksRow, this.lanesRow, this.grid, this.insp);
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
    s.key = clamp(key, 0, 127);
    this.view.pen[this.t] = s.key;
  }
  private select(i: number): void {
    this.view.sel[this.t] = clamp(i, 0, this.len() - 1);
    this.closePop();
    this.renderGrid(); this.renderInspector();
  }

  // ---- transport ----------------------------------------------------------

  private measured(): [number, number] | undefined { return this.last?.playhead?.[this.id]; }
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
      tab.append(dot, el('span', 'g-tn', `T${t + 1}`), el('span', 'g-nm', t ? 'Bass' : 'Lead'), pos, mute);
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
    this.tracksRow.append(seg);
  }

  private renderLanes(): void {
    const seg = el('span', 'g-seg underline');
    for (const [lane, label] of [['trig', 'Trig'], ['note', 'Note']] as const) {
      const b = this.button(lane === this.view.lane ? 'on' : '', 'lane-' + lane, '', label);
      b.addEventListener('click', () => { this.view.lane = lane; this.refresh(); });
      seg.append(b);
    }
    const root = el('select', 'g-select') as HTMLSelectElement;
    root.dataset.testid = 'groove-scale-root-' + this.id;
    root.setAttribute('aria-label', 'Scale root');
    ROOTS.forEach((name, i) => {
      const opt = el('option', '', name) as HTMLOptionElement;
      opt.value = String(i); root.append(opt);
    });
    root.value = String(this.view.root);
    root.addEventListener('change', () => { this.view.root = Number(root.value); this.renderGrid(); });
    const scale = el('select', 'g-select') as HTMLSelectElement;
    scale.dataset.testid = 'groove-scale-' + this.id;
    scale.setAttribute('aria-label', 'Scale helper (display and pitch gestures only)');
    (Object.keys(SCALES) as Scale[]).forEach(name => {
      const opt = el('option', '', name) as HTMLOptionElement;
      opt.value = name; scale.append(opt);
    });
    scale.value = this.view.scale;
    scale.addEventListener('change', () => { this.view.scale = scale.value as Scale; this.renderGrid(); });
    const copy = this.button('', 'copy-bar', 'Copy the selected 16-step bar', 'Copy');
    copy.disabled = !this.cfg;
    copy.addEventListener('click', () => { if (this.cfg) this.clipboard = copyBar(this.cfg, this.t, this.view.sel[this.t]); });
    const paste = this.button('', 'paste-bar', 'Paste into selected bar as one undo step', 'Paste');
    paste.disabled = !this.cfg || !this.clipboard;
    paste.addEventListener('click', () => {
      if (this.clipboard) this.edit(c => pasteBar(c, this.t, this.view.sel[this.t], this.clipboard!));
    });
    const clear = this.button('', 'clear-bar', 'Clear the selected bar as one undo step', 'Clear');
    clear.disabled = !this.cfg;
    clear.addEventListener('click', () => this.edit(c => clearBar(c, this.t, this.view.sel[this.t])));
    const look = el('select', 'g-select') as HTMLSelectElement;
    look.dataset.testid = 'groove-look-' + this.id;
    look.setAttribute('aria-label', 'Trig look');
    for (const name of ['graphite', 'oxblood', 'led'] as const) {
      const opt = el('option', '', name === 'led' ? 'LED' : name[0]!.toUpperCase() + name.slice(1)) as HTMLOptionElement;
      opt.value = name; look.append(opt);
    }
    look.value = this.view.look;
    look.addEventListener('change', () => { this.view.look = look.value as View['look']; this.root.dataset.look = this.view.look; });
    this.lanesRow.replaceChildren(seg, el('span', 'g-lbl', 'Scale'), root, scale, el('span', 'g-sp'),
      copy, paste, clear, look);
    if (!this.cfg) this.lanesRow.append(el('span', 'g-note',
      'This Groove pattern uses a newer format; it is kept as saved and is read-only here.'));
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
      bar.append(el('span', 'g-barn', String(b + 1)), pads);
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
    pad.setAttribute('aria-label', `Step ${i + 1}: ${s.kind === 'note' ? noteName(s.key ?? 60) : s.kind}`);
    if (s.kind === 'note') {
      pad.append(el('span', 'g-nn', noteName(s.key ?? 60)));
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

  private renderInspector(): void {
    this.insp.replaceChildren();
    if (!this.cfg) return;
    const t = this.t, i = this.view.sel[t], s = this.step(i), src = sourceOf(this.cfg, t, i);
    const head = el('div', 'g-istep', `STEP ${String(i + 1).padStart(2, '0')}`);
    head.append(el('small', '', `bar ${Math.floor(i / 16) + 1} · T${t + 1}`));
    const field = (label: string, ...kids: HTMLElement[]) => {
      const f = el('div', 'g-f'); f.append(el('span', 'g-lbl', label), ...kids); return f;
    };
    const kind = el('span', 'g-seg');
    for (const k of ['rest', 'note', 'tie'] as Kind[]) {
      const b = this.button(s.kind === k ? 'on' : '', `kind-${k}`, '', k[0]!.toUpperCase() + k.slice(1));
      b.addEventListener('click', () => this.edit(c => setKind(stepFor(c, t, i), k, this.view.pen[t])));
      kind.append(b);
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
    const cond = this.button('g-ccbtn', 'cond', 'Trig condition (note trigs only)', condLabel(s.cond, true));
    cond.disabled = s.kind !== 'note';
    cond.addEventListener('click', e => { e.stopPropagation(); this.openCond(cond, i); });
    this.insp.append(head, field('Trig', kind), field('Note', note), field('Velocity %', vel),
      field('Length', gate), field('Condition', cond));
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
      if (n === 6) head(this.len() === 16 ? 'A:B · plays on loop A of every B (loops = bars here)' : 'A:B · plays on loop A of every B loops');
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
    const t = this.t, i0 = Number(pad.dataset.i), wasSel = this.view.sel[t] === i0;
    const orig = JSON.parse(JSON.stringify(this.cfg)) as GrooveConfig;
    const src = sourceOf(orig, t, i0), key0 = src >= 0 ? stepAt(orig, t, src).key ?? 60 : null;
    let mode: 'pitch' | 'tie' | null = null, lastKey = key0, lastTie = i0;
    if (!wasSel) this.select(i0);
    const move = (ev: PointerEvent) => {
      const dx = ev.clientX - e.clientX, dy = ev.clientY - e.clientY;
      if (!mode) {
        if (Math.abs(dy) > 6 && Math.abs(dy) > Math.abs(dx) && key0 !== null) mode = 'pitch';
        else if (Math.abs(dx) > Math.abs(dy) && this.padAt(ev.clientX, ev.clientY) > i0) mode = 'tie';
      }
      if (mode === 'pitch' && key0 !== null) {
        const k = this.stepPitch(key0, Math.round(-dy / 14), ev.shiftKey);
        if (k !== lastKey) { lastKey = k; this.edit(c => this.setPitch(c, i0, k), orig); }
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
    if (this.view.sel[t] !== i) this.view.sel[t] = i;
    const key = this.stepPitch(stepAt(this.cfg, t, src).key ?? 60, n, e.shiftKey);
    this.edit(c => this.setPitch(c, i, key));
  }

  // ---- live (telemetry only) ------------------------------------------------

  private paintLive(): void {
    const ph = this.measured(), obs = this.last?.groove?.[this.id];
    const running = this.running(), canApply = this.host.canApply(this.id), draft = this.host.isDraft(this.id);
    if (!running) this.stopRequested = false;
    if (this.pendingApply && canApply && !this.applying) void this.doApply();
    this.play.innerHTML = running ? ICON_STOP : ICON_PLAY;
    this.play.classList.toggle('on', running);
    this.play.classList.toggle('q', running && this.stopRequested);
    const s = ph?.[this.t] ?? -1;
    this.status.textContent = this.applying ? 'LOADING…'
      : running && this.stopRequested ? 'STOPS at loop end'
      : running && s >= 0 ? `PLAYING ${Math.floor(s / 16) + 1}.${Math.floor((s % 16) / 4) + 1}.${(s % 4) + 1}`
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
    const pad = this.grid.querySelector<HTMLElement>(`.g-pad[data-i="${s}"]`);
    if (!pad) return;
    pad.classList.add('play');
    pad.nextElementSibling?.classList.add('play');
    pad.closest('.g-bar')?.classList.add('cur');
    // Skipped this pass: the measured condition result for the current trig failed.
    if (obs?.[this.t]?.fired === false && (pad.classList.contains('note') || pad.classList.contains('tie')))
      pad.classList.add('skip');
  }

  destroy(): void {
    this.closePop();
    for (const u of this.unbind) u();
  }
}
