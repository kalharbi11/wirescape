// SPDX-License-Identifier: GPL-3.0-only
import type { PatchV2 } from '../patch/types';

export interface SharedPatch { id: string; name: string; patch: PatchV2 }
export async function sharedPatches(): Promise<SharedPatch[]> {
  const response = await fetch(new URL('patches/index.json', document.baseURI), { signal: AbortSignal.timeout(5000), cache: 'no-cache' });
  if (!response.ok) throw new Error('Shared patch library is unavailable.');
  const text = await response.text();
  if (text.length > 8 * 1024 * 1024) throw new Error('Shared patch library is too large.');
  const data: unknown = JSON.parse(text);
  if (!data || typeof data !== 'object' || !('patches' in data) ||
      !Array.isArray(data.patches) || data.patches.length > 128) throw new Error('Invalid shared patch library.');
  return data.patches.map((item: unknown) => {
    if (!item || typeof item !== 'object') throw new Error('Invalid shared patch.');
    const entry = item as SharedPatch;
    if (typeof entry.id !== 'string' || !/^[a-z0-9][a-z0-9-]*$/.test(entry.id) ||
        typeof entry.name !== 'string' || entry.name.length > 240 ||
        entry.patch?.format !== 'wirescape.native.patch' || entry.patch.version !== 2 ||
        !Array.isArray(entry.patch.modules) || !Array.isArray(entry.patch.connections) ||
        !Array.isArray(entry.patch.outputs) || entry.patch.modules.length > 32 ||
        !entry.patch.modules.every(m => m && typeof m.module === 'string'))
      throw new Error('Invalid shared patch.');
    return entry;
  });
}
