// SPDX-License-Identifier: GPL-3.0-only
import type { Starter } from '../app/starters';
import type { ModuleRegistry } from '../modules/registry';

/**
 * Starters the running engine can play: every module in the patch must be in
 * the engine's hello catalogue. An empty catalogue proves nothing, so it hides nothing.
 */
export function playableStarters(starters: readonly Starter[], registry: ModuleRegistry): Starter[] {
  if (!registry.all.length) return [...starters];
  return starters.filter(s => s.patch().modules.every(m => registry.has(m.module)));
}
