// SPDX-License-Identifier: GPL-3.0-only
import type { Connection, Endpoint } from '../patch/types';
import { connKey } from '../patch/types';
import type { PatchDoc, PatchEvent } from '../patch/PatchDoc';
import type { ModuleRegistry } from '../modules/registry';
import { findParam, findPort, inputs, outputs } from '../modules/registry';
import type { PanelOverrides } from '../panels/layouts';
import type { PanelHost } from '../panels/PanelBuilder';
import type { Ticker } from '../runtime/Ticker';
import type { PortDirection } from '../modules/types';
import { Viewport, clampZoom, boxesOverlap, type Box } from './Viewport';
import { CanvasPane } from '../panels/widgets/canvasBase';
import { CableLayer, type Point, type Resolved } from './CableLayer';
import { ModuleCard } from './ModuleCard';
import { el, motionOff } from '../app/dom';
import type { OscStatus } from '../engine/EngineClient';

export type Selection =
  | { kind: 'none' }
  | { kind: 'modules'; ids: number[] }
  | { kind: 'cable'; c: Connection };

export interface CanvasServices extends PanelHost {
  registry: ModuleRegistry;
  overrides: PanelOverrides;
  doc: PatchDoc;
  canEdit(): boolean;
  ticker: Ticker;
  toast(message: string, tone?: 'info' | 'error'): void;
  /** Offer a filtered quick-add when a cable is dropped on empty canvas. */
  quickAdd(world: Point, need: { direction: PortDirection; kind: string } | null,
           then?: (moduleId: number) => void): void;
  /** Ask the user about connecting a stereo pair. Resolves to the decision. */
  confirmStereo(message: string): Promise<boolean>;
  onSelectionChange(s: Selection): void;
  rename(moduleId: number): void;
  onEditOsc(moduleId: number): void;
  oscStatus(): OscStatus | undefined;
  /** Send a connect to the engine; throws EngineRefusal if it is refused. */
  engineConnect(c: Connection): void;
  engineDisconnect(c: Connection): void;
  /** MIDI Learn (#287). Absent or `available() === false` when the engine cannot observe CCs. */
  midiLearn?: {
    available(): boolean;
    begin(moduleId: number, paramId: string): void;
    mapped(moduleId: number, paramId: string): boolean;
    forget(moduleId: number, paramId: string): void;
  };
}

type Pending = {
  /** The fixed end of the cable being dragged. */
  origin: { module: number; port: string; direction: PortDirection; kind: string; hint: string };
  cursor: Point;
  /** Set when the drag started by pulling an existing cable off an input. */
  detached?: Connection;
};

export class PatchCanvas {
  readonly root: HTMLElement;
  readonly viewport: Viewport;
  readonly cables: CableLayer;
  private world: HTMLElement;
  private cardLayer: HTMLElement;
  private cards = new Map<number, ModuleCard>();
  private selection: Selection = { kind: 'none' };
  private pending: Pending | null = null;
  private gesture: 'none' | 'pan' | 'drag' | 'patch' | 'box' | 'pinch' = 'none';
  private modAnchors = new Map<string, Point>();
  private marquee: HTMLElement;
  private emptyState: HTMLElement;
  private zCounter = 10;
  private cableTools!: HTMLElement;
  private cableAmount!: HTMLElement;

  constructor(private readonly s: CanvasServices) {
    this.root = el('div', 'viewport');
    this.root.dataset.testid = 'viewport';
    this.root.setAttribute('aria-label', 'Patch canvas');

    this.world = el('div', 'world');
    this.cardLayer = el('div', 'card-layer');
    this.cables = new CableLayer(
      c => this.resolveCable(c),
      c => this.select(c ? { kind: 'cable', c } : { kind: 'none' }),
      (c, e) => this.beginGrabEnd(c, e),
      c => this.describeConnection(c),
    );
    this.cableTools = this.buildCableTools();
    this.world.append(this.cables.root, this.cardLayer, this.cableTools);

    this.marquee = el('div', 'marquee');
    this.marquee.style.display = 'none';
    this.emptyState = this.buildEmptyState();

    this.root.append(this.world, this.marquee, this.emptyState);
    this.viewport = new Viewport(this.root, this.world);

    this.attachViewportGestures();
    this.s.doc.on(e => this.onDocEvent(e));
    this.viewport.on(v => { CanvasPane.setZoom(v.s); this.updateVisibility(); this.syncZoomLabel(); });
    this.rebuild();
  }

  // ---- building ---------------------------------------------------------

  rebuild(): void {
    for (const c of this.cards.values()) { this.s.ticker.dropModule(c.root.dataset.id ? Number(c.root.dataset.id) : -1, true); c.destroy(); }
    this.cards.clear();
    this.cables.clear();
    this.modAnchors.clear();
    for (const id of this.s.doc.modules.keys()) this.addCard(id, false);
    // One layout pass for all cards (Pi rule 8), then route.
    for (const c of this.cards.values()) c.measure();
    this.syncCables();
    this.refreshEmptyState();
    this.updateVisibility();
  }

  private addCard(id: number, single = true): void {
    const m = this.s.doc.modules.get(id);
    if (!m) return;
    const d = this.s.registry.get(m.module) ?? null;
    const card = new ModuleCard({
      host: this.s,
      moduleId: id,
      descriptor: d,
      override: d ? this.s.overrides.get(d.id) : undefined,
      savedType: m.module,
      onHeaderPointerDown: e => this.beginDrag(e, id),
      onRenameRequest: () => this.s.rename(id),
      onEditOsc: () => this.s.onEditOsc(id),
      oscStatus: () => this.s.oscStatus(),
      wirePort: p => {
        p.addEventListener('pointerdown', e => this.beginPatch(e as PointerEvent, p));
        p.addEventListener('contextmenu', e => this.portMenu(e as MouseEvent, p));
      },
      onPortsChanged: () => { this.clearModAnchors(id); this.syncCables(); },
    });
    const [x, y] = this.s.doc.position(id);
    card.root.style.left = `${x}px`;
    card.root.style.top = `${y}px`;
    card.root.style.zIndex = String(++this.zCounter);
    card.root.addEventListener('pointerdown', e => {
      if ((e.target as HTMLElement).closest('.port,.knob,.seg,.toggle,.control-readout,.readout-input,.step-gate,.step-pitch,.module-header,.control-number,.control-fader,.input-select,.ib,.ibox,.filter-pane,.groove-pane'))
        return;
      this.selectModule(id, e.shiftKey);
    });
    // Modulatable knobs act as cable targets, and as mapping targets / sources.
    for (const k of card.root.querySelectorAll<HTMLElement>('.knob-unit[data-modulatable="true"]')) {
      k.addEventListener('pointerup', () => { if (this.pending) this.dropOnKnob(id, k.dataset.param!); });
      k.addEventListener('contextmenu', e => this.knobMenu(e, id, k.dataset.param!));
    }
    this.cardLayer.append(card.root);
    this.cards.set(id, card);
    if (single) {
      card.measure();
      // Add: drops in (DESIGN §8, 240 ms, --bounce, from -10 px / 0.96).
      if (!motionOff()) {
        // Bounce only the drop; scale eases without overshoot so a full-width phone card never
        // renders wider than its box (scale > 1) while the animation settles.
        card.root.animate([{ opacity: 0, transform: 'translateY(-10px)' }, { opacity: 1, transform: 'none' }],
          { duration: 240, easing: BOUNCE });
        card.root.animate([{ scale: '.96' }, { scale: '1' }], { duration: 240, easing: 'ease-out' });
      }
    }
    this.applyPolyBadge(id);
  }

  updateOscStatus(status?: OscStatus): void {
    for (const card of this.cards.values()) card.setOscStatus(status);
  }

  private applyPolyBadge(id: number): void {
    const card = this.cards.get(id);
    const m = this.s.doc.modules.get(id);
    if (!card || !m) return;
    const d = this.s.registry.get(m.module);
    if (!d) return;
    const voiceParam = d.parameters.find(p => p.id === 'voices' || p.id === 'voice_count');
    const polyPorts = d.ports.some(p => p.poly === 'poly');
    const n = voiceParam ? (m.parameters?.[voiceParam.id] ?? voiceParam.default) : null;
    card.setPolyVoices(voiceParam ? Math.round(n!) : (polyPorts ? null : null));
  }

  private buildEmptyState(): HTMLElement {
    const e = el('div', 'empty-state');
    e.dataset.testid = 'empty-state';
    e.append(
      el('div', 'empty-title', 'Nothing patched yet.'),
      el('div', 'empty-sub', 'Add a module from the library, double-click the canvas to search, or open a starter patch.'),
    );
    return e;
  }

  private refreshEmptyState(): void {
    this.emptyState.style.display = this.s.doc.modules.size ? 'none' : '';
  }

  // ---- document events --------------------------------------------------

  private onDocEvent(e: PatchEvent): void {
    switch (e.t) {
      case 'reset': this.rebuild(); break;
      case 'module-added':
        this.addCard(e.id); this.syncCables(); this.refreshEmptyState(); this.updateVisibility(); break;
      case 'module-removed': {
        const c = this.cards.get(e.id);
        if (c) {
          this.cards.delete(e.id);
          // Remove: a quick fade (DESIGN §8, 120 ms).
          if (motionOff()) c.destroy();
          else {
            c.root.style.pointerEvents = 'none';
            c.root.animate([{ opacity: 1 }, { opacity: 0 }], { duration: 120, easing: 'ease-out', fill: 'forwards' })
              .onfinish = () => c.destroy();
          }
        }
        this.s.ticker.dropModule(e.id);
        this.clearModAnchors(e.id);
        this.syncCables(); this.refreshEmptyState(); break;
      }
      case 'connection-added': case 'connection-removed':
        // A parameter node appears with its first modulation and goes with its last;
        // a collapsed rail reveals a port that gets connected.
        for (const mid of new Set([e.c.from.module, e.c.to.module])) {
          const card = this.cards.get(mid);
          if (card?.syncPorts(false)) { card.measure(); this.clearModAnchors(mid); }
        }
        this.syncCables();
        this.refreshKnobsFor(e.c);
        break;
      case 'mod-amount': this.syncCables(); this.placeCableTools(); break;
      case 'config': this.cards.get(e.id)?.refreshCcLabels(); break;
      case 'param': {
        this.cards.get(e.id)?.panel?.refreshParam(e.paramId);
        this.cards.get(e.id)?.refreshMissing();
        this.applyPolyBadge(e.id);
        break;
      }
      case 'position': {
        const card = this.cards.get(e.id);
        if (card) {
          const [x, y] = this.s.doc.position(e.id);
          card.root.style.left = `${x}px`;
          card.root.style.top = `${y}px`;
        }
        this.clearModAnchors(e.id);
        this.syncCables();
        break;
      }
      case 'label': this.cards.get(e.id)?.refreshHeader(); break;
      case 'view': break;
    }
  }

  private refreshKnobsFor(c: Connection): void {
    if (c.kind !== 'modulation') return;
    this.cards.get(c.to.module)?.panel?.refreshParam(c.to.port);
  }

  private clearModAnchors(moduleId: number): void {
    for (const k of [...this.modAnchors.keys()]) {
      if (k.startsWith(`${moduleId}:`)) this.modAnchors.delete(k);
    }
  }

  syncCables(): void {
    this.cables.sync(this.s.doc.connections, this.s.doc.feedbackKeys());
    if (this.selection.kind === 'cable') {
      this.cables.select(this.selection.c, this.s.doc.connections, this.s.doc.feedbackKeys());
    }
  }

  // ---- cable geometry ---------------------------------------------------

  private cardBox(id: number): Box | null {
    const card = this.cards.get(id);
    if (!card) return null;
    const [x, y] = this.s.doc.position(id);
    return { x, y, w: card.width, h: card.height };
  }

  private endpointPoint(e: Endpoint, dir: PortDirection, kind: Connection['kind']): Point | null {
    const card = this.cards.get(e.module);
    if (!card) return null;
    const [cx, cy] = this.s.doc.position(e.module);
    if (kind === 'modulation' && dir === 'input') {
      const node = card.paramAnchor(e.port);
      if (node) return { x: cx + node.x, y: cy + node.y };
      const local = this.modAnchor(e.module, e.port);
      return local ? { x: cx + local.x, y: cy + local.y } : null;
    }
    const a = card.anchor(dir, e.port);
    return a ? { x: cx + a.x, y: cy + a.y } : null;
  }

  /** Knob centre in card-local units. Measured once, then cached. */
  private modAnchor(moduleId: number, paramId: string): Point | null {
    const k = `${moduleId}:${paramId}`;
    const hit = this.modAnchors.get(k);
    if (hit) return hit;
    const card = this.cards.get(moduleId);
    if (!card) return null;
    const knob = card.root.querySelector<HTMLElement>(`.knob-unit[data-param="${CSS.escape(paramId)}"] .dial-wrap`);
    if (!knob) return null;
    const kr = knob.getBoundingClientRect();
    const cr = card.root.getBoundingClientRect();
    const s = this.viewport.view.s || 1;
    if (!kr.width) return null;
    const p = { x: (kr.left - cr.left) / s, y: (kr.top + kr.height / 2 - cr.top) / s };
    this.modAnchors.set(k, p);
    return p;
  }

  private resolveCable(c: Connection): Resolved | null {
    const from = this.endpointPoint(c.from, 'output', c.kind);
    const to = this.endpointPoint(c.to, 'input', c.kind);
    if (!from || !to) return null;
    const srcMod = this.s.doc.modules.get(c.from.module);
    const d = srcMod && this.s.registry.get(srcMod.module);
    const port = d && findPort(d, 'output', c.from.port);
    return {
      from, to,
      style: { color: this.s.cableColor(c), kind: c.kind, feedback: false, stereo: !!port?.stereo_pair },
    };
  }

  private describeConnection(c: Connection): string {
    const name = (id: number) => {
      const m = this.s.doc.modules.get(id);
      return this.s.doc.label(id) ?? this.s.registry.get(m?.module ?? '')?.name ?? m?.module ?? `#${id}`;
    };
    const portName = (id: number, port: string, dir: PortDirection) => {
      const m = this.s.doc.modules.get(id);
      const d = m && this.s.registry.get(m.module);
      if (!d) return port;
      if (c.kind === 'modulation' && dir === 'input') return findParam(d, port)?.name ?? port;
      return findPort(d, dir, port)?.name ?? port;
    };
    const amt = c.kind === 'modulation' ? `  (${(c.amount ?? 0) >= 0 ? '+' : ''}${(c.amount ?? 0).toFixed(2)})` : '';
    return `${name(c.from.module)} ${portName(c.from.module, c.from.port, 'output')}  →  ${name(c.to.module)} ${portName(c.to.module, c.to.port, 'input')}${amt}`;
  }

  // ---- selection --------------------------------------------------------

  select(s: Selection): void {
    this.selection = s;
    for (const [id, card] of this.cards) {
      card.root.classList.toggle('selected', s.kind === 'modules' && s.ids.includes(id));
    }
    this.cables.select(s.kind === 'cable' ? s.c : null, this.s.doc.connections, this.s.doc.feedbackKeys());
    this.placeCableTools();
    this.s.onSelectionChange(s);
  }

  selectModule(id: number, additive: boolean): void {
    const current = this.selection.kind === 'modules' ? this.selection.ids : [];
    if (additive) {
      const next = current.includes(id) ? current.filter(x => x !== id) : [...current, id];
      this.select(next.length ? { kind: 'modules', ids: next } : { kind: 'none' });
    } else {
      this.select({ kind: 'modules', ids: [id] });
    }
    const card = this.cards.get(id);
    if (card) card.root.style.zIndex = String(++this.zCounter);
  }

  selectAll(): void {
    const ids = [...this.s.doc.modules.keys()];
    this.select(ids.length ? { kind: 'modules', ids } : { kind: 'none' });
  }

  get currentSelection(): Selection { return this.selection }

  // ---- module dragging --------------------------------------------------

  private beginDrag(e: PointerEvent, id: number): void {
    if (e.button !== 0) return;
    e.preventDefault(); e.stopPropagation();
    const card = this.cards.get(id);
    if (!card) return;
    if (!(this.selection.kind === 'modules' && this.selection.ids.includes(id))) {
      this.selectModule(id, e.shiftKey);
    }
    card.root.style.zIndex = String(++this.zCounter);
    const ids = this.selection.kind === 'modules' ? this.selection.ids : [id];
    const start = new Map(ids.map(i => [i, this.s.doc.position(i)] as const));
    const sx = e.clientX, sy = e.clientY;
    this.gesture = 'drag';
    this.cables.springy = true;
    card.root.setPointerCapture(e.pointerId);

    const move = (ev: PointerEvent) => {
      if (ev.pointerId !== e.pointerId || this.gesture === 'pinch') return;
      const dx = (ev.clientX - sx) / this.viewport.view.s;
      const dy = (ev.clientY - sy) / this.viewport.view.s;
      for (const [i, [ox, oy]] of start) this.s.doc.setPosition(i, ox + dx, oy + dy);
    };
    const up = () => {
      card.root.removeEventListener('pointermove', move);
      card.root.removeEventListener('pointerup', up);
      card.root.removeEventListener('pointercancel', up);
      if (this.gesture !== 'pinch') this.gesture = 'none';
      this.cables.springy = false;
      this.updateVisibility();
    };
    card.root.addEventListener('pointermove', move);
    card.root.addEventListener('pointerup', up);
    card.root.addEventListener('pointercancel', up);
  }

  // ---- cable gestures ---------------------------------------------------

  private beginPatch(e: PointerEvent, portEl: HTMLElement): void {
    if (e.button !== 0) return;
    e.preventDefault(); e.stopPropagation();
    const module = Number(portEl.dataset.module);
    const port = portEl.dataset.port!;
    const direction = portEl.dataset.direction as PortDirection;
    const kind = portEl.dataset.kind!;
    const hint = portEl.dataset.hint!;

    // A parameter node: pull its modulation cable off, or start one from it.
    if (kind === 'param') {
      const mods = this.s.doc.connections.filter(c => c.kind === 'modulation' && c.to.module === module && c.to.port === port);
      if (mods.length && !e.altKey) { this.beginGrabEnd(mods.at(-1)!, e); return; }
      this.startDrag(e, { module, port, direction: 'input', kind: 'param', hint: 'cv_bi' });
      return;
    }

    // Pulling on an input that already has a cable grabs that cable's end.
    if (direction === 'input') {
      const existing = this.s.doc.cablesInto(module, port);
      if (existing.length && !e.altKey) {
        this.beginGrabEnd(existing.at(-1)!, e);
        return;
      }
    }

    this.startDrag(e, { module, port, direction, kind, hint });
  }

  /** Grab a cable's destination end to move it or pull it off. */
  private beginGrabEnd(c: Connection, e: PointerEvent): void {
    const srcMod = this.s.doc.modules.get(c.from.module);
    const d = srcMod && this.s.registry.get(srcMod.module);
    const port = d && findPort(d, 'output', c.from.port);
    this.s.doc.disconnect(c);
    try { this.s.engineDisconnect(c); } catch { /* best effort */ }
    this.startDrag(e, {
      module: c.from.module, port: c.from.port, direction: 'output',
      kind: port?.kind ?? 'signal', hint: String(port?.hint ?? 'audio'),
    }, c);
  }

  private startDrag(e: PointerEvent, origin: Pending['origin'], detached?: Connection): void {
    this.pending = { origin, cursor: this.viewport.toWorld(e.clientX, e.clientY), detached };
    this.gesture = 'patch';
    this.root.classList.add('patching');
    this.highlightTargets(origin);
    this.updateGhost();

    const target = e.currentTarget as HTMLElement;
    target.setPointerCapture?.(e.pointerId);

    const move = (ev: PointerEvent) => {
      if (!this.pending) return;
      this.pending.cursor = this.viewport.toWorld(ev.clientX, ev.clientY);
      this.updateGhost(ev);
    };
    const up = (ev: PointerEvent) => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      window.removeEventListener('pointercancel', cancel);
      void this.finishDrag(ev);
    };
    const cancel = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
      window.removeEventListener('pointercancel', cancel);
      this.cancelPatch();
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
    window.addEventListener('pointercancel', cancel);
  }

  private updateGhost(ev?: PointerEvent): void {
    if (!this.pending) return;
    const o = this.pending.origin;
    const anchor = this.endpointPoint(
      { module: o.module, port: o.port },
      o.direction, o.kind === 'param' ? 'modulation' : 'signal',
    );
    let invalid = false;
    if (ev) {
      const el2 = document.elementFromPoint(ev.clientX, ev.clientY);
      const port = el2?.closest<HTMLElement>('.port');
      const knob = el2?.closest<HTMLElement>('.knob-unit');
      if (port) invalid = !this.portCompatible(o, port);
      else if (knob) invalid = knob.dataset.modulatable !== 'true' || o.kind !== 'signal' || o.direction !== 'output';
    }
    this.cables.setGhost(
      o.direction === 'output' ? anchor : this.pending.cursor,
      o.direction === 'output' ? this.pending.cursor : anchor,
      invalid,
    );
  }

  private portCompatible(o: Pending['origin'], portEl: HTMLElement): boolean {
    const dir = portEl.dataset.direction as PortDirection;
    if (dir === o.direction) return false;
    if (Number(portEl.dataset.module) === o.module) return false;
    // A signal output can modulate a parameter node, and a parameter node takes a signal output.
    if (portEl.dataset.kind === 'param') return o.kind === 'signal' && o.direction === 'output';
    if (o.kind === 'param') return dir === 'output' && portEl.dataset.kind === 'signal';
    return portEl.dataset.kind === o.kind;
  }

  /** Compatible ports and knobs light up while a cable is in the air. */
  private highlightTargets(o: Pending['origin']): void {
    for (const card of this.cards.values()) {
      for (const p of card.allPorts()) {
        p.classList.toggle('candidate', this.portCompatible(o, p));
      }
      for (const k of card.root.querySelectorAll<HTMLElement>('.knob-unit')) {
        const ok = o.kind === 'signal' && o.direction === 'output' && k.dataset.modulatable === 'true'
          && Number(k.closest<HTMLElement>('.module')?.dataset.id) !== o.module;
        k.classList.toggle('candidate', ok);
      }
    }
    const originEl = this.cards.get(o.module)?.portEl(o.direction, o.port);
    originEl?.classList.add('armed');
  }

  private clearHighlights(): void {
    for (const card of this.cards.values()) {
      for (const p of card.allPorts()) p.classList.remove('candidate', 'armed');
      for (const k of card.root.querySelectorAll<HTMLElement>('.knob-unit')) k.classList.remove('candidate');
    }
  }

  private async finishDrag(ev: PointerEvent): Promise<void> {
    const pending = this.pending;
    if (!pending) return;
    const under = document.elementFromPoint(ev.clientX, ev.clientY);
    const portEl = under?.closest<HTMLElement>('.port');
    const knobEl = under?.closest<HTMLElement>('.knob-unit');

    if (portEl) {
      const handled = await this.dropOnPort(portEl, ev.shiftKey);
      if (handled) { this.cancelPatch(); return; }
    } else if (knobEl) {
      const card = knobEl.closest<HTMLElement>('.module');
      if (card) {
        this.dropOnKnob(Number(card.dataset.id), knobEl.dataset.param!);
        this.cancelPatch();
        return;
      }
    } else {
      // Dropped on empty canvas: offer a filtered quick-add.
      const world = this.viewport.toWorld(ev.clientX, ev.clientY);
      const o = pending.origin;
      const need = { direction: (o.direction === 'output' ? 'input' : 'output') as PortDirection, kind: o.kind };
      this.cancelPatch();
      this.s.quickAdd(world, need, newId => {
        const m = this.s.doc.modules.get(newId);
        const d = m && this.s.registry.get(m.module);
        if (!d) return;
        const list = need.direction === 'input' ? inputs(d) : outputs(d);
        const match = list.find(p => p.kind === o.kind);
        if (!match) return;
        const c: Connection = o.direction === 'output'
          ? { kind: o.kind === 'event' ? 'event' : 'signal', from: { module: o.module, port: o.port }, to: { module: newId, port: match.id } }
          : { kind: o.kind === 'event' ? 'event' : 'signal', from: { module: newId, port: match.id }, to: { module: o.module, port: o.port } };
        this.commitConnection(c, false);
      });
      return;
    }
    // A drop that resolved to nothing just restores what was detached.
    this.cancelPatch();
  }

  private async dropOnPort(portEl: HTMLElement, additive: boolean): Promise<boolean> {
    const pending = this.pending;
    if (!pending) return false;
    const o = pending.origin;
    const target = {
      module: Number(portEl.dataset.module),
      port: portEl.dataset.port!,
      direction: portEl.dataset.direction as PortDirection,
      kind: portEl.dataset.kind!,
      stereo: portEl.dataset.stereo,
    };
    // Parameter nodes: modulation, in either direction of the gesture.
    if (target.kind === 'param' || o.kind === 'param') {
      if (target.kind === 'param' && o.direction === 'output' && o.kind === 'signal') {
        this.dropOnKnob(target.module, target.port);
      } else if (o.kind === 'param' && target.direction === 'output' && target.kind === 'signal') {
        this.commitMapping({ module: target.module, port: target.port }, { module: o.module, param: o.port }, 0.3);
      } else {
        this.s.toast('A parameter node takes a signal output.', 'error');
      }
      return true;
    }
    if (target.direction === o.direction) {
      this.s.toast(o.direction === 'output' ? 'Connect an output to an input.' : 'Connect an input to an output.', 'error');
      return true;
    }
    if (target.module === o.module) {
      this.s.toast('Connect two different modules.', 'error');
      return true;
    }
    if (target.kind !== o.kind) {
      this.s.toast(o.kind === 'event'
        ? 'An event output cannot drive a signal input.'
        : 'A signal output cannot drive an event input.', 'error');
      return true;
    }

    const out = o.direction === 'output' ? o : target;
    const inp = o.direction === 'output' ? target : o;
    const kind: Connection['kind'] = o.kind === 'event' ? 'event' : 'signal';
    const base: Connection = { kind, from: { module: out.module, port: out.port }, to: { module: inp.module, port: inp.port } };

    // Stereo pairs: offer to connect L and R together as one step.
    const pair = this.stereoCounterpart(out.module, out.port, inp.module, inp.port);
    if (pair && !additive) {
      const view = this.s.doc.ui;
      const ok = await this.s.confirmStereo(
        `Connect the stereo pair? ${pair.outA}/${pair.outB} → ${pair.inA}/${pair.inB}.`);
      if (this.s.doc.ui !== view || !this.s.canEdit()) return true;
      if (ok) {
        const second: Connection = { kind, from: { module: out.module, port: pair.outB }, to: { module: inp.module, port: pair.inB } };
        // Do not create half a pair if either edge already exists.
        if (this.s.doc.has(base) || this.s.doc.has(second)) {
          this.s.toast('A pair cable already exists. Choose JUST THIS ONE to change a single side.', 'error');
          return true;
        }
        this.commitStereoPair(base, second);
        return true;
      }
    }
    this.commitConnection(base, additive);
    return true;
  }

  /**
   * If both ends belong to a stereo pair, name the partner ports. Pairing is
   * positional: the Nth stereo output maps to the Nth stereo input.
   */
  private stereoCounterpart(outMod: number, outPort: string, inMod: number, inPort: string):
    { outA: string; outB: string; inA: string; inB: string } | null {
    const od = this.descriptorOf(outMod), idd = this.descriptorOf(inMod);
    if (!od || !idd) return null;
    const outs = od.ports.filter(p => p.direction === 'output' && p.stereo_pair === 'stereo');
    const insP = idd.ports.filter(p => p.direction === 'input' && p.stereo_pair === 'stereo');
    if (outs.length !== 2 || insP.length !== 2) return null;
    const oi = outs.findIndex(p => p.id === outPort);
    const ii = insP.findIndex(p => p.id === inPort);
    if (oi < 0 || ii < 0) return null;
    const ob = outs[1 - oi]!, ib = insP[1 - ii]!;
    return { outA: outPort, outB: ob.id, inA: inPort, inB: ib.id };
  }

  private descriptorOf(moduleId: number) {
    const m = this.s.doc.modules.get(moduleId);
    return m ? this.s.registry.get(m.module) : undefined;
  }

  private dropOnKnob(moduleId: number, paramId: string): void {
    const pending = this.pending;
    if (!pending) return;
    const o = pending.origin;
    if (o.direction !== 'output' || o.kind !== 'signal') {
      this.s.toast('Only a signal output can modulate a knob.', 'error');
      return;
    }
    if (moduleId === o.module) { this.s.toast('Connect two different modules.', 'error'); return; }
    const d = this.descriptorOf(moduleId);
    const p = d && findParam(d, paramId);
    if (!p) return;
    if (!p.modulatable) { this.s.toast(`${p.name} cannot be modulated.`, 'error'); return; }
    this.commitMapping({ module: o.module, port: o.port }, { module: moduleId, param: paramId }, pending.detached?.amount ?? 0.3);
    this.pending = null;
  }

  /** Create a modulation (from a cable drop, a parameter node or the Map gesture). */
  private commitMapping(from: { module: number; port: string }, to: { module: number; param: string }, amount: number): void {
    if (from.module === to.module) { this.s.toast('Connect two different modules.', 'error'); return; }
    const d = this.descriptorOf(to.module);
    const p = d && findParam(d, to.param);
    if (!p) return;
    if (!p.modulatable) { this.s.toast(`${p.name} cannot be modulated.`, 'error'); return; }
    this.commitConnection({ kind: 'modulation', from: { ...from }, to: { module: to.module, port: to.param }, amount }, true);
  }

  // ---- mapping (owner, 3.5 review) ---------------------------------------
  //
  // Right-click a signal output → "Map to a parameter…": every modulatable
  // knob on the other modules lights up; click one to map it (a parameter
  // node appears on that module's rail and the cable lands there). Or the
  // other way round: right-click a knob → "Map from a source…", then click an
  // output. Esc, or a click on empty canvas, cancels.

  private mapping: { from?: { module: number; port: string }; to?: { module: number; param: string } } | null = null;
  private menuEl: HTMLElement | null = null;

  private portMenu(e: MouseEvent, portEl: HTMLElement): void {
    e.preventDefault(); e.stopPropagation();
    if (portEl.dataset.direction !== 'output' || portEl.dataset.kind !== 'signal') return;
    const from = { module: Number(portEl.dataset.module), port: portEl.dataset.port! };
    this.showMenu(e, [['Map to a parameter…', () => this.beginMapping({ from })]]);
  }

  private knobMenu(e: MouseEvent, moduleId: number, param: string): void {
    e.preventDefault(); e.stopPropagation();
    const items: [string, () => void][] = [['Map from a source…', () => this.beginMapping({ to: { module: moduleId, param } })]];
    const learn = this.s.midiLearn;
    const p = this.descriptorOf(moduleId) && findParam(this.descriptorOf(moduleId)!, param);
    if (learn?.available() && p?.modulatable) {
      items.push(['MIDI Learn…', () => learn.begin(moduleId, param)]);
      if (learn.mapped(moduleId, param)) items.push(['Forget MIDI mapping', () => learn.forget(moduleId, param)]);
    }
    this.showMenu(e, items);
  }

  private showMenu(e: MouseEvent, items: [string, () => void][]): void {
    this.closeMenu();
    const m = el('div', 'ctx-menu');
    m.dataset.testid = 'context-menu';
    for (const [label, act] of items) {
      const b = el('button', 'ctx-item', label);
      b.dataset.testid = `context-${label.split(' ')[0]!.toLowerCase()}`;
      b.addEventListener('pointerdown', ev => { ev.stopPropagation(); this.closeMenu(); act(); });
      m.append(b);
    }
    m.style.left = `${e.clientX}px`; m.style.top = `${e.clientY}px`;
    document.body.append(m);
    this.menuEl = m;
    setTimeout(() => window.addEventListener('pointerdown', this.closeMenuBound, { once: true }), 0);
  }
  private closeMenuBound = () => this.closeMenu();
  private closeMenu(): void { this.menuEl?.remove(); this.menuEl = null; }

  private beginMapping(m: NonNullable<PatchCanvas['mapping']>): void {
    this.endMapping();
    this.mapping = m;
    this.root.classList.add('mapping');
    for (const [mid, card] of this.cards) {
      if (m.from) {
        if (mid === m.from.module) continue;
        for (const k of card.root.querySelectorAll<HTMLElement>('.knob-unit[data-modulatable="true"]')) k.classList.add('map-target');
        for (const p of card.allPorts()) if (p.dataset.kind === 'param') p.classList.add('map-target');
      } else if (m.to) {
        if (mid === m.to.module) continue;
        for (const p of card.allPorts()) if (p.dataset.direction === 'output' && p.dataset.kind === 'signal') p.classList.add('map-target');
      }
    }
    this.s.toast(m.from ? 'Click a parameter to map it. Esc cancels.' : 'Click an output to map from. Esc cancels.');
    window.addEventListener('pointerdown', this.mapClick, true);
    window.addEventListener('keydown', this.mapKey, true);
  }

  private mapClick = (e: PointerEvent) => {
    const m = this.mapping;
    if (!m) return;
    const t = e.target as HTMLElement;
    const hit = t.closest<HTMLElement>('.map-target');
    e.preventDefault(); e.stopPropagation();
    if (hit) {
      const mid = Number(hit.closest<HTMLElement>('.module')?.dataset.id ?? hit.dataset.module);
      if (m.from) this.commitMapping(m.from, { module: mid, param: hit.dataset.param! }, 0.3);
      else if (m.to) this.commitMapping({ module: mid, port: hit.dataset.port! }, m.to, 0.3);
    }
    this.endMapping();
  };
  private mapKey = (e: KeyboardEvent) => { if (e.key === 'Escape') { e.stopPropagation(); this.endMapping(); } };

  private endMapping(): void {
    if (!this.mapping) return;
    this.mapping = null;
    this.root.classList.remove('mapping');
    for (const n of this.root.querySelectorAll('.map-target')) n.classList.remove('map-target');
    window.removeEventListener('pointerdown', this.mapClick, true);
    window.removeEventListener('keydown', this.mapKey, true);
  }

  /**
   * Replace both stereo inputs as one engine transaction. Only publish document
   * edits after both engine connects succeed: a refused second edge cannot add
   * a history entry, dirty the patch, or leave the first edge in the document.
   */
  private commitStereoPair(first: Connection, second: Connection): void {
    if (!this.s.canEdit()) return;
    const replaced = [
      ...this.s.doc.cablesInto(first.to.module, first.to.port),
      ...this.s.doc.cablesInto(second.to.module, second.to.port),
    ].filter((c, i, all) => all.findIndex(other => connKey(other) === connKey(c)) === i);
    const removed: Connection[] = [], added: Connection[] = [];
    try {
      for (const old of replaced) { this.s.engineDisconnect(old); removed.push(old); }
      for (const cable of [first, second]) { this.s.engineConnect(cable); added.push(cable); }
    } catch (err) {
      let restored = true;
      // Reverse successful engine operations. Only mirror exceptional rollback
      // failures into the document, so it never advertises a missing engine edge.
      for (const cable of added.reverse()) {
        try { this.s.engineDisconnect(cable); }
        catch { restored = false; this.s.doc.connect(cable); }
      }
      for (const old of removed) {
        try { this.s.engineConnect(old); }
        catch { restored = false; this.s.doc.disconnect(old); }
      }
      this.s.toast(restored
        ? (err instanceof Error ? err.message : 'The engine refused the stereo pair.')
        : 'The engine could not restore every previous cable; the patch reflects the remaining connections.', 'error');
      this.shake(second.to.module);
      return;
    }
    for (const old of replaced) this.s.doc.disconnect(old);
    this.s.doc.connect(first);
    this.s.doc.connect(second);
    this.s.toast('Stereo pair connected.');
    for (const c of [first, second]) {
      this.cards.get(c.from.module)?.pulsePort('output', c.from.port);
      this.cards.get(c.to.module)?.pulsePort('input', c.to.port);
    }
  }

  /**
   * Add one cable, honouring the replace-or-add rule: a signal input
   * takes one cable by default, and shift-drop adds alongside.
   */
  private commitConnection(c: Connection, additive: boolean): void {
    if (!this.s.canEdit()) return;
    if (this.s.doc.has(c)) { this.s.toast('Already connected.'); return; }
    const replaced: Connection[] = [];
    if (c.kind !== 'modulation' && !additive) {
      replaced.push(...this.s.doc.cablesInto(c.to.module, c.to.port));
    }
    for (const old of replaced) {
      this.s.doc.disconnect(old);
      try { this.s.engineDisconnect(old); } catch { /* ignore */ }
    }
    this.s.doc.connect(c);
    try {
      this.s.engineConnect(c);
      this.s.toast(c.kind === 'modulation' ? 'Modulation connected.' : 'Connected.');
      const from = this.cards.get(c.from.module), to = this.cards.get(c.to.module);
      from?.pulsePort('output', c.from.port);
      if (c.kind !== 'modulation') to?.pulsePort('input', c.to.port);
      if (to && !motionOff()) {
        to.root.animate([{ transform: 'scale(1)' }, { transform: 'scale(1.03)' }, { transform: 'scale(1)' }], { duration: 200 });
      }
    } catch (err) {
      this.s.doc.disconnect(c);
      for (const old of replaced) {
        try { this.s.engineConnect(old); this.s.doc.connect(old); }
        catch { this.s.toast('Could not restore a previous cable in the engine; it remains disconnected.', 'error'); }
      }
      this.s.toast(err instanceof Error ? err.message : 'The engine refused that cable.', 'error');
      this.shake(c.to.module);
    }
  }

  /** Refused edit: a short shake (DESIGN §8, 280 ms, ±5 px). */
  shake(moduleId: number): void {
    const card = this.cards.get(moduleId);
    if (!card || motionOff()) return;
    card.root.animate([{ transform: 'none' }, { transform: 'translateX(-5px)' }, { transform: 'translateX(4px)' },
      { transform: 'translateX(-3px)' }, { transform: 'translateX(2px)' }, { transform: 'none' }],
    { duration: 280, easing: 'ease-out' });
  }

  private cancelPatch(): void {
    const detached = this.pending?.detached;
    this.pending = null;
    this.gesture = 'none';
    this.root.classList.remove('patching');
    this.clearHighlights();
    this.cables.setGhost(null, null);
    if (detached && !this.s.doc.has(detached)) {
      // The grabbed end was released over nothing: the cable stays off.
      this.s.toast('Cable removed.');
    }
    this.syncCables();
  }

  // ---- viewport gestures ------------------------------------------------

  private attachViewportGestures(): void {
    this.root.addEventListener('wheel', e => {
      e.preventDefault();
      // ctrlKey means a trackpad pinch; both paths zoom on the pointer.
      const factor = Math.exp(-e.deltaY * (e.ctrlKey ? 0.01 : 0.001));
      this.viewport.zoomAt(this.viewport.view.s * factor, e.clientX, e.clientY);
    }, { passive: false });

    this.root.addEventListener('dblclick', e => {
      if ((e.target as HTMLElement).closest('.module')) return;
      const world = this.viewport.toWorld(e.clientX, e.clientY);
      this.s.quickAdd(world, null);
    });

    // The capture-phase listener sees both fingers even if one is on a knob,
    // port or module header. Those controls may stop bubbling or capture the
    // first pointer, so a bubbling pinch recognizer cannot work reliably.
    const touches = new Map<number, { x: number; y: number }>();
    let pinch: { distance: number; scale: number; anchor: Point } | null = null;
    this.root.addEventListener('pointerdown', e => {
      if (e.pointerType !== 'touch') return;
      touches.set(e.pointerId, { x: e.clientX, y: e.clientY });
      if (touches.size < 2) return;
      if (touches.size === 2) {
        const [a, b] = [...touches.values()];
        const cx = (a!.x + b!.x) / 2, cy = (a!.y + b!.y) / 2;
        // Drop a half-started cable, not the module or parameter beneath it.
        if (this.pending) this.cancelPatch();
        pinch = {
          distance: Math.max(1, Math.hypot(a!.x - b!.x, a!.y - b!.y)),
          scale: this.viewport.view.s,
          anchor: this.viewport.toWorld(cx, cy),
        };
        this.gesture = 'pinch';
        this.root.classList.remove('panning');
        this.cables.springy = false;
      }
      // Second/subsequent fingers must never begin a module drag or turn a knob.
      e.preventDefault();
      e.stopPropagation();
    }, true);

    this.root.addEventListener('pointermove', e => {
      if (e.pointerType !== 'touch' || !touches.has(e.pointerId)) return;
      touches.set(e.pointerId, { x: e.clientX, y: e.clientY });
      if (this.gesture !== 'pinch') return;
      // Suppress in-progress one-finger module/knob/cable handlers during pinch.
      e.preventDefault();
      e.stopPropagation();
      if (touches.size !== 2 || !pinch) return;
      const [a, b] = [...touches.values()];
      const cx = (a!.x + b!.x) / 2, cy = (a!.y + b!.y) / 2;
      const distance = Math.hypot(a!.x - b!.x, a!.y - b!.y);
      const scale = clampZoom(pinch.scale * distance / pinch.distance);
      const rect = this.viewport.rect();
      // Hold the original world point under the moving pinch midpoint:
      // panning and scaling are one transform, with no cumulative drift.
      this.viewport.setView(cx - rect.left - pinch.anchor.x * scale,
        cy - rect.top - pinch.anchor.y * scale, scale);
    }, true);

    for (const kind of ['pointerup', 'pointercancel'] as const) {
      this.root.addEventListener(kind, e => {
        if (e.pointerType !== 'touch') return;
        touches.delete(e.pointerId);
        if (touches.size === 0) {
          pinch = null;
          if (this.gesture === 'pinch') this.gesture = 'none';
        } else if (touches.size !== 2) {
          pinch = null; // No one-finger edit resumes until both lift.
        }
      }, true);
    }

    // iOS Safari also emits legacy gesture events. Prevent *canvas* gestures
    // from zooming the whole browser page (without disabling accessibility
    // zoom on the rest of the document).
    for (const kind of ['gesturestart', 'gesturechange'] as const) {
      this.root.addEventListener(kind, e => e.preventDefault(), { passive: false });
    }
    this.root.addEventListener('touchmove', e => {
      if (e.touches.length > 1) e.preventDefault();
    }, { passive: false });

    this.root.addEventListener('pointerdown', e => {
      if ((e.target as HTMLElement).closest('.module,.cable-hit,.navigator,.empty-state')) return;
      if (e.button !== 0 && e.button !== 1) return;
      if (this.pending) { this.cancelPatch(); return; }
      if (this.gesture === 'pinch') return;

      // Shift or middle button starts a box-select; a plain drag pans.
      if (e.shiftKey || e.button === 1) { this.beginBoxSelect(e); return; }

      // Delay touch deselection until a tap completes. A pinch starting on
      // blank paper should not deselect the module the user was inspecting.
      if (e.pointerType !== 'touch') this.select({ kind: 'none' });
      this.gesture = 'pan';
      this.root.classList.add('panning');
      const sx = e.clientX, sy = e.clientY;
      let moved = false;
      const start = { ...this.viewport.view };
      this.root.setPointerCapture(e.pointerId);
      const move = (ev: PointerEvent) => {
        if (ev.pointerId !== e.pointerId || this.gesture === 'pinch') return;
        if (Math.hypot(ev.clientX - sx, ev.clientY - sy) > 5) moved = true;
        this.viewport.setView(start.x + ev.clientX - sx, start.y + ev.clientY - sy, start.s);
      };
      const up = (ev: PointerEvent) => {
        if (ev.pointerId !== e.pointerId) return;
        this.root.removeEventListener('pointermove', move);
        this.root.removeEventListener('pointerup', up);
        this.root.removeEventListener('pointercancel', up);
        if (this.gesture !== 'pinch') {
          if (e.pointerType === 'touch' && !moved && ev.type === 'pointerup') this.select({ kind: 'none' });
          this.gesture = 'none';
        }
        this.root.classList.remove('panning');
      };
      this.root.addEventListener('pointermove', move);
      this.root.addEventListener('pointerup', up);
      this.root.addEventListener('pointercancel', up);
    });
  }

  private beginBoxSelect(e: PointerEvent): void {
    this.gesture = 'box';
    const r = this.root.getBoundingClientRect();
    const sx = e.clientX, sy = e.clientY;
    const additive = this.selection.kind === 'modules' ? this.selection.ids : [];
    this.marquee.style.display = '';
    this.root.setPointerCapture(e.pointerId);

    const paint = (ev: PointerEvent) => {
      const x = Math.min(sx, ev.clientX) - r.left, y = Math.min(sy, ev.clientY) - r.top;
      const w = Math.abs(ev.clientX - sx), h = Math.abs(ev.clientY - sy);
      this.marquee.style.transform = `translate(${x}px,${y}px)`;
      this.marquee.style.width = `${w}px`;
      this.marquee.style.height = `${h}px`;
      const a = this.viewport.toWorld(Math.min(sx, ev.clientX), Math.min(sy, ev.clientY));
      const b = this.viewport.toWorld(Math.max(sx, ev.clientX), Math.max(sy, ev.clientY));
      const box: Box = { x: a.x, y: a.y, w: b.x - a.x, h: b.y - a.y };
      const hits = [...this.cards.keys()].filter(id => {
        const cb = this.cardBox(id);
        return cb && boxesOverlap(box, cb);
      });
      const ids = [...new Set([...additive, ...hits])];
      this.select(ids.length ? { kind: 'modules', ids } : { kind: 'none' });
    };
    const up = () => {
      this.root.removeEventListener('pointermove', paint);
      this.root.removeEventListener('pointerup', up);
      this.root.removeEventListener('pointercancel', up);
      this.marquee.style.display = 'none';
      this.gesture = 'none';
    };
    this.root.addEventListener('pointermove', paint);
    this.root.addEventListener('pointerup', up);
    this.root.addEventListener('pointercancel', up);
  }

  // ---- visibility culling -----------------------------------------------

  updateVisibility(): void {
    const vb = this.viewport.visibleWorldBox();
    const visible: number[] = [];
    for (const id of this.cards.keys()) {
      const b = this.cardBox(id);
      const on = !!b && boxesOverlap(vb, b);
      visible.push(...(on ? [id] : []));
      this.cards.get(id)!.root.classList.toggle('offscreen', !on);
    }
    this.s.ticker.setVisible(visible);
  }

  private syncZoomLabel(): void {
    this.s.doc.ui.view = { ...this.s.doc.ui.view, x: this.viewport.view.x, y: this.viewport.view.y, zoom: this.viewport.view.s };
  }

  fit(): void {
    const boxes = [...this.cards.keys()].map(id => this.cardBox(id)).filter((b): b is Box => !!b);
    this.viewport.fit(boxes);
  }

  cardFor(id: number): ModuleCard | undefined { return this.cards.get(id) }
  allCards(): Map<number, ModuleCard> { return this.cards }

  /** Re-measure knob anchors; call after a layout-affecting change. */
  invalidateAnchors(): void { this.modAnchors.clear(); this.syncCables(); }

  get activeGesture(): string { return this.gesture }

  /**
   * Port activity, port pulses, "carrying signal" cables and the sag
   * springs, all from the one loop. Cable activity is re-checked at ~10 Hz
   * and touches the DOM only when a cable flips.
   */
  bindActivity(): void {
    const prevEvent = new Map<string, number>();
    let lastCheck = 0;
    this.s.ticker.bindGlobal(t => {
      const now = performance.now();
      for (const [id, card] of this.cards) {
        for (const p of card.allPorts()) {
          if (p.dataset.kind !== 'event') continue;
          const k = `${id}:${p.dataset.port}`;
          const v = t.eventActivity[k] ?? 0;
          if (v > 0.9 && (prevEvent.get(k) ?? 0) < 0.5 && p.dataset.direction === 'output') {
            card.pulsePort('output', p.dataset.port!);
          }
          prevEvent.set(k, v);
        }
      }
      if (now - lastCheck > 100) {
        lastCheck = now;
        for (const c of this.s.doc.connections) {
          const k = `${c.from.module}:${c.from.port}`;
          const on = c.kind === 'event'
            ? (t.eventActivity[k] ?? 0) > 0.05
            : Math.abs(t.outputs?.[k] ?? 0) > 0.02;
          this.cables.setActive(connKey(c), on);
        }
      }
      if (this.cables.settling) this.cables.step(now);
    });
  }

  // ---- selected-cable tools: colour swatches, modulation amount ---------

  private buildCableTools(): HTMLElement {
    const box = el('div', 'cable-tools');
    box.dataset.testid = 'cable-tools';
    box.style.display = 'none';
    box.addEventListener('pointerdown', e => e.stopPropagation());
    for (const name of CABLE_PALETTE) {
      const b = el('button', 'swatch') as HTMLButtonElement;
      b.type = 'button';
      b.title = name;
      b.dataset.testid = `cable-color-${name}`;
      b.style.background = `var(--cable-${name})`;
      b.addEventListener('click', () => {
        if (this.selection.kind !== 'cable') return;
        this.s.doc.setCableColor(connKey(this.selection.c), name);
        this.syncCables();
        this.placeCableTools();
      });
      box.append(b);
    }
    // Modulation depth lives on the cable (DESIGN §5: no depth handle on the knob).
    const amt = el('span', 'cable-amount');
    amt.dataset.testid = 'cable-amount';
    amt.tabIndex = 0;
    amt.title = 'Modulation amount: drag sideways (shift for fine), or use the arrow keys';
    amt.setAttribute('role', 'slider');
    const setAmt = (v: number, commit: boolean) => {
      if (this.selection.kind !== 'cable') return;
      this.s.commitModAmount(this.selection.c, Math.max(-1, Math.min(1, Math.round(v * 1000) / 1000)), commit);
    };
    amt.addEventListener('pointerdown', e => {
      if (this.selection.kind !== 'cable') return;
      e.preventDefault(); e.stopPropagation();
      amt.setPointerCapture(e.pointerId);
      const sx = e.clientX, start = this.selection.c.amount ?? 0;
      const move = (ev: PointerEvent) => setAmt(start + (ev.clientX - sx) * (ev.shiftKey ? 0.001 : 0.004), false);
      const up = () => {
        amt.removeEventListener('pointermove', move); amt.removeEventListener('pointerup', up);
        if (this.selection.kind === 'cable') setAmt(this.selection.c.amount ?? 0, true);
      };
      amt.addEventListener('pointermove', move); amt.addEventListener('pointerup', up);
    });
    amt.addEventListener('keydown', e => {
      if (this.selection.kind !== 'cable') return;
      const d = { ArrowUp: 1, ArrowRight: 1, ArrowDown: -1, ArrowLeft: -1 }[e.key];
      if (!d) return;
      e.preventDefault();
      setAmt((this.selection.c.amount ?? 0) + d * (e.shiftKey ? 0.002 : 0.01), true);
    });
    this.cableAmount = amt;
    box.append(amt);
    return box;
  }

  private placeCableTools(): void {
    const t = this.cableTools;
    if (this.selection.kind !== 'cable') { t.style.display = 'none'; return; }
    const c = this.s.doc.connections.find(x => connKey(x) === connKey((this.selection as { c: Connection }).c));
    const mid = c && this.cables.midpoint(c);
    if (!c || !mid) { t.style.display = 'none'; return; }
    this.selection = { kind: 'cable', c };
    t.style.display = '';
    t.style.transform = `translate(${mid.x.toFixed(1)}px, ${(mid.y + 12).toFixed(1)}px) translateX(-50%)`;
    const chosen = this.s.doc.cableColor(connKey(c)) ?? 'ink';
    for (const b of t.querySelectorAll<HTMLElement>('.swatch')) b.classList.toggle('on', b.title === chosen);
    const isMod = c.kind === 'modulation';
    this.cableAmount.style.display = isMod ? '' : 'none';
    if (isMod) {
      const a = Math.round((c.amount ?? 0) * 100);
      this.cableAmount.textContent = `amount ${a > 0 ? '+' : a < 0 ? '−' : ''}${Math.abs(a)}%`;
      this.cableAmount.setAttribute('aria-valuenow', String(c.amount ?? 0));
    }
  }
}

/** User cable palette (DESIGN §2, OPEN-3). Names map to `--cable-<name>`. */
export const CABLE_PALETTE = ['ink', 'rust', 'blue', 'olive', 'ochre', 'slate'] as const;
const BOUNCE = 'linear(0,.18 5%,.6 13%,1.02 21%,1.13 27%,1.1 33%,1.01 43%,.97 52%,1.005 68%,1)';

export const cableKey = connKey;
