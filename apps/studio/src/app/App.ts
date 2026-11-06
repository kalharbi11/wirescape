// SPDX-License-Identifier: GPL-3.0-only
import { connKey } from './../patch/types';
import { ModuleRegistry, findParam, findPort } from '../modules/registry';
import { PanelOverrides } from '../panels/layouts';
import { PatchDoc } from '../patch/PatchDoc';
import { History } from '../patch/History';
import type { Connection, PatchV2 } from '../patch/types';
import { FakeEngine } from '../engine/FakeEngine';
import { RealEngine } from '../engine/RealEngine';
import { EngineRefusal, type SystemStatus, type EngineClient } from '../engine/EngineClient';
import { Ticker } from '../runtime/Ticker';
import { PatchCanvas, type Selection } from '../canvas/PatchCanvas';
import { CanvasPane } from '../panels/widgets/canvasBase';
import type { PortDirection } from '../modules/types';
import { Library } from './Library';
import { QuickAdd } from './QuickAdd';
import { Toasts } from '../system/Toasts';
import { StatusBar } from '../system/StatusBar';
import { ComputerKeyboard, isTyping } from './Keyboard';
import { OnScreenKeyboard } from './OnScreenKeyboard';
import { DevPanel } from './DevPanel';
import { Modal, field, select, switchButton } from './Modal';
import { STARTERS, DEFAULT_STARTER } from './starters';
import { loadPrefs, savePrefs, applyPrefs, detectSlowDevice, type Preferences } from '../theme/theme';
import { el } from './dom';

const AUTOSAVE_KEY = 'wirescape.autosave.v2';
/** Copied modules survive a reload and a patch change, so paste works between patches. */
const CLIPBOARD_KEY = 'wirescape.clipboard.v1';

interface ClipModule { id: number; module: string; parameters: Record<string, number>; pos: [number, number]; label?: string; extra: Record<string, unknown> }
interface Clip { modules: ClipModule[]; connections: Connection[]; colors: Record<string, string> }

export class App {
  readonly root: HTMLElement;
  private doc = new PatchDoc();
  private ticker = new Ticker();
  private engine: FakeEngine | RealEngine;
  private maxModuleId = 0;
  private canvas!: PatchCanvas;
  private library!: Library;
  private quick!: QuickAdd;
  private toasts = new Toasts();
  private statusBar!: StatusBar;
  private keyboard!: ComputerKeyboard;
  private piano!: OnScreenKeyboard;
  private devPanel!: DevPanel;
  private settings = new Modal('settings-modal');
  private shortcuts = new Modal('shortcuts-modal');
  private prefs: Preferences;
  private patchName = 'Untitled';
  private zoomLabel!: HTMLElement;
  private dirtyDot!: HTMLElement;
  private nameEl!: HTMLElement;
  private octaveEl!: HTMLElement;
  private lastStatus: SystemStatus | null = null;
  private fileInput!: HTMLInputElement;
  private history!: History;
  private undoBtn!: HTMLButtonElement;
  private redoBtn!: HTMLButtonElement;
  private clip: Clip | null = null;
  private pasteCount = 0;

  constructor(
    private readonly registry: ModuleRegistry,
    private readonly overrides: PanelOverrides,
  ) {
    this.prefs = loadPrefs();
    if (detectSlowDevice() && this.prefs.profile === 'normal') this.prefs.profile = 'pi';
    applyPrefs(this.prefs);
    this.engine = window.wirescape
      ? new RealEngine(window.wirescape, () => this.doc.toJSON(), registry)
      : new FakeEngine(registry);
    this.root = el('div', 'app');
    this.root.dataset.testid = 'app';
  }

  static async boot(mount: HTMLElement): Promise<App> {
    const [registry, overrides] = await Promise.all([
      ModuleRegistry.load(),
      PanelOverrides.load(),
      // Card heights are measured once, so the faces must be ready first.
      Promise.all(['400 11px "IBM Plex Mono"', '500 11px "IBM Plex Mono"', '600 10px "IBM Plex Mono"',
        '400 13px "IBM Plex Sans"', '500 11px "IBM Plex Sans"'].map(f => document.fonts.load(f))).catch(() => undefined),
    ]);
    const app = new App(registry, overrides);
    app.build();
    mount.append(app.root);
    await app.startup();
    return app;
  }

  // ---- construction -----------------------------------------------------

  private build(): void {
    this.library = new Library(this.registry, {
      add: type => this.addModuleCentred(type),
      addAt: (type, x, y) => this.addModuleAt(type, x, y),
    });

    this.quick = new QuickAdd(this.registry);

    this.canvas = new PatchCanvas({
      doc: this.doc,
      ticker: this.ticker,
      registry: this.registry,
      overrides: this.overrides,
      commitParam: (m, p, v, c) => this.commitParam(m, p, v, c),
      commitModAmount: (c, a, commit) => this.commitModAmount(c, a, commit),
      listMidiDevices: () => this.engine.listMidiDevices(),
      setModuleEndpoint: (id, endpoint, sys) => this.setMidiEndpoint(id, endpoint, sys),
      describeSource: c => this.describeSource(c),
      cableColor: c => `var(--cable-${this.doc.cableColor(connKey(c)) ?? 'ink'})`,
      toast: (m, tone) => this.toasts.show(m, tone),
      quickAdd: (world, need, then) => this.openQuickAdd(world, need, then),
      confirmStereo: msg => this.confirmStereo(msg),
      onSelectionChange: s => this.onSelectionChange(s),
      rename: id => this.renameModule(id),
      engineConnect: c => {
        this.engine.connect(c);
        this.trackRollback(() => { this.doc.disconnect(c); this.canvas.shake(c.to.module); });
      },
      engineDisconnect: c => {
        this.engine.disconnect(c);
        this.trackRollback(() => { if (!this.doc.has(c)) this.doc.connect(c); });
      },
    });
    this.canvas.bindActivity();

    this.statusBar = new StatusBar({
      onPanic: () => { this.keyboard.releaseAll(); this.engine.panic(); this.toasts.show('All notes stopped.'); },
      onOpenSettings: () => this.openSettings(),
      onClockToggle: running => this.toggleClock(running),
    });

    this.keyboard = new ComputerKeyboard(this.engine);
    this.keyboard.setEnabled(this.prefs.computerKeyboardPlays);
    this.piano = new OnScreenKeyboard(this.keyboard);
    this.keyboard.onOctaveChange = o => { this.octaveEl.textContent = `OCT ${o}`; this.piano.refresh(); };
    this.devPanel = new DevPanel(this.engine, this.ticker);

    const main = el('div', 'main');
    main.append(this.buildTopBar(), this.canvas.root, this.piano.root, this.statusBar.root);

    this.fileInput = el('input') as HTMLInputElement;
    this.fileInput.type = 'file';
    this.fileInput.accept = '.json,application/json';
    this.fileInput.hidden = true;
    this.fileInput.dataset.testid = 'file-input';
    this.fileInput.addEventListener('change', () => void this.importFile());

    this.root.append(
      this.library.root, main, this.toasts.root, this.quick.root,
      this.devPanel.root, this.settings.root, this.shortcuts.root, this.fileInput,
    );

    this.attachDropTarget();
    this.attachShortcuts();
    this.buildShortcutSheet();
  }

  private buildTopBar(): HTMLElement {
    const bar = el('header', 'topbar');
    bar.dataset.testid = 'topbar';

    const left = el('div', 'topbar-left');
    const menuBtn = this.action('☰', () => this.root.classList.toggle('library-open'), 'menu-toggle');
    menuBtn.classList.add('icon-only', 'menu-btn');
    this.nameEl = el('span', 'patch-name', this.patchName);
    this.nameEl.dataset.testid = 'patch-name';
    this.nameEl.title = 'Click to rename the patch';
    this.nameEl.addEventListener('click', () => this.renamePatch());
    this.dirtyDot = el('span', 'dirty-dot');
    this.dirtyDot.dataset.testid = 'dirty-dot';
    this.dirtyDot.title = 'Unsaved changes';
    const path = el('div', 'patch-path');
    path.append(el('span', 'patch-prefix', 'PATCH /'), this.nameEl, this.dirtyDot);
    left.append(menuBtn, path);

    const right = el('div', 'topbar-right');

    const starters = el('select', 'input-select starter-select') as HTMLSelectElement;
    starters.dataset.testid = 'starter-select';
    starters.setAttribute('aria-label', 'Starter patches');
    const blank = document.createElement('option');
    blank.value = ''; blank.textContent = 'Starter patches…';
    starters.append(blank);
    for (const s of STARTERS) {
      const o = document.createElement('option');
      o.value = s.id; o.textContent = s.name; o.title = s.note;
      starters.append(o);
    }
    starters.addEventListener('change', () => {
      const s = STARTERS.find(x => x.id === starters.value);
      if (s) { this.loadPatch(s.patch(), s.name); this.toasts.show(`${s.name} loaded.`); }
      starters.value = '';
    });

    this.octaveEl = el('span', 'octave-tag', 'OCT 4');
    this.octaveEl.dataset.testid = 'octave';
    this.octaveEl.title = 'Computer keyboard octave · Z and X shift it';

    const zoomGroup = el('div', 'zoom-group');
    zoomGroup.append(
      this.action('−', () => this.canvas.viewport.zoomBy(1 / 1.15), 'zoom-out'),
      (this.zoomLabel = (() => {
        const o = el('output', 'zoom-label', '100%');
        o.dataset.testid = 'zoom-label';
        return o;
      })()),
      this.action('+', () => this.canvas.viewport.zoomBy(1.15), 'zoom-in'),
      this.action('⌗', () => this.canvas.fit(), 'zoom-fit'),
    );
    this.canvas.viewport.on(v => { this.zoomLabel.textContent = `${Math.round(v.s * 100)}%`; });

    const tools = el('div', 'toolbar');
    const keys = this.action('KEYS', () => {
      keys.setAttribute('aria-expanded', String(this.piano.toggle()));
      this.canvas.updateVisibility();
    }, 'toggle-keyboard');
    keys.setAttribute('aria-expanded', 'false');
    keys.setAttribute('aria-controls', this.piano.root.id);
    keys.title = 'Show or hide the playable keyboard';
    this.undoBtn = this.action('↶', () => this.undo(), 'undo');
    this.redoBtn = this.action('↷', () => this.redo(), 'redo');
    this.undoBtn.title = 'Undo (Ctrl/Cmd + Z)';
    this.redoBtn.title = 'Redo (Ctrl/Cmd + Shift + Z)';
    const history = el('div', 'zoom-group history-group');
    history.append(this.undoBtn, this.redoBtn);
    tools.append(
      keys,
      this.action('NEW', () => this.newPatch(), 'new-patch'),
      this.action('OPEN', () => void this.openPatchFile(), 'open-patch'),
      this.action('SAVE', () => void this.savePatchFile(), 'save-patch'),
      this.action('?', () => this.shortcuts.open('Keyboard shortcuts'), 'shortcuts-btn'),
    );

    right.append(starters, this.octaveEl, history, zoomGroup, tools);
    bar.append(left, right);
    return bar;
  }

  private action(label: string, fn: () => void, testid: string): HTMLButtonElement {
    const b = el('button', 'action', label) as HTMLButtonElement;
    b.type = 'button';
    b.dataset.testid = testid;
    b.addEventListener('click', fn);
    return b;
  }

  // ---- startup ----------------------------------------------------------

  private async startup(): Promise<void> {
    this.history = new History(this.doc, p => this.restoreSnapshot(p));
    this.history.onChange(() => this.refreshHistory());
    this.ticker.throttleHz = this.prefs.profile === 'pi' ? 30 : 0;
    this.ticker.start();
    this.engine.onTelemetry(t => this.ticker.push(t));
    this.engine.onStatus(s => { this.announceStatus(this.lastStatus, s); this.lastStatus = s; this.statusBar.update(s); });
    this.engine.onRefusal?.(r => this.toasts.show(r.detail.message, 'error'));
    if (this.engine instanceof RealEngine) {
      this.engine.onConnection(state => {
        this.statusBar.setEngineState(state);
        if (state === 'restarting') { this.ticker.resetTelemetry(); this.lastStatus = null; }
      });
      this.engine.onGraphError(e => this.toasts.show(e.message, 'error'));
      this.engine.onCatalogue(() => { this.library.refresh(); this.canvas.rebuild(); });
      window.wirescape?.onMessage(m => {
        if (m.type === 'app.menuOpenPatch') void this.openPatchFile();
        if (m.type === 'app.menuSavePatch') void this.savePatchFile(m.saveAs === true);
      });
    } else this.statusBar.setEngineState('simulated');
    this.startFrameWatchdog();
    this.engine.start();
    this.keyboard.attach();

    const restored = this.restoreAutosave();
    if (!restored) this.loadPatch(DEFAULT_STARTER.patch(), DEFAULT_STARTER.name);

    requestAnimationFrame(() => { this.canvas.fit(); this.canvas.invalidateAnchors(); });
    window.addEventListener('resize', () => this.canvas.updateVisibility());
    setInterval(() => this.autosave(), 4000);
    this.doc.on(() => this.refreshDirty());
  }

  private restoreAutosave(): boolean {
    try {
      const raw = localStorage.getItem(AUTOSAVE_KEY);
      if (!raw) return false;
      const saved = JSON.parse(raw) as { name?: string; patch: PatchV2 };
      if (!saved?.patch?.modules) return false;
      this.loadPatch(saved.patch, saved.name ?? 'Untitled');
      this.toasts.show('Restored your last patch.');
      return true;
    } catch { return false; }
  }

  private autosave(): void {
    if (!this.doc.dirty) return;
    try {
      localStorage.setItem(AUTOSAVE_KEY, JSON.stringify({ name: this.patchName, patch: this.doc.toJSON() }));
    } catch { /* storage full or unavailable */ }
  }

  // ---- patch lifecycle --------------------------------------------------

  async loadPatch(patch: PatchV2, name = 'Untitled'): Promise<void> {
    this.ticker.resetTelemetry();
    this.doc.replace(patch);
    this.patchName = name;
    this.nameEl.textContent = name;
    this.maxModuleId = Math.max(0, ...patch.modules.map(m => m.id));
    const running = this.engine.loadGraph(this.doc.toJSON());
    const v = patch.ui?.view;
    if (v && v.zoom) this.canvas.viewport.setView(v.x, v.y, v.zoom);
    else requestAnimationFrame(() => this.canvas.fit());
    requestAnimationFrame(() => this.canvas.invalidateAnchors());
    this.doc.markClean();
    this.refreshDirty();
    this.history?.reset();
    this.pasteCount = 0;
    try { await running; }
    catch (e) { this.toasts.show(e instanceof Error ? e.message : 'Patch could not run; the editable document was kept.', 'error'); }
  }

  // ---- system (phase 5) ---------------------------------------------------

  /** Toasts for things that change under the user: devices coming and going, audio dropping. */
  private announceStatus(prev: SystemStatus | null, s: SystemStatus): void {
    if (!prev) return;
    const before = new Map(prev.midi.devices.map(d => [d.id, d]));
    const after = new Map(s.midi.devices.map(d => [d.id, d]));
    for (const [id, d] of after) if (!before.has(id)) this.toasts.show(`MIDI: ${d.name} connected.`);
    for (const [id, d] of before) if (!after.has(id)) this.toasts.show(`MIDI: ${d.name} disconnected.`);
    const a = prev.audio.state, b = s.audio.state;
    if (a !== b) {
      if (b === 'device-lost') this.toasts.show(`Audio device lost: ${s.audio.deviceName ?? 'output'}. Reconnecting when it returns.`, 'error');
      else if (b === 'no-device') this.toasts.show('No audio device. Pick one in Settings.', 'error');
      else if (b === 'running' && (a === 'reconnecting' || a === 'device-lost' || a === 'no-device')) this.toasts.show('Audio is back.');
    }
  }

  /**
   * The Pi profile switches itself on (once a session) when frames stay below
   * 40 fps for 5 seconds in a row, unless the user turned that off.
   */
  private startFrameWatchdog(): void {
    let slowFor = 0;
    let fired = false;
    setInterval(() => {
      if (fired || !this.prefs.autoProfile || this.prefs.profile === 'pi' || document.hidden) { slowFor = 0; return; }
      const fps = this.devPanel.simulateSlow ? 20 : this.ticker.fps;
      slowFor = fps > 0 && fps < 40 ? slowFor + 1 : 0;
      if (slowFor >= 5) {
        fired = true;
        this.setPref({ profile: 'pi' });
        this.toasts.show('Frames were slow, so the Pi profile is on. You can change this in Settings.');
      }
    }, 1000);
  }

  private checkForUpdates(btn: HTMLButtonElement, out: HTMLElement): void {
    btn.disabled = true;
    out.textContent = 'Checking…';
    // Stub: the real app asks its update channel. Nothing leaves the machine here.
    setTimeout(() => {
      btn.disabled = false;
      out.textContent = `Up to date · ${__APP_VERSION__} · checked ${new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`;
    }, 600);
  }

  // ---- undo / redo --------------------------------------------------------

  undo(): void {
    if (!this.history.undo()) { this.toasts.show('Nothing to undo.'); return; }
  }

  redo(): void {
    if (!this.history.redo()) { this.toasts.show('Nothing to redo.'); return; }
  }

  /** Put a history snapshot back, keeping the view where it is. */
  private restoreSnapshot(p: PatchV2): void {
    const view = this.doc.ui.view;
    this.canvas.select({ kind: 'none' });
    this.ticker.resetTelemetry();
    this.doc.replace({ ...p, ui: { ...(p.ui ?? {}), view } });
    void this.engine.loadGraph(this.doc.toJSON()).catch(e =>
      this.toasts.show(e instanceof Error ? e.message : 'The snapshot could not run.', 'error'));
    requestAnimationFrame(() => this.canvas.invalidateAnchors());
    this.refreshDirty();
  }

  private refreshHistory(): void {
    this.undoBtn.disabled = !this.history.canUndo;
    this.redoBtn.disabled = !this.history.canRedo;
  }

  // ---- copy / paste --------------------------------------------------------

  private copySelection(): boolean {
    const s = this.canvas.currentSelection;
    if (s.kind !== 'modules' || !s.ids.length) return false;
    const ids = new Set(s.ids);
    const modules: ClipModule[] = s.ids.map(id => {
      const m = this.doc.modules.get(id)!;
      const { id: _id, module, parameters, ...extra } = m as typeof m & Record<string, unknown>;
      return { id, module, parameters: { ...(parameters ?? {}) }, pos: this.doc.position(id), label: this.doc.label(id), extra };
    });
    const connections = this.doc.connections
      .filter(c => ids.has(c.from.module) && ids.has(c.to.module))
      .map(c => JSON.parse(JSON.stringify(c)) as Connection);
    const colors: Record<string, string> = {};
    for (const c of connections) { const col = this.doc.cableColor(connKey(c)); if (col) colors[connKey(c)] = col; }
    this.clip = { modules, connections, colors };
    this.pasteCount = 0;
    try { localStorage.setItem(CLIPBOARD_KEY, JSON.stringify(this.clip)); } catch { /* storage unavailable */ }
    return true;
  }

  private copy(): void {
    const s = this.canvas.currentSelection;
    if (!this.copySelection()) { this.toasts.show('Select modules to copy.'); return; }
    const n = s.kind === 'modules' ? s.ids.length : 0;
    this.toasts.show(`Copied ${n} module${n > 1 ? 's' : ''}.`);
  }

  private cut(): void {
    if (!this.copySelection()) { this.toasts.show('Select modules to cut.'); return; }
    this.deleteSelection();
  }

  private duplicate(): void {
    if (!this.copySelection()) { this.toasts.show('Select modules to duplicate.'); return; }
    this.paste();
  }

  /**
   * Paste the clipboard: new instance ids, cables between pasted modules
   * kept (with their colours), and a cascade offset so repeated pastes don't
   * stack. If the originals are off screen, the group lands in the middle
   * of the view.
   */
  paste(): void {
    let clip = this.clip;
    if (!clip) {
      try { clip = JSON.parse(localStorage.getItem(CLIPBOARD_KEY) ?? 'null') as Clip | null; } catch { clip = null; }
    }
    if (!clip?.modules.length) { this.toasts.show('Nothing to paste.'); return; }
    this.pasteCount++;
    const xs = clip.modules.map(m => m.pos[0]), ys = clip.modules.map(m => m.pos[1]);
    const minX = Math.min(...xs), minY = Math.min(...ys), maxX = Math.max(...xs), maxY = Math.max(...ys);
    const r = this.canvas.viewport.rect();
    const tl = this.canvas.viewport.toWorld(r.left, r.top), br = this.canvas.viewport.toWorld(r.right, r.bottom);
    const visible = minX < br.x && maxX > tl.x && minY < br.y && maxY > tl.y;
    let dx = 40 * this.pasteCount, dy = 40 * this.pasteCount;
    if (!visible) {
      const cx = (tl.x + br.x) / 2, cy = (tl.y + br.y) / 2;
      dx = cx - (minX + maxX) / 2 - 150 + 24 * (this.pasteCount - 1);
      dy = cy - (minY + maxY) / 2 - 100 + 24 * (this.pasteCount - 1);
    }
    const map = new Map<number, number>();
    const skipped: string[] = [];
    for (const m of clip.modules) {
      const id = this.allocateId();
      try { this.engine.addModule(id, m.module, m.parameters); }
      catch { skipped.push(m.module); continue; }
      this.doc.addModule(m.module, m.parameters, m.pos[0] + dx, m.pos[1] + dy, id);
      this.trackRollback(() => this.doc.removeModule(id));
      const added = this.doc.modules.get(id);
      if (added) Object.assign(added, m.extra);
      if (m.label) this.doc.setLabel(id, m.label);
      map.set(m.id, id);
    }
    for (const c of clip.connections) {
      const from = map.get(c.from.module), to = map.get(c.to.module);
      if (from === undefined || to === undefined) continue;
      const nc: Connection = { ...c, from: { ...c.from, module: from }, to: { ...c.to, module: to } };
      if (this.doc.has(nc)) continue;
      this.doc.connect(nc);
      try { this.engine.connect(nc); this.trackRollback(() => this.doc.disconnect(nc)); }
      catch { this.doc.disconnect(nc); continue; }
      const col = clip.colors[connKey(c)];
      if (col) this.doc.setCableColor(connKey(nc), col);
    }
    const ids = [...map.values()];
    this.canvas.select(ids.length ? { kind: 'modules', ids } : { kind: 'none' });
    this.canvas.invalidateAnchors();
    this.toasts.show(skipped.length
      ? `Pasted ${ids.length}; ${skipped.length} unknown module${skipped.length > 1 ? 's' : ''} skipped.`
      : `Pasted ${ids.length} module${ids.length > 1 ? 's' : ''}.`);
  }

  private newPatch(): void {
    this.loadPatch({
      format: 'wirescape.native.patch', version: 2, modules: [], connections: [],
      ui: { positions: {}, labels: {}, view: { x: 0, y: 0, zoom: 1 } },
    }, 'Untitled');
    this.toasts.show('New patch.');
  }

  private exportPatch(): void {
    const json = JSON.stringify(this.doc.toJSON(), null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = el('a') as HTMLAnchorElement;
    a.href = url;
    a.download = `${this.patchName.replace(/[^\w\-]+/g, '-').toLowerCase() || 'patch'}.wirescape.json`;
    a.click();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
    this.doc.markClean();
    this.refreshDirty();
    this.toasts.show('Patch saved.');
  }

  private async importFile(): Promise<void> {
    const f = this.fileInput.files?.[0];
    if (!f) return;
    try {
      const patch = JSON.parse(await f.text()) as PatchV2;
      if (!Array.isArray(patch.modules) || !Array.isArray(patch.connections)) {
        throw new Error('That file is not a Wirescape patch.');
      }
      this.loadPatch(patch, f.name.replace(/\.(wirescape\.)?json$/i, ''));
      const unknown = [...this.doc.modules.values()].filter(m => !this.registry.has(m.module));
      this.toasts.show(unknown.length
        ? `Patch opened. ${unknown.length} unknown module${unknown.length > 1 ? 's' : ''} kept as-is.`
        : 'Patch opened.');
    } catch (err) {
      this.toasts.show(err instanceof Error ? err.message : 'Could not open that file.', 'error');
    }
    this.fileInput.value = '';
  }

  private refreshDirty(): void {
    this.dirtyDot.classList.toggle('on', this.doc.dirty);
  }

  // ---- edits through the engine -----------------------------------------

  /**
   * The UI draws a change at once, sends it to the engine, and rolls back
   * with a message if the engine refuses.
   */
  private commitParam(moduleId: number, paramId: string, value: number, commit: boolean): void {
    const before = this.doc.param(moduleId, paramId);
    this.doc.setParam(moduleId, paramId, value);
    try {
      this.engine.setParam(moduleId, paramId, value);
      this.trackRollback(() => {
        const m = this.doc.modules.get(moduleId);
        if (before === undefined && m?.parameters) delete m.parameters[paramId];
        else if (before !== undefined) this.doc.setParam(moduleId, paramId, before);
        this.canvas.cardFor(moduleId)?.panel?.refreshParam(paramId);
      });
    } catch (err) {
      if (before === undefined) {
        const m = this.doc.modules.get(moduleId);
        if (m?.parameters) delete m.parameters[paramId];
      } else {
        this.doc.setParam(moduleId, paramId, before);
      }
      this.canvas.cardFor(moduleId)?.panel?.refreshParam(paramId);
      this.toasts.show(err instanceof EngineRefusal ? err.message : 'The engine refused that change.', 'error');
    }
    if (commit && paramId === 'bpm' && this.lastStatus) this.statusBar.update(this.lastStatus);
  }

  private setMidiEndpoint(moduleId: number, endpoint: string, systemName?: string): void {
    const previous = this.doc.toJSON();
    this.doc.setModuleEndpoint(moduleId, endpoint, systemName);
    void this.engine.loadGraph(this.doc.toJSON()).catch(err => {
      this.doc.replace(previous);
      void this.engine.loadGraph(previous);
      this.toasts.show(err instanceof Error ? err.message : 'MIDI endpoint change was refused.', 'error');
    });
  }

  private commitModAmount(c: Connection, amount: number, _commit: boolean): void {
    const previous = this.doc.connections.find(x => connKey(x) === connKey(c))?.amount;
    this.doc.setModAmount(c, amount);
    this.engine.setModAmount(c, amount);
    this.trackRollback(() => this.doc.setModAmount(c, previous ?? 0));
  }

  private describeSource(c: Connection): string {
    const m = this.doc.modules.get(c.from.module);
    const d = m && this.registry.get(m.module);
    const label = this.doc.label(c.from.module) ?? d?.name ?? m?.module ?? `#${c.from.module}`;
    const port = d ? findPort(d, 'output', c.from.port)?.name ?? c.from.port : c.from.port;
    return `${label} ${port}`;
  }

  // ---- adding modules ---------------------------------------------------

  private addModuleCentred(type: string): number | null {
    const r = this.canvas.viewport.rect();
    return this.addModuleAt(type, r.left + r.width / 2, r.top + r.height / 2, true);
  }

  private addModuleAt(type: string, clientX: number, clientY: number, centre = false): number | null {
    const d = this.registry.get(type);
    if (!d) { this.toasts.show(`Unknown module "${type}".`, 'error'); return null; }
    const world = this.canvas.viewport.toWorld(clientX, clientY);
    const params: Record<string, number> = {};
    for (const p of d.parameters) params[p.id] = p.default;

    const id = this.allocateId();
    try {
      this.engine.addModule(id, type, params);
    } catch (err) {
      this.toasts.show(err instanceof EngineRefusal ? err.message : 'The engine refused that module.', 'error');
      return null;
    }
    // Rough centring: the real size is known once the card exists.
    const x = centre ? world.x - 300 : world.x - 40;
    const y = centre ? world.y - 180 : world.y - 30;
    this.doc.addModule(type, params, x, y, id);
    this.trackRollback(() => this.doc.removeModule(id));
    if (centre) {
      const card = this.canvas.cardFor(id);
      if (card) this.doc.setPosition(id, world.x - card.width / 2, world.y - card.height / 2);
    }
    this.canvas.selectModule(id, false);
    this.canvas.invalidateAnchors();
    this.root.classList.remove('library-open');
    this.toasts.show(`${d.name} added.`);
    return id;
  }

  private attachDropTarget(): void {
    const vp = this.canvas.root;
    vp.addEventListener('dragover', e => {
      if (e.dataTransfer?.types.includes('application/x-wirescape-module')) {
        e.preventDefault();
        e.dataTransfer.dropEffect = 'copy';
        vp.classList.add('drop-target');
      }
    });
    vp.addEventListener('dragleave', () => vp.classList.remove('drop-target'));
    vp.addEventListener('drop', e => {
      vp.classList.remove('drop-target');
      const type = e.dataTransfer?.getData('application/x-wirescape-module');
      if (!type) return;
      e.preventDefault();
      this.addModuleAt(type, e.clientX, e.clientY);
    });
  }

  private openQuickAdd(
    world: { x: number; y: number },
    need: { direction: PortDirection; kind: string } | null,
    then?: (id: number) => void,
  ): void {
    const client = this.canvas.viewport.toClient(world.x, world.y);
    this.quick.open({
      at: client,
      need,
      choose: type => {
        const id = this.addModuleAt(type, client.x, client.y);
        if (id !== null && then) {
          requestAnimationFrame(() => { then(id); this.canvas.invalidateAnchors(); });
        }
      },
      cancel: () => { /* nothing was added */ },
    });
  }

  private async confirmStereo(message: string): Promise<boolean> {
    return new Promise(resolve => {
      const bar = el('div', 'confirm-bar');
      bar.dataset.testid = 'stereo-confirm';
      bar.append(el('span', '', message));
      const yes = el('button', 'action strong', 'CONNECT BOTH') as HTMLButtonElement;
      yes.dataset.testid = 'stereo-yes';
      const no = el('button', 'action', 'JUST THIS ONE') as HTMLButtonElement;
      no.dataset.testid = 'stereo-no';
      const done = (v: boolean) => { bar.remove(); clearTimeout(timer); resolve(v); };
      yes.addEventListener('click', () => done(true));
      no.addEventListener('click', () => done(false));
      bar.append(no, yes);
      this.root.append(bar);
      const timer = window.setTimeout(() => done(false), 6000);
    });
  }

  // ---- selection & editing ----------------------------------------------

  private onSelectionChange(s: Selection): void {
    this.root.dataset.selection = s.kind;
  }

  private deleteSelection(): void {
    const s = this.canvas.currentSelection;
    if (s.kind === 'cable') {
      this.doc.disconnect(s.c);
      try { this.engine.disconnect(s.c); this.trackRollback(() => this.doc.connect(s.c)); }
      catch { /* ignore */ }
      this.canvas.select({ kind: 'none' });
      this.toasts.show('Cable removed.');
    } else if (s.kind === 'modules') {
      for (const id of s.ids) {
        const m = this.doc.modules.get(id);
        if (!m) continue;
        const original = structuredClone(m);
        const pos = this.doc.position(id), label = this.doc.label(id);
        const cables = this.doc.connections.filter(c => c.from.module === id || c.to.module === id).map(c => structuredClone(c));
        this.doc.removeModule(id);
        this.engine.removeModule(id);
        this.trackRollback(() => {
          this.doc.addModule(original.module, original.parameters ?? {}, pos[0], pos[1], id);
          Object.assign(this.doc.modules.get(id)!, original);
          if (label) this.doc.setLabel(id, label);
          for (const c of cables) this.doc.connect(c);
        });
      }
      this.canvas.select({ kind: 'none' });
      this.toasts.show(`${s.ids.length} module${s.ids.length > 1 ? 's' : ''} deleted.`);
    }
  }

  private renameModule(id: number): void {
    const current = this.doc.label(id) ?? '';
    const next = window.prompt('Instance label (empty clears it)', current);
    if (next === null) return;
    this.doc.setLabel(id, next);
  }

  private renamePatch(): void {
    const next = window.prompt('Patch name', this.patchName);
    if (next === null) return;
    this.patchName = next.trim() || 'Untitled';
    this.nameEl.textContent = this.patchName;
  }

  private toggleClock(running: boolean): void {
    for (const [id, m] of this.doc.modules) {
      if (m.module !== 'essentials.clock') continue;
      this.commitParam(id, 'run', running ? 1 : 0, true);
      this.canvas.cardFor(id)?.panel?.refreshParam('run');
    }
  }

  // ---- shortcuts --------------------------------------------------------

  private attachShortcuts(): void {
    window.addEventListener('keydown', e => {
      if (isTyping() || this.settings.isOpen || this.shortcuts.isOpen) return;
      const mod = e.ctrlKey || e.metaKey;
      if (mod && e.key.toLowerCase() === 's') { e.preventDefault(); void this.savePatchFile(); return; }
      if (mod && e.key.toLowerCase() === 'o') { e.preventDefault(); void this.openPatchFile(); return; }
      if (mod && e.key.toLowerCase() === 'a') { e.preventDefault(); this.canvas.selectAll(); return; }
      if (mod && e.key.toLowerCase() === 'z') { e.preventDefault(); if (e.shiftKey) this.redo(); else this.undo(); return; }
      if (mod && e.key.toLowerCase() === 'y') { e.preventDefault(); this.redo(); return; }
      if (mod && e.key.toLowerCase() === 'c') { e.preventDefault(); this.copy(); return; }
      if (mod && e.key.toLowerCase() === 'x') { e.preventDefault(); this.cut(); return; }
      if (mod && e.key.toLowerCase() === 'v') { e.preventDefault(); this.paste(); return; }
      if (mod && e.key.toLowerCase() === 'd') { e.preventDefault(); this.duplicate(); return; }
      if (mod && e.key.toLowerCase() === 'f') { e.preventDefault(); this.library.focusSearch(); return; }
      if (e.key === 'Delete' || e.key === 'Backspace') { e.preventDefault(); this.deleteSelection(); return; }
      if (e.key === 'Escape') { this.canvas.select({ kind: 'none' }); this.root.classList.remove('library-open'); return; }
      if (e.key === '?') { e.preventDefault(); this.shortcuts.open('Keyboard shortcuts'); return; }
      if (e.key === '`' || (e.shiftKey && e.key.toLowerCase() === 'd')) { e.preventDefault(); this.devPanel.toggle(); return; }
      if (e.key === '0' && mod) { e.preventDefault(); this.canvas.viewport.zoomTo(1); return; }
      if (e.key === '1' && mod) { e.preventDefault(); this.canvas.fit(); return; }
    });
  }

  private buildShortcutSheet(): void {
    const rows: [string, string][] = [
      ['Drag a module header', 'Move it (moves the whole selection)'],
      ['Drag an output to an input', 'Patch a cable'],
      ['Drag an output onto a knob', 'Modulate that parameter'],
      ['Shift-drop on an input', 'Add alongside instead of replacing'],
      ['Drag from a connected input', 'Grab that cable’s end'],
      ['Drag empty canvas', 'Pan'],
      ['Shift-drag empty canvas', 'Box-select'],
      ['Shift-click a module', 'Add to the selection'],
      ['Wheel / pinch', 'Zoom on the pointer'],
      ['Double-click canvas', 'Quick-add search'],
      ['Double-click a knob', 'Reset to default'],
      ['Click a knob readout', 'Type a value'],
      ['A–L, W, E, T, Y, U, O, P', 'Play notes'],
      ['Z / X', 'Octave down / up'],
      ['Delete', 'Remove the selection'],
      ['Ctrl/Cmd + Z', 'Undo'],
      ['Ctrl/Cmd + Shift + Z, Ctrl + Y', 'Redo'],
      ['Ctrl/Cmd + C / X / V', 'Copy / cut / paste modules (also between patches)'],
      ['Ctrl/Cmd + D', 'Duplicate the selection'],
      ['Right-click an output', 'Map it to a parameter'],
      ['Ctrl/Cmd + A', 'Select all'],
      ['Ctrl/Cmd + S', 'Save the patch'],
      ['Ctrl/Cmd + O', 'Open a patch'],
      ['Ctrl/Cmd + F', 'Search the library'],
      ['Ctrl/Cmd + 0 / 1', 'Zoom 100% / fit'],
      ['?', 'This sheet'],
      ['`', 'Debug panel'],
    ];
    const list = el('div', 'shortcut-list');
    for (const [k, v] of rows) {
      const row = el('div', 'shortcut-row');
      row.append(el('kbd', '', k), el('span', '', v));
      list.append(row);
    }
    this.shortcuts.body.append(list);
  }

  // ---- settings ---------------------------------------------------------

  private openSettings(): void {
    const engine: EngineClient = this.engine;
    this.settings.body.replaceChildren();
    const b = this.settings.body;
    let stopMonitor: (() => void) | undefined;

    b.append(el('h3', 'settings-group', 'Audio'));
    const audioFields = el('div', 'settings-audio');
    b.append(audioFields);
    void engine.listAudioDevices().then(devices => {
      // The dialog may have been rebuilt while device enumeration was pending.
      if (!b.contains(audioFields)) return;
      if (!devices.length) {
        const empty = el('div', 'muted', 'No audio devices available.');
        empty.dataset.testid = 'audio-no-devices';
        audioFields.append(empty);
        return;
      }
      const currentId = this.lastStatus?.audio.deviceId ?? devices[0]!.id;
      const rates = select([], '', v => engine.setSampleRate?.(Number(v)));
      rates.dataset.testid = 'audio-rate';
      const refreshRates = (deviceId: string) => {
        const dev = devices.find(d => d.id === deviceId);
        rates.replaceChildren(...(dev?.sampleRates ?? [48000]).map(n => {
          const option = document.createElement('option');
          option.value = String(n); option.textContent = `${(n / 1000).toFixed(1)} kHz`;
          return option;
        }));
        const actual = String(this.lastStatus?.audio.sampleRate ?? 48000);
        rates.value = actual;
        // Do not imply an unsupported rate is active; ask the engine to pick it.
        if (!rates.value && rates.options.length) {
          rates.selectedIndex = 0;
          engine.setSampleRate?.(Number(rates.value));
        }
      };
      const sel = select(devices.map(d => ({ value: d.id, label: d.name })), currentId, v => {
        engine.selectAudioDevice(v);
        if (engine.setSampleRate) refreshRates(v);
      });
      sel.dataset.testid = 'audio-device';
      audioFields.append(field('Output device', sel));
      if (engine.setSampleRate) {
        refreshRates(currentId);
        audioFields.append(field('Sample rate', rates));
      }
      if (engine.setBufferFrames) {
        const buffer = select(
          [64, 128, 256, 512, 1024].map(n => ({ value: String(n), label: `${n} frames` })),
          String(this.lastStatus?.audio.bufferFrames ?? 256),
          v => engine.setBufferFrames?.(Number(v)),
        );
        buffer.dataset.testid = 'audio-buffer';
        audioFields.append(field('Buffer size', buffer, 'Lower is tighter, and more likely to drop out'));
      }
    }).catch(() => {
      if (b.contains(audioFields)) audioFields.textContent = 'Audio devices could not be listed.';
    });

    b.append(el('h3', 'settings-group', 'MIDI'));
    const midiFields = el('div', 'settings-midi');
    b.append(midiFields);
    const computerKeys = switchButton(this.prefs.computerKeyboardPlays,
      on => this.setPref({ computerKeyboardPlays: on }));
    computerKeys.dataset.testid = 'computer-keyboard-plays';
    b.append(field('Computer keyboard plays', computerKeys,
      'A/W/S… plays notes in patches with a Keyboard module. MIDI inputs use their own device switches.'));
    void engine.listMidiDevices().then(devices => {
      if (!b.contains(midiFields)) return;
      if (!devices.length) {
        const empty = el('div', 'muted', 'No MIDI devices available.');
        empty.dataset.testid = 'midi-no-devices';
        midiFields.append(empty);
        return;
      }
      const selected = select(devices.map(d => ({ value: d.id, label: d.name })),
        devices[0]!.id, () => refreshToggle());
      selected.dataset.testid = 'midi-device';
      const toggleSlot = el('div', 'settings-midi-toggle');
      const refreshToggle = () => {
        const d = devices.find(device => device.id === selected.value)!;
        const enabled = switchButton(d.enabled !== false, on => {
          engine.setMidiEnabled(d.id, on);
          d.enabled = on;
        });
        enabled.dataset.testid = 'midi-enabled';
        toggleSlot.replaceChildren(enabled);
      };
      midiFields.append(field('Input device', selected), field('Enabled', toggleSlot));
      refreshToggle();
    }).catch(() => {
      if (b.contains(midiFields)) midiFields.textContent = 'MIDI devices could not be listed.';
    });

    const osc = this.lastStatus?.osc;
    if (osc && engine.setOsc) {
      b.append(el('h3', 'settings-group', 'OSC'));
      const on = switchButton(osc.enabled, v => engine.setOsc!({ enabled: v }));
      on.dataset.testid = 'osc-enabled';
      b.append(field('OSC server', on, 'Lets other apps change parameters by message'));
      const port = el('input', 'input-text') as HTMLInputElement;
      port.type = 'number'; port.min = '1024'; port.max = '65535'; port.value = String(osc.port);
      port.dataset.testid = 'osc-port';
      port.addEventListener('change', () => {
        try { engine.setOsc!({ port: Number(port.value) }); }
        catch (err) {
          this.toasts.show(err instanceof EngineRefusal ? err.message : 'That port was refused.', 'error');
          port.value = String(this.lastStatus?.osc?.port ?? 9000);
        }
      });
      b.append(field('Port', port));
      const net = switchButton(osc.allowNetwork, v => engine.setOsc!({ allowNetwork: v }));
      net.dataset.testid = 'osc-network';
      b.append(field('Allow network', net, 'Off: this computer only. On: any device on your network can control the patch'));
      const mon = el('div', 'osc-monitor');
      mon.dataset.testid = 'osc-monitor';
      const paint = () => {
        const r = this.lastStatus?.osc?.recent ?? [];
        mon.replaceChildren(...(r.length
          ? r.map(m => el('div', 'osc-line', `${m.address}  ${m.args.join(' ')}`))
          : [el('div', 'osc-line muted', this.lastStatus?.osc?.enabled ? 'Waiting for messages…' : 'OSC is off')]));
      };
      paint();
      stopMonitor = engine.onStatus(paint);
      b.append(field('Recent messages', mon));
    }

    b.append(el('h3', 'settings-group', 'Interface'));
    b.append(field('Motion', switchButton(this.prefs.motion, v => this.setPref({ motion: v })),
      'Pauses signal-driven animation'));
    b.append(field('Performance profile', select(
      [{ value: 'normal', label: 'Normal' }, { value: 'pi', label: 'Raspberry Pi' }],
      this.prefs.profile, v => this.setPref({ profile: v as Preferences['profile'] }),
    ), 'Pi lowers telemetry drawing to 30 Hz and drops shadows'));
    const auto = switchButton(this.prefs.autoProfile, v => this.setPref({ autoProfile: v }));
    auto.dataset.testid = 'auto-profile';
    b.append(field('Switch to Pi when slow', auto, 'Turns the Pi profile on if frames stay under 40 fps for 5 s'));
    b.append(field('Hit areas', select(
      [{ value: 'mouse', label: 'Mouse and keyboard' }, { value: 'touch', label: 'Touch' }],
      this.prefs.hit, v => this.setPref({ hit: v as Preferences['hit'] }),
    ), 'Touch enlarges ports and knobs'));
    b.append(field('UI scale', select(
      [0.9, 1, 1.1, 1.25].map(n => ({ value: String(n), label: `${Math.round(n * 100)}%` })),
      String(this.prefs.uiScale), v => this.setPref({ uiScale: Number(v) }),
    )));
    b.append(field('Theme', select(
      [{ value: 'light', label: 'Light' }, { value: 'dark', label: 'Dark (unfinished)' }],
      this.prefs.theme, v => this.setPref({ theme: v as Preferences['theme'] }),
    ), 'Dark is wired but not designed yet'));
    b.append(field('Port labels', switchButton(this.prefs.showPortLabels, v => this.setPref({ showPortLabels: v }))));

    b.append(el('h3', 'settings-group', 'About'));
    const about = el('div', 'about');
    const upd = el('button', 'action', 'Check for updates') as HTMLButtonElement;
    upd.type = 'button';
    upd.dataset.testid = 'check-updates';
    const updOut = el('div', 'muted');
    updOut.dataset.testid = 'update-status';
    upd.addEventListener('click', () => this.checkForUpdates(upd, updOut));
    about.append(
      el('div', '', `Wirescape Studio prototype · ${__APP_VERSION__}`),
      el('div', 'muted', `Build ${__BUILD__} · ${navigator.hardwareConcurrency ?? '?'} cores · profile ${this.prefs.profile}`),
      el('div', 'muted', this.engine instanceof RealEngine
        ? 'Native engine via the isolated preload bridge.' : 'Simulated engine. No audio or hardware MIDI.'),
      upd, updOut,
    );
    b.append(about);

    const debug = el('button', 'action', 'Open the debug panel') as HTMLButtonElement;
    debug.type = 'button';
    debug.dataset.testid = 'open-debug';
    debug.addEventListener('click', () => { this.settings.close(); this.devPanel.toggle(true); });
    b.append(debug);

    this.settings.open('Settings', () => stopMonitor?.());
  }

  private setPref(patch: Partial<Preferences>): void {
    const hitChanged = patch.hit !== undefined && patch.hit !== this.prefs.hit;
    this.prefs = { ...this.prefs, ...patch };
    applyPrefs(this.prefs);
    savePrefs(this.prefs);
    this.keyboard.setEnabled(this.prefs.computerKeyboardPlays);
    CanvasPane.setZoom(this.canvas.viewport.view.s, true);
    this.ticker.throttleHz = this.prefs.profile === 'pi' ? 30 : 0;
    // Hit areas change the geometry the layout is derived from, so every
    // card has to be rebuilt rather than just re-measured.
    if (hitChanged) this.canvas.rebuild();
    requestAnimationFrame(() => this.canvas.invalidateAnchors());
  }


  /** Native dialogs belong to Electron main; browser-only use keeps file input/export. */
  private async openPatchFile(): Promise<void> {
    if (!window.wirescape) { this.fileInput.click(); return; }
    try {
      const file = await window.wirescape.openPatch();
      if (!file) return;
      const parsed = JSON.parse(file.text) as PatchV2;
      if (!Array.isArray(parsed.modules) || !Array.isArray(parsed.connections)) throw new Error('Invalid patch.');
      await this.loadPatch(parsed, file.name);
    } catch (e) {
      this.toasts.show(e instanceof Error ? e.message : 'Could not open patch.', 'error');
    }
  }

  private async savePatchFile(saveAs = false): Promise<void> {
    if (!window.wirescape) { this.exportPatch(); return; }
    try {
      const result = await window.wirescape.savePatch(JSON.stringify(this.doc.toJSON(), null, 2), { saveAs });
      if (!result) return;
      this.patchName = result.name;
      this.nameEl.textContent = result.name;
      this.doc.markClean();
      this.refreshDirty();
      this.toasts.show('Patch saved.');
    } catch (e) {
      this.toasts.show(e instanceof Error ? e.message : 'Could not save patch.', 'error');
    }
  }

  /** Never recycle numeric IDs within a loaded document on the real sidecar. */
  private allocateId(): number {
    if (!(this.engine instanceof RealEngine)) return this.doc.nextId();
    this.maxModuleId = Math.max(this.maxModuleId, ...this.doc.modules.keys(), 0) + 1;
    return this.maxModuleId;
  }

  private trackRollback(undo: () => void): void {
    if (this.engine instanceof RealEngine) this.engine.registerRollback(undo);
  }

  // ---- test hooks -------------------------------------------------------

  testApi() {
    return {
      state: () => this.doc.toJSON(),
      autosaveNow: () => {
        localStorage.setItem(AUTOSAVE_KEY, JSON.stringify({ name: this.patchName, patch: this.doc.toJSON() }));
      },
      clearAutosave: () => localStorage.removeItem(AUTOSAVE_KEY),
      patchName: () => this.patchName,
      load: (p: PatchV2, name?: string) => this.loadPatch(p, name),
      save: () => this.doc.toJSON(),
      doc: this.doc,
      engine: this.engine,
      commitParam: (m: number, p: string, v: number, commit = true) => this.commitParam(m, p, v, commit),
      ticker: this.ticker,
      canvas: this.canvas,
      registry: this.registry,
      starters: STARTERS,
      setPref: (p: Partial<Preferences>) => this.setPref(p),
      zoomTo: (s: number) => this.canvas.viewport.zoomTo(s),
      fit: () => this.canvas.fit(),
      addModule: (type: string) => this.addModuleCentred(type),
      connect: (c: Connection) => {
        this.doc.connect(c);
        try { this.engine.connect(c); this.trackRollback(() => this.doc.disconnect(c)); }
        catch (e) { this.doc.disconnect(c); throw e; }
      },
      deleteSelection: () => this.deleteSelection(),
      undo: () => this.undo(),
      redo: () => this.redo(),
      flushHistory: () => this.history.flush(),
      historyDepth: () => this.history.depth,
      copy: () => this.copy(),
      paste: () => this.paste(),
      duplicate: () => this.duplicate(),
      select: (s: Selection) => this.canvas.select(s),
      noteOn: (n: number, v = 0.8) => this.engine.noteOn(n, v),
      noteOff: (n: number) => this.engine.noteOff(n),
      effective: (moduleId: number, paramId: string) => {
        const d = this.registry.get(this.doc.modules.get(moduleId)?.module ?? '');
        const p = d && findParam(d, paramId);
        return p ? this.doc.param(moduleId, paramId) ?? p.default : undefined;
      },
      fps: () => this.ticker.fps,
    };
  }
}

declare const __APP_VERSION__: string;
declare const __BUILD__: string;

