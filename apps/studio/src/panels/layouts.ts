import type { ModuleDescriptor, ModuleParam } from '../modules/types';
import { inputs, outputs, isChoice, isToggle, hasChoices, SEGMENT_LIMIT } from '../modules/registry';

/**
 * Parameter roles (DESIGN §4.2). A role says what the parameter is *for*;
 * the control and its size follow from it. `panels/<id>.json` may set a role
 * per parameter; otherwise it is inferred from the parameter's own shape.
 */
export type Role =
  | 'hero' | 'primary' | 'secondary' | 'trim'
  | 'mode' | 'toggle' | 'fader' | 'number' | 'readout' | 'select';

export type WidgetKind = 'knob' | 'segmented' | 'toggle' | 'readout' | 'select' | 'fader' | 'number';
export type KnobTier = 'L' | 'M' | 'S';
export type PaneKind = 'wave' | 'envelope' | 'meter' | 'scope' | 'steps' | 'filter' | 'models' | 'channels';
/** Card proportion (DESIGN §7): `tall` stacks displays above controls, `wide` puts them side by side. */
export type CardShape = 'tall' | 'wide';

export interface PanelSection {
  label?: string;
  /** Legacy flag: secondary knobs in a hero section are drawn one tier up (M). */
  hero?: boolean;
  /** Controls per row. Defaults to as many as fit. */
  columns?: number;
  params: string[];
}

/**
 * A `panels/<module-id>.json` override. Everything is optional: a module
 * with no override still gets a complete working panel.
 */
export interface PanelOverride {
  width?: number;
  /** Header code, e.g. `VCF` (DESIGN §7 header A). Derived from the id when absent. */
  code?: string;
  layout?: CardShape;
  sections?: PanelSection[];
  /** Shorter labels for cramped controls. */
  short?: Record<string, string>;
  roles?: Record<string, Role>;
  /** Force a widget kind (older overrides; `roles` is preferred). */
  widgets?: Record<string, WidgetKind>;
  panes?: PaneKind[];
  /** Free-form options per pane, e.g. which parameters the filter display reads. */
  paneOptions?: Partial<Record<PaneKind, Record<string, unknown>>>;
  /** Parameters to leave off the card (still saved, still modulatable). */
  hidden?: string[];
  /** Modulatable parameters that always have a node on the left rail. Default: hero + primary, at most 2. */
  modPorts?: string[];
  /** Rail options per side: how many ports show when collapsed, which first, editable CC numbers. */
  ports?: Partial<Record<'input' | 'output', { shown?: number; priority?: string[]; assign?: 'cc' }>>;
}

export interface ResolvedLayout {
  width: number;
  /** Minimum height so the side-rail ports keep their pitch. The real height is measured once. */
  minHeight: number;
  shape: CardShape;
  gutterIn: number;
  gutterOut: number;
  sections: PanelSection[];
  short: Record<string, string>;
  roles: Record<string, Role>;
  widgets: Record<string, WidgetKind>;
  tiers: Record<string, KnobTier>;
  panes: PaneKind[];
  paneOptions: Partial<Record<PaneKind, Record<string, unknown>>>;
}

/* ---- geometry (recorded in DESIGN.md "Card sizing") -------------------- */
export const HEADER_H = 36;
const CORE_PAD_X = 12;
const CORE_MIN_W = 150;
const ITEM_GAP = 10;
/** Footprint of each control, in px. Heights are measured; widths decide the card width. */
const W_TIER: Record<KnobTier, number> = { L: 150, M: 84, S: 58 };
const PANE_MIN_W: Record<PaneKind, number> = { wave: 230, envelope: 240, meter: 160, scope: 220, steps: 430, filter: 220, models: 260, channels: 200 };

/** Vertical distance between side-rail ports. Touch needs more room. */
export let portMinPitch = 30;
export function setDensity(hit: 'mouse' | 'touch'): void {
  portMinPitch = hit === 'touch' ? 42 : 30;
}

const isTime = (u: string) => u === 's' || u === 'ms';

/** Infer roles for one module, in catalogue order (DESIGN §4.2). */
export function inferRoles(params: ModuleParam[]): Record<string, Role> {
  const out: Record<string, Role> = {};
  let hero = false;
  let primaries = 0;
  for (const p of params) {
    let r: Role;
    if (hasChoices(p) && !isChoice(p)) r = 'readout';
    else if (isToggle(p)) r = 'toggle';
    else if (isChoice(p)) r = p.choices!.length > SEGMENT_LIMIT ? 'select' : 'mode';
    else if (p.unit === 'dB') r = 'fader';
    else if (p.unit === 'BPM' || (p.step >= 1 && !p.modulatable)) r = 'number';
    else if (p.minimum < 0) r = 'trim';
    else if (!hero && p.modulatable && (p.unit === 'Hz' || isTime(p.unit))) { r = 'hero'; hero = true; }
    else r = 'secondary';
    out[p.id] = r;
  }
  // The first two remaining knobs become primaries.
  for (const p of params) {
    if (out[p.id] !== 'secondary' || primaries >= 2) continue;
    out[p.id] = 'primary'; primaries++;
  }
  return out;
}

const ROLE_WIDGET: Record<Role, WidgetKind> = {
  hero: 'knob', primary: 'knob', secondary: 'knob', trim: 'knob',
  mode: 'segmented', toggle: 'toggle', fader: 'fader', number: 'number', readout: 'readout', select: 'select',
};
const ROLE_TIER: Partial<Record<Role, KnobTier>> = { hero: 'L', primary: 'M', secondary: 'M', trim: 'S' };

/** Approximate footprint width of one control, for the card width. */
function itemWidth(p: ModuleParam, w: WidgetKind, tier: KnobTier | undefined, label: string): number {
  switch (w) {
    case 'knob': return W_TIER[tier ?? 'M'];
    case 'segmented': return 8 + (p.choices ?? []).reduce((n, c) => n + Math.max(28, 14 + c.label.length * 6.2), 0);
    case 'toggle': return 26 + label.length * 6.4;
    case 'number': return Math.max(96, 30 + label.length * 6);
    case 'select': return 150;
    case 'fader': return 46;
    default: return Math.max(90, 20 + label.length * 6);
  }
}

/** Width a port rail needs for its longest label (labels sit inside the card). */
function gutter(labels: string[]): number {
  if (!labels.length) return 14;
  const longest = labels.reduce((n, s) => Math.max(n, s.length), 0);
  return Math.round(Math.min(132, 22 + longest * 5.6));
}

/**
 * Resolve a module descriptor plus an optional override into a concrete
 * card layout. Width is derived from the controls (or set by the override);
 * height is measured once after the card is built, so content is never
 * clipped.
 */
export function resolveLayout(d: ModuleDescriptor, ov: PanelOverride | undefined): ResolvedLayout {
  const hidden = new Set(ov?.hidden ?? []);
  const visible = d.parameters.filter(p => !hidden.has(p.id));
  const inferred = inferRoles(visible);
  const short = ov?.short ?? {};

  const roles: Record<string, Role> = {};
  const widgets: Record<string, WidgetKind> = {};
  const tiers: Record<string, KnobTier> = {};
  for (const p of visible) {
    roles[p.id] = ov?.roles?.[p.id] ?? inferred[p.id];
    widgets[p.id] = ov?.widgets?.[p.id] ?? ROLE_WIDGET[roles[p.id]];
    if (widgets[p.id] === 'knob') tiers[p.id] = ROLE_TIER[roles[p.id]] ?? 'M';
  }

  let sections: PanelSection[] = ov?.sections
    ? ov.sections.map(s => ({ ...s, params: s.params.filter(id => !hidden.has(id) && visible.some(p => p.id === id)) }))
        .filter(s => s.params.length)
    : autoSections(visible.map(p => p.id), roles);

  // Any parameter the override forgot still gets a home, so the panel can
  // never silently drop a control.
  const placed = new Set(sections.flatMap(s => s.params));
  const orphans = visible.filter(p => !placed.has(p.id)).map(p => p.id);
  if (orphans.length) sections = [...sections, { label: 'MORE', params: orphans }];

  const panes = ov?.panes ?? [];
  const shape: CardShape = ov?.layout ?? 'tall';
  // Parameter nodes share the left rail, so their labels count too (owner, 3.5 review).
  const gutterIn = gutter([...inputs(d).map(p => p.name), ...d.parameters.filter(p => p.modulatable).map(p => short[p.id] ?? p.name)]);
  const gutterOut = gutter(outputs(d).map(p => p.name));

  // Width: the widest row of controls, or the widest pane.
  let controlsW = CORE_MIN_W;
  for (const s of sections) {
    const ws = s.params.map(id => {
      const p = visible.find(q => q.id === id)!;
      return itemWidth(p, widgets[id], tiers[id], short[id] ?? p.name);
    });
    const per = s.columns ?? ws.length;
    for (let i = 0; i < ws.length; i += per) {
      const row = ws.slice(i, i + per);
      controlsW = Math.max(controlsW, row.reduce((a, b) => a + b, 0) + ITEM_GAP * (row.length - 1));
    }
  }
  const panesW = panes.reduce((n, p) => Math.max(n, PANE_MIN_W[p]), 0);
  const coreW = shape === 'wide' && panes.length
    ? panesW + 18 + controlsW
    : Math.max(controlsW, panesW);
  // The header must fit: code · name · id · i, plus a view switch when there is one.
  const headerW = 24 + ((ov?.code ?? d.id.split('.').pop() ?? '').length * 8) + d.name.length * 6.2 + 54
    + (ov?.paneOptions?.filter?.graph ? 96 : 0);
  const width = Math.round(ov?.width ?? Math.max(headerW, gutterIn + gutterOut + coreW + CORE_PAD_X * 2));

  const portRows = Math.max(inputs(d).length, outputs(d).length);
  const minHeight = HEADER_H + (portRows + 1) * portMinPitch;

  return {
    width, minHeight, shape, gutterIn, gutterOut, sections, short, roles, widgets, tiers,
    panes, paneOptions: ov?.paneOptions ?? {},
  };
}

/**
 * Group parameters when no override says how (DESIGN §7: hero first,
 * primaries in a row, trims and selectors last). No labels.
 */
function autoSections(ids: string[], roles: Record<string, Role>): PanelSection[] {
  const pick = (...rs: Role[]) => ids.filter(id => rs.includes(roles[id]));
  return [
    { params: pick('hero') },
    { params: pick('primary') },
    { params: pick('secondary', 'fader') },
    { params: pick('trim', 'number', 'readout') },
    { params: pick('mode', 'toggle', 'select') },
  ].filter(s => s.params.length);
}

/* ---- override loading -------------------------------------------------- */

export class PanelOverrides {
  private map = new Map<string, PanelOverride>();

  static async load(base = 'panels'): Promise<PanelOverrides> {
    const o = new PanelOverrides();
    try {
      const res = await fetch(`${base}/index.json`);
      if (!res.ok) return o;
      const list = (await res.json()) as string[];
      await Promise.all(list.map(async id => {
        try {
          const r = await fetch(`${base}/${id}.json`);
          if (r.ok) o.map.set(id, (await r.json()) as PanelOverride);
        } catch { /* a missing override is not an error */ }
      }));
    } catch { /* no overrides at all is fine */ }
    return o;
  }

  get(id: string): PanelOverride | undefined { return this.map.get(id); }
}

