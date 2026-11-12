// SPDX-License-Identifier: GPL-3.0-only
import type { ModuleDescriptor, ModuleParam, ModulePort, PortDirection } from '../modules/types';
import { inputs, outputs, hintColor, findPort, findParam, formatValue } from '../modules/registry';
import { HEADER_H, portMinPitch } from '../panels/layouts';
import { Panel, type PanelHost } from '../panels/PanelBuilder';
import type { PanelOverride } from '../panels/layouts';
import { attachModuleInfo } from '../panels/widgets/InfoBox';
import { el, motionOff } from '../app/dom';
import type { Telemetry, OscStatus } from '../engine/EngineClient';

export interface PortAnchor { x: number; y: number }

export interface CardOptions {
  host: PanelHost;
  moduleId: number;
  descriptor: ModuleDescriptor | null;
  override: PanelOverride | undefined;
  /** Used only for a missing module, to reconstruct its ports. */
  savedType: string;
  onHeaderPointerDown(e: PointerEvent): void;
  onRenameRequest(): void;
  onEditOsc?(): void;
  oscStatus?(): OscStatus | undefined;
  /** Wire a port node's gestures (called once per node, when it is created). */
  wirePort?(node: HTMLElement): void;
  /** The rails changed (a node appeared or went): re-measure and reroute. */
  onPortsChanged?(): void;
}

/** A rail with more ports than this collapses to a few plus + / − (owner, 3.5 review). */
export const COLLAPSE_AT = 5;
const SHOW_DEFAULT = 4;

interface RailItem { dir: PortDirection; id: string; node: HTMLElement; param?: boolean; ctl?: boolean }

/** Per-side rail options from `panels/<id>.json` → `ports`. */
export interface RailOptions {
  /** How many ports a collapsed rail shows before + (default 4). */
  shown?: number;
  /** Ports that show first when the rail is collapsed. */
  priority?: string[];
  /** `cc`: each port carries an editable MIDI CC number in its label. */
  assign?: 'cc';
}

/** Short module code for the header: `VCF`, `LFO`, `MO` (DESIGN §7 header A). */
export function moduleCode(type: string, override?: string): string {
  if (override) return override;
  const last = type.split('.').pop() ?? type;
  const words = last.split('_').filter(Boolean);
  if (words.length > 1) return words.map(w => w[0]).join('').toUpperCase();
  return (last.length <= 4 ? last : last.slice(0, 3)).toUpperCase();
}

/**
 * A module card: header (code · name ……… id · i), the panel, and ports on
 * the side rails, spread evenly along the full height and straddling the
 * edge (DESIGN §7). The height comes from the content and is measured once
 * after the card is in the DOM (`measure()`); port anchors are derived from
 * that height arithmetically and never read again.
 */
export class ModuleCard {
  readonly root: HTMLElement;
  readonly panel: Panel | null = null;
  readonly width: number;
  height = 0;
  readonly missing: boolean;
  private minHeight: number;
  private readonly body: HTMLElement;
  private readonly titleEl: HTMLElement;
  private rail: Record<PortDirection, RailItem[]> = { input: [], output: [] };
  private readonly nodePool = new Map<string, HTMLElement>();
  private readonly sigIns: ModulePort[];
  private readonly sigOuts: ModulePort[];
  private readonly baseMinHeight: number;
  private readonly portEls = new Map<string, HTMLElement>();
  private readonly anchors = new Map<string, PortAnchor>();
  private readonly polyBadge: HTMLElement;
  private readonly info: { destroy(): void } | null = null;
  private monitorOff: (() => void) | null = null;
  private oscLabel?: HTMLElement;
  private oscCount?: HTMLElement;
  private oscDot?: HTMLElement;
  private oscLastReceived: number | undefined;

  constructor(private readonly o: CardOptions) {
    const d = o.descriptor;
    this.missing = !d;

    this.root = el('section', `module${this.missing ? ' missing' : ''}`);
    this.root.dataset.id = String(o.moduleId);
    this.root.dataset.type = o.savedType;
    this.root.dataset.testid = `module-${o.moduleId}`;
    this.root.setAttribute('aria-label', `${d?.name ?? o.savedType} module`);

    // ---- header: CODE  Name ……… id  i --------------------------------
    const header = el('div', 'module-header');
    header.dataset.testid = `module-header-${o.moduleId}`;
    header.style.height = `${HEADER_H}px`;
    const code = el('span', 'module-code', this.missing ? '?' : moduleCode(d!.id, o.override?.code));
    code.title = this.missing ? o.savedType : d!.id;
    this.titleEl = el('span', 'module-title');
    this.polyBadge = el('span', 'poly-badge');
    this.polyBadge.dataset.testid = `poly-${o.moduleId}`;
    this.polyBadge.style.display = 'none';
    const idTag = el('span', 'module-inst', `#${o.moduleId}`);
    const slot = el('span', 'module-header-slot');
    header.append(code, this.titleEl, this.polyBadge, el('span', 'module-header-fill'), idTag, slot);
    header.addEventListener('pointerdown', e => {
      if ((e.target as HTMLElement).closest('.ib,.seg,.view-switch,button,select,.module-presets')) return;
      o.onHeaderPointerDown(e);
    });
    this.titleEl.addEventListener('dblclick', e => { e.stopPropagation(); o.onRenameRequest(); });
    this.root.append(header);

    // ---- body ----------------------------------------------------------
    this.body = el('div', 'module-body');
    const ins = d ? inputs(d) : this.reconstructPorts('input');
    const outs = d ? outputs(d) : this.reconstructPorts('output');

    let coreEl: HTMLElement;
    if (d) {
      this.panel = new Panel(o.host, o.moduleId, d, o.override, () => {
        this.measure();
        o.onPortsChanged?.();
      });
      coreEl = this.panel.root;
      this.width = this.panel.layout.width;
      this.minHeight = this.panel.layout.minHeight;
      this.body.style.paddingLeft = `${this.panel.layout.gutterIn}px`;
      this.body.style.paddingRight = `${this.panel.layout.gutterOut}px`;
      for (const h of this.panel.headerEls) slot.before(h);
    } else {
      coreEl = this.buildMissingCore();
      this.width = 420;
      this.minHeight = HEADER_H + (Math.max(ins.length, outs.length) + 1) * portMinPitch;
      this.body.style.paddingLeft = '110px';
      this.body.style.paddingRight = '110px';
    }

    this.body.append(coreEl);
    if (d?.id === 'monitor') this.mountMonitorLog();
    if (d?.id === 'osc.in') {
      const box = el('div', 'osc-card');
      this.oscLabel = el('span', 'osc-status-label', 'OSC status unavailable');
      this.oscLabel.dataset.testid = `osc-status-${o.moduleId}`;
      this.oscCount = el('span', 'osc-counter', 'Received: 0');
      this.oscCount.dataset.testid = `osc-received-${o.moduleId}`;
      this.oscDot = el('span', 'osc-activity-dot');
      this.oscDot.setAttribute('aria-hidden', 'true');
      const status = el('div', 'osc-status-row');
      status.append(this.oscDot, this.oscLabel);
      const edit = el('button', 'btn osc-edit', 'Edit…') as HTMLButtonElement;
      edit.type = 'button'; edit.dataset.testid = `osc-edit-${o.moduleId}`;
      edit.addEventListener('click', () => o.onEditOsc?.());
      box.append(status, this.oscCount, edit);
      this.body.append(box);
      this.setOscStatus(o.oscStatus?.());
    }
    this.sigIns = ins;
    this.sigOuts = outs;
    // The rails set their own height in syncPorts (a collapsed rail is short), so the base only keeps two rows.
    this.baseMinHeight = HEADER_H + 2 * portMinPitch;
    this.root.append(this.body);
    this.root.style.width = `${this.width}px`;
    this.syncPorts(false);

    if (d) {
      this.info = attachModuleInfo(this.root, slot, {
        connections: () => this.describeConnections(),
        modulated: () => this.describeModulated(),
      }, o.host.ticker, `module-info-${o.moduleId}`);
    }
    this.refreshHeader();
  }

  /** Direct-input Monitor log: bounded snapshots from the engine; no inferred signal values. */
  private mountMonitorLog(): void {
    const id = this.o.moduleId;
    const wrap = el('section', 'monitor-log');
    wrap.dataset.testid = `monitor-log-${id}`;
    wrap.style.cssText = 'width:100%;min-width:0;box-sizing:border-box;padding:9px;border:1px solid var(--hair);font:11px var(--mono);';
    const head = el('div', 'monitor-log-head');
    head.style.cssText = 'display:flex;flex-wrap:wrap;justify-content:space-between;align-items:center;gap:6px;';
    const title = el('strong', '', 'Messages');
    const actions = el('div');
    actions.style.cssText = 'display:flex;gap:6px;';
    const pause = el('button', 'btn', 'Pause') as HTMLButtonElement;
    pause.type = 'button';
    pause.dataset.testid = `monitor-pause-${id}`;
    pause.setAttribute('aria-pressed', 'false');
    const clear = el('button', 'btn', 'Clear') as HTMLButtonElement;
    clear.type = 'button';
    clear.dataset.testid = `monitor-clear-${id}`;
    actions.append(pause, clear); head.append(title, actions);
    const list = el('div', 'monitor-lines');
    list.dataset.testid = `monitor-lines-${id}`;
    list.setAttribute('role', 'log');
    list.setAttribute('aria-label', 'Monitor messages, oldest first');
    list.style.cssText = 'height:138px;overflow-y:auto;overflow-wrap:anywhere;padding:7px 1px;';
    const note = el('small', '', 'direct OSC/MIDI only');
    note.style.cssText = 'display:block;color:var(--sub);';
    wrap.append(head, list, note);
    this.body.append(wrap);
    let paused = false, clearedAt = -Infinity, drawn: Telemetry | null = null;
    let seenGen: string | undefined;
    // The sidecar clears its ring and restarts entry timestamps at each graph commit.
    // A same-card edit can replan without remounting this card, so Clear is epoch-local.
    const syncEpoch = (t: Telemetry | null) => {
      if (t?.gen === undefined || t.gen === seenGen) return;
      seenGen = t.gen;
      clearedAt = -Infinity;
      drawn = null;
    };
    const render = (t: Telemetry | null, force = false) => {
      if (!t) return;
      syncEpoch(t);
      if (paused || (!force && t === drawn)) return;
      drawn = t;
      if (t.monitorAvailable !== true) {
        list.replaceChildren(el('span', '', 'not available'));
        return;
      }
      const entries = (t.monitor?.[id] ?? []).filter(x => Number.isFinite(x.t) && x.t > clearedAt).slice(-32);
      if (!entries.length) {
        list.replaceChildren(el('span', '', 'direct OSC/MIDI only'));
        return;
      }
      list.replaceChildren(...entries.map((entry, i) => {
        const address = entry.kind === 'osc' ? (entry.address ?? '?') : `note ${entry.note ?? '?'}`;
        const args = (entry.args ?? []).filter(Number.isFinite).slice(0, 4).join(', ');
        const line = el('div', 'monitor-line', `${entry.t.toFixed(2)}s · ${entry.kind.toUpperCase()} · ${address} · ${args}`);
        line.dataset.testid = `monitor-line-${id}-${i}`;
        line.style.cssText = 'padding:3px 0;border-bottom:1px solid var(--hair);';
        return line;
      }));
      list.scrollTop = list.scrollHeight;
    };
    pause.addEventListener('click', () => {
      paused = !paused;
      pause.textContent = paused ? 'Resume' : 'Pause';
      pause.setAttribute('aria-pressed', String(paused));
      if (!paused) render(this.o.host.ticker.last, true);
    });
    clear.addEventListener('click', () => {
      const current = this.o.host.ticker.last;
      syncEpoch(current);
      // The paused display is stale; Clear must use the latest ingress snapshot.
      const seen = current?.monitor?.[id] ?? [];
      if (seen.length) clearedAt = Math.max(clearedAt, seen[seen.length - 1].t);
      list.replaceChildren(el('span', '', 'direct OSC/MIDI only'));
      drawn = null;
    });
    this.monitorOff = this.o.host.ticker.bind(id, t => render(t));
    render(this.o.host.ticker.last);
  }

  /**
   * Read the card's natural height once (call after it is in the DOM, and
   * batch calls for many cards), then place the ports from it.
   */
  measure(): void {
    if (!this.root.isConnected) return;
    this.root.style.height = '';
    this.height = Math.max(this.minHeight, Math.ceil(this.root.offsetHeight));
    this.root.style.height = `${this.height}px`;
    const bodyH = this.height - HEADER_H;
    this.anchors.clear();
    for (const dir of ['input', 'output'] as const) {
      const items = this.rail[dir];
      items.forEach((it, i) => {
        const y = ((i + 1) * bodyH) / (items.length + 1);
        it.node.style.top = `${y}px`;
        if (it.ctl) return;
        this.anchors.set(it.param ? pkey(it.id) : key(dir, it.id), { x: dir === 'input' ? 0 : this.width, y: HEADER_H + y });
      });
    }
  }

  /* ---- rails: ports, parameter nodes, collapse --------------------------- */

  private connected(dir: PortDirection, port: string): boolean {
    const id = this.o.moduleId;
    return this.o.host.doc.connections.some(c => dir === 'input'
      ? c.kind !== 'modulation' && c.to.module === id && c.to.port === port
      : c.from.module === id && c.from.port === port);
  }

  private railOpt(dir: PortDirection): RailOptions {
    return (this.o.override as { ports?: Partial<Record<PortDirection, RailOptions>> } | undefined)?.ports?.[dir] ?? {};
  }

  private uiMap<T>(name: string): Record<string, T> {
    const ui = this.o.host.doc.ui as Record<string, unknown>;
    const all = (ui[name] ?? {}) as Record<string, Record<string, T>>;
    return all[String(this.o.moduleId)] ?? {};
  }

  private setUi(name: string, k: string, v: unknown): void {
    const ui = this.o.host.doc.ui as Record<string, unknown>;
    const all = { ...((ui[name] ?? {}) as Record<string, Record<string, unknown>>) };
    all[String(this.o.moduleId)] = { ...(all[String(this.o.moduleId)] ?? {}), [k]: v };
    ui[name] = all;
  }

  /** How many ports a collapsed rail shows (patch `ui.portsShown`, else the override, else 4). */
  private shownCount(dir: PortDirection, total: number): number {
    const n = this.uiMap<number>('portsShown')[dir] ?? this.railOpt(dir).shown ?? SHOW_DEFAULT;
    return Math.max(1, Math.min(total, n));
  }

  /**
   * Parameter nodes (owner, 3.5 review): a few essential modulatable
   * parameters always have a node; any other parameter gets one the moment
   * something modulates it, and loses it when the last cable goes.
   */
  private paramNodes(): ModuleParam[] {
    const d = this.o.descriptor;
    if (!d) return [];
    const id = this.o.moduleId;
    const modulated = new Set(this.o.host.doc.connections
      .filter(c => c.kind === 'modulation' && c.to.module === id).map(c => c.to.port));
    const essential = new Set(this.essentialParams());
    return d.parameters.filter(p => p.modulatable && (essential.has(p.id) || modulated.has(p.id)));
  }

  private essentialParams(): string[] {
    const ov = this.o.override as { modPorts?: string[] } | undefined;
    if (ov?.modPorts) return ov.modPorts;
    const d = this.o.descriptor!;
    const roles = this.panel?.layout.roles ?? {};
    return d.parameters
      .filter(p => p.modulatable && (roles[p.id] === 'hero' || roles[p.id] === 'primary'))
      .slice(0, 2).map(p => p.id);
  }

  /** Rebuild the rails from the patch. Returns true when a node appeared or went. */
  syncPorts(notify = true): boolean {
    const before = (['input', 'output'] as const).map(d => this.rail[d].map(r => (r.param ? 'p:' : '') + r.id).join(',')).join('|');
    const next: Record<PortDirection, RailItem[]> = { input: [], output: [] };
    for (const dir of ['input', 'output'] as const) {
      const list = dir === 'input' ? this.sigIns : this.sigOuts;
      let shown = list;
      const collapsible = list.length > COLLAPSE_AT;
      if (collapsible) {
        const n = this.shownCount(dir, list.length);
        const pri = this.railOpt(dir).priority ?? [];
        const ranked = [...list].sort((a, b) => rank(pri, a.id, list) - rank(pri, b.id, list));
        const keep = new Set(ranked.slice(0, n).map(p => p.id));
        shown = list.filter(p => keep.has(p.id) || this.connected(dir, p.id));
      }
      for (const p of shown) next[dir].push({ dir, id: p.id, node: this.portNode(dir, p) });
      if (dir === 'input') {
        for (const p of this.paramNodes()) next.input.push({ dir, id: p.id, param: true, node: this.paramNode(p) });
      }
      if (collapsible) next[dir].push({ dir, id: '__ctl', ctl: true, node: this.railCtl(dir, shown.length, list.length) });
    }
    for (const dir of ['input', 'output'] as const) for (const it of this.rail[dir]) it.node.remove();
    this.portEls.clear();
    for (const dir of ['input', 'output'] as const) {
      for (const it of next[dir]) {
        this.body.append(it.node);
        if (!it.ctl) this.portEls.set(it.param ? pkey(it.id) : key(dir, it.id), it.node);
      }
    }
    this.rail = next;
    const rows = Math.max(next.input.length, next.output.length);
    this.minHeight = Math.max(this.baseMinHeight, HEADER_H + (rows + 1) * portMinPitch);
    this.root.style.minHeight = `${this.minHeight}px`;
    const after = (['input', 'output'] as const).map(d => this.rail[d].map(r => (r.param ? 'p:' : '') + r.id).join(',')).join('|');
    const changed = before !== after;
    if (changed && notify && this.root.isConnected) { this.measure(); this.o.onPortsChanged?.(); }
    return changed;
  }

  private portNode(dir: PortDirection, p: ModulePort): HTMLElement {
    const k = key(dir, p.id);
    const hit = this.nodePool.get(k);
    if (hit) return hit;
    const node = el('div', `port ${dir}`);
    node.dataset.module = String(this.o.moduleId);
    node.dataset.port = p.id;
    node.dataset.direction = dir;
    node.dataset.kind = p.kind;
    node.dataset.hint = String(p.hint);
    node.dataset.poly = String(p.poly);
    if (p.stereo_pair) node.dataset.stereo = p.stereo_pair;
    node.dataset.testid = `port-${this.o.moduleId}-${dir}-${p.id}`;
    node.title = `${dir === 'input' ? 'Input' : 'Output'}: ${p.name} · ${p.kind} / ${p.hint}`;
    const jack = el('span', 'port-jack');
    jack.style.setProperty('--hint', hintColor(String(p.hint)));
    jack.append(el('span', 'port-ring'), el('span', 'port-activity'));
    const label = this.railOpt(dir).assign === 'cc' ? this.ccLabel(p) : el('span', 'port-label', p.name);
    if (dir === 'input') node.append(jack, label); else node.append(label, jack);
    this.nodePool.set(k, node);
    this.o.wirePort?.(node);
    return node;
  }

  /** A modulation input for a parameter: rust ring, lives on the left rail. */
  private paramNode(p: ModuleParam): HTMLElement {
    const k = pkey(p.id);
    const hit = this.nodePool.get(k);
    if (hit) return hit;
    const node = el('div', 'port input param');
    node.dataset.module = String(this.o.moduleId);
    node.dataset.port = p.id;
    node.dataset.param = p.id;
    node.dataset.direction = 'input';
    node.dataset.kind = 'param';
    node.dataset.testid = `param-port-${this.o.moduleId}-${p.id}`;
    node.title = `Modulation input: ${p.name}. Drag an output here, or right-click an output and choose Map.`;
    const jack = el('span', 'port-jack');
    jack.append(el('span', 'port-ring'), el('span', 'port-activity'));
    const short = this.o.override?.short?.[p.id] ?? p.name;
    node.append(jack, el('span', 'port-label', short));
    this.nodePool.set(k, node);
    this.o.wirePort?.(node);
    return node;
  }

  /** + / − under a collapsed rail, with the count. */
  private railCtl(dir: PortDirection, shown: number, total: number): HTMLElement {
    const k = `ctl:${dir}`;
    let node = this.nodePool.get(k);
    if (!node) {
      node = el('div', `port-ctl ${dir}`);
      const less = el('button', 'pc-btn', '−');
      const more = el('button', 'pc-btn', '+');
      less.dataset.testid = `ports-less-${this.o.moduleId}-${dir}`;
      more.dataset.testid = `ports-more-${this.o.moduleId}-${dir}`;
      less.title = 'Show fewer ports'; more.title = 'Show more ports';
      const count = el('span', 'pc-count');
      node.append(less, count, more);
      const step = (d: number) => {
        const list = dir === 'input' ? this.sigIns : this.sigOuts;
        this.setUi('portsShown', dir, Math.max(1, Math.min(list.length, this.shownCount(dir, list.length) + d)));
        this.syncPorts();
      };
      less.addEventListener('pointerdown', e => { e.stopPropagation(); step(-1); });
      more.addEventListener('pointerdown', e => { e.stopPropagation(); step(1); });
      this.nodePool.set(k, node);
    }
    node.querySelector('.pc-count')!.textContent = `${shown}/${total}`;
    return node;
  }

  /** `CC 74` label you can click and type into (stored in patch `ui.ccMap`). */
  private ccLabel(p: ModulePort): HTMLElement {
    const lab = el('span', 'port-label cc-label');
    const def = Number(p.id.replace(/\D/g, '')) || 1;
    const show = () => { lab.textContent = `CC ${this.uiMap<number>('ccMap')[p.id] ?? def}`; };
    lab.dataset.port = p.id;
    show();
    lab.title = 'Click to type the CC number';
    lab.addEventListener('pointerdown', e => {
      e.stopPropagation(); e.preventDefault();
      const inp = el('input', 'cc-input') as HTMLInputElement;
      inp.type = 'number'; inp.min = '0'; inp.max = '127';
      inp.value = String(this.uiMap<number>('ccMap')[p.id] ?? def);
      lab.replaceChildren(inp);
      inp.focus(); inp.select();
      const done = (ok: boolean) => {
        const n = Math.round(Number(inp.value));
        if (ok && Number.isFinite(n) && n >= 0 && n <= 127) this.setUi('ccMap', p.id, n);
        show();
      };
      inp.addEventListener('keydown', ev => { if (ev.key === 'Enter') done(true); if (ev.key === 'Escape') done(false); ev.stopPropagation(); });
      inp.addEventListener('blur', () => done(true));
      inp.addEventListener('pointerdown', ev => ev.stopPropagation());
    });
    return lab;
  }

  /** Re-read the `CC n` labels after a Learn (or undo) changed `ui.ccMap`. */
  refreshCcLabels(): void {
    const map = this.uiMap<number>('ccMap');
    for (const lab of this.root.querySelectorAll<HTMLElement>('.cc-label')) {
      if (lab.querySelector('input')) continue;
      const port = lab.dataset.port ?? '';
      lab.textContent = `CC ${map[port] ?? (Number(port.replace(/\D/g, '')) || 1)}`;
    }
  }

  /** Reconstruct a missing module's ports from the cables that reference it. */
  private reconstructPorts(dir: PortDirection): ModulePort[] {
    const id = this.o.moduleId;
    const names = new Set<string>();
    for (const c of this.o.host.doc.connections) {
      if (dir === 'input' && c.to.module === id && c.kind !== 'modulation') names.add(c.to.port);
      if (dir === 'output' && c.from.module === id) names.add(c.from.port);
    }
    return [...names].map(n => ({
      id: n, name: n, direction: dir, kind: 'signal' as const,
      hint: 'audio', poly: 'poly', stereo_pair: null,
    }));
  }

  private buildMissingCore(): HTMLElement {
    const core = el('div', 'panel-core missing-core');
    core.append(el('div', 'missing-note',
      'This module type is not in the catalogue. Its ports and values are kept exactly as saved.'));
    const m = this.o.host.doc.modules.get(this.o.moduleId);
    const params = Object.entries(m?.parameters ?? {});
    if (params.length) {
      const list = el('div', 'missing-params');
      for (const [k, v] of params) {
        const row = el('div', 'control-number static');
        row.append(el('div', 'control-label', k), el('div', 'control-readout', String(v)));
        list.append(row);
      }
      core.append(list);
    }
    return core;
  }

  anchor(dir: PortDirection, port: string): PortAnchor | undefined {
    return this.anchors.get(key(dir, port));
  }
  /** Card-local anchor of a parameter's modulation node, if it is on the rail. */
  paramAnchor(param: string): PortAnchor | undefined {
    return this.anchors.get(pkey(param));
  }
  /** Parameter nodes and knobs that a mapping gesture can target. */
  paramEl(param: string): HTMLElement | undefined { return this.portEls.get(pkey(param)); }
  portEl(dir: PortDirection, port: string): HTMLElement | undefined {
    return this.portEls.get(key(dir, port));
  }
  allPorts(): HTMLElement[] { return [...this.portEls.values()] }

  /** One-shot port pulse (event, DESIGN §8: 380 ms, scale 1→2.4, opacity .9→0). */
  pulsePort(dir: PortDirection, port: string): void {
    const ring = (this.portEls.get(key(dir, port)) ?? (dir === 'input' ? this.portEls.get(pkey(port)) : undefined))
      ?.querySelector<HTMLElement>('.port-ring');
    if (!ring || motionOff()) return;
    ring.getAnimations().forEach(a => a.cancel());
    ring.animate([{ transform: 'scale(1)', opacity: 0.9 }, { transform: 'scale(2.4)', opacity: 0 }],
      { duration: 380, easing: 'cubic-bezier(.2,.7,.3,1)' });
  }

  refreshHeader(): void {
    const label = this.o.host.doc.label(this.o.moduleId);
    const d = this.o.descriptor;
    // A name that only repeats the code ("VCF  VCF") gives way to the category ("VCF  Filter").
    const code = d ? moduleCode(d.id, this.o.override?.code) : '';
    const base = d ? (d.name.toUpperCase() === code ? d.category : d.name) : this.o.savedType;
    this.titleEl.textContent = label ?? base;
    this.titleEl.title = label ? `${label} — ${base} · double-click to rename` : 'Double-click to rename';
    this.titleEl.dataset.testid = `module-title-${this.o.moduleId}`;
  }

  /** Actual route counters are per-module, not the global OSC aggregate. */
  setOscStatus(osc?: OscStatus): void {
    if (!this.oscLabel || !this.oscCount || !this.oscDot) return;
    const route = osc?.routes?.find(r => r.module === String(this.o.moduleId));
    this.oscLabel.textContent = !route ? 'OSC host unavailable' :
      route.state === 'port-busy' ? `Port busy · ${route.port}` :
      route.state === 'lan-not-confirmed' ? `LAN not confirmed · 127.0.0.1:${route.port}` :
      `Listening on ${route.bind}:${route.port}`;
    this.oscCount.textContent = `Received: ${route?.received ?? 0}`;
    if (route && this.oscLastReceived !== undefined && route.received > this.oscLastReceived && !motionOff()) {
      this.oscDot.animate([{ opacity: 1, transform: 'scale(1.6)' },
        { opacity: 0.25, transform: 'scale(1)' }], { duration: 400, easing: 'ease-out' });
    }
    this.oscDot.classList.toggle('listening', route?.state === 'listening');
    this.oscLastReceived = route?.received;
  }

  /** Poly badge shows the voice count when the module is polyphonic. */
  setPolyVoices(n: number | null): void {
    if (n === null || n <= 1) { this.polyBadge.style.display = 'none'; return; }
    this.polyBadge.style.display = '';
    this.polyBadge.textContent = `${n}v`;
    this.polyBadge.title = `Runs polyphonically, ${n} voices`;
  }

  /** Readouts for a missing module, kept in step with the saved values. */
  refreshMissing(): void {
    if (!this.missing) return;
    const m = this.o.host.doc.modules.get(this.o.moduleId);
    const rows = [...this.root.querySelectorAll<HTMLElement>('.missing-params .control-number')];
    const params = Object.entries(m?.parameters ?? {});
    rows.forEach((row, i) => {
      const v = params[i]?.[1];
      if (v !== undefined) row.querySelector('.control-readout')!.textContent = String(v);
    });
  }

  // ---- module info (i in the header) -----------------------------------

  private nameOf(id: number): string {
    const h = this.o.host;
    const m = h.doc.modules.get(id);
    return h.doc.label(id) ?? (m ? moduleCode(m.module) : '?') + ` ${id}`;
  }

  private describeConnections(): string[] {
    const h = this.o.host, id = this.o.moduleId, d = this.o.descriptor!;
    const out: string[] = [];
    for (const c of h.doc.connections) {
      if (c.kind === 'modulation') continue;
      if (c.to.module === id) {
        const src = h.doc.modules.get(c.from.module);
        const sd = src && h.registry.get(src.module);
        const sp = sd ? findPort(sd, 'output', c.from.port)?.name ?? c.from.port : c.from.port;
        out.push(`${(findPort(d, 'input', c.to.port)?.name ?? c.to.port).toUpperCase()} ← ${this.nameOf(c.from.module)} · ${sp}`);
      } else if (c.from.module === id) {
        const dst = h.doc.modules.get(c.to.module);
        const dd = dst && h.registry.get(dst.module);
        const dp = dd ? findPort(dd, 'input', c.to.port)?.name ?? c.to.port : c.to.port;
        out.push(`${(findPort(d, 'output', c.from.port)?.name ?? c.from.port).toUpperCase()} → ${this.nameOf(c.to.module)} · ${dp}`);
      }
    }
    return out.length ? out : ['no connections'];
  }

  private describeModulated(): { param: string; sources: string; live(): string }[] {
    const h = this.o.host, id = this.o.moduleId, d = this.o.descriptor!;
    const byParam = new Map<string, string[]>();
    for (const c of h.doc.connections) {
      if (c.kind !== 'modulation' || c.to.module !== id) continue;
      const list = byParam.get(c.to.port) ?? [];
      list.push(this.nameOf(c.from.module));
      byParam.set(c.to.port, list);
    }
    return [...byParam].map(([pid, srcs]) => {
      const p = findParam(d, pid);
      return {
        param: (p?.name ?? pid).toUpperCase(),
        sources: srcs.join(' + '),
        live: () => {
          const v = h.ticker.last?.effective[id]?.[pid] ?? h.doc.param(id, pid) ?? p?.default ?? 0;
          if (!p) return String(v);
          const f = formatValue(p, v);
          return `→ ${f.unit === '%' ? f.text + '%' : f.text + (f.unit ? ' ' + f.unit : '')}`;
        },
      };
    });
  }

  destroy(): void { this.monitorOff?.(); this.info?.destroy(); this.panel?.destroy(); this.root.remove(); }

  static describeValue = formatValue;
}

const key = (dir: PortDirection, port: string) => `${dir}:${port}`;
const pkey = (param: string) => `param:${param}`;
const rank = (pri: string[], id: string, list: ModulePort[]) => {
  const i = pri.indexOf(id);
  return i >= 0 ? i - 1000 : list.findIndex(p => p.id === id);
};
