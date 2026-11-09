// SPDX-License-Identifier: GPL-3.0-only
import { app, BrowserWindow, dialog, ipcMain, Menu, net, protocol, screen } from 'electron';
import { spawn, ChildProcessWithoutNullStreams } from 'node:child_process';
import * as fs from 'node:fs';
import * as path from 'node:path';
import { pathToFileURL } from 'node:url';
import { randomUUID } from 'node:crypto';

protocol.registerSchemesAsPrivileged([{
  scheme: 'app', privileges: { standard: true, secure: true, supportFetchAPI: true }
}]);
const LIMIT = 2 * 1024 * 1024;
const MAX_PENDING = 256;
const MAX_STDIN_BUFFER = 4 * LIMIT;
const RESTART_WINDOW_MS = 30_000;
const ENGINE_FLAGS_WITH_VALUE = new Set(['--device', '--block', '--rate', '--settings']);
const argv = process.argv.slice(1);
let enginePath: string | undefined;
let initialFullscreen = false;
let isolatedProfile: string | null = null;
const engineArgs: string[] = [];
for (let i = 0; i < argv.length; i++) {
  const a = argv[i];
  if (a === '--engine') enginePath = argv[++i];
  else if (a === '--fullscreen') initialFullscreen = true;
  else if (a === '--profile-dir') {
    const chosen = argv[++i];
    if (!chosen || !path.isAbsolute(chosen)) throw new Error('--profile-dir requires an absolute directory');
    if (isolatedProfile !== null) throw new Error('--profile-dir must be supplied only once');
    isolatedProfile = path.resolve(chosen);
  }
  else if (a === '--confirm-audio' || a === '--confirm-midi' || a === '--null-audio') engineArgs.push(a);
  else if (ENGINE_FLAGS_WITH_VALUE.has(a)) engineArgs.push(a, argv[++i]);
}
// Opt-in source/test profile: configure both Electron paths before app readiness.
// Fail closed on duplicate settings flags and symlinked paths; the engine uses
// the LAST --settings, so validating only the first would leak outside profile.
if (isolatedProfile !== null) {
  const profile = isolatedProfile;
  // Check existing ancestors BEFORE creating anything through a symlink.
  let ancestor = profile;
  while (!fs.existsSync(ancestor)) {
    const parent = path.dirname(ancestor);
    if (parent === ancestor) throw new Error('Invalid profile directory');
    ancestor = parent;
  }
  if (fs.realpathSync(ancestor) !== ancestor)
    throw new Error('Profile directory must not contain a symlinked ancestor');
  fs.mkdirSync(profile, { recursive: true, mode: 0o700 });
  if (fs.realpathSync(profile) !== profile) throw new Error('Profile directory must not be a symlink');
  const userData = path.join(profile, 'userData');
  const sessionData = path.join(profile, 'sessionData');
  for (const dir of [userData, sessionData]) {
    fs.mkdirSync(dir, { recursive: true, mode: 0o700 });
    if (fs.lstatSync(dir).isSymbolicLink() || !fs.statSync(dir).isDirectory() ||
        fs.realpathSync(dir) !== dir)
      throw new Error('Invalid profile subdirectory');
  }
  const positions: number[] = [];
  for (let i = 0; i < engineArgs.length; i++) {
    if (engineArgs[i] === '--settings') {
      positions.push(i);
      i++; // Skip this flag's value; reject missing/relative values below.
    }
  }
  if (positions.length > 1) throw new Error('Duplicate --settings in isolated profile');
  const settingsIndex = positions.length ? positions[0] : -1;
  const requested = settingsIndex === -1 ?
    path.join(profile, 'native-settings.json') : engineArgs[settingsIndex + 1];
  if (!requested || !path.isAbsolute(requested))
    throw new Error('Isolated native --settings must be an absolute file path');
  const selected = path.resolve(requested);
  if (!selected.startsWith(profile + path.sep))
    throw new Error('Native settings must stay within the isolated profile');

  // The file may not yet exist. Resolve its nearest existing parent, so an
  // inside-looking path through a symlink cannot write to an outside directory.
  let settingsParent = path.dirname(selected);
  while (!fs.existsSync(settingsParent)) {
    const parent = path.dirname(settingsParent);
    if (parent === settingsParent) throw new Error('Invalid native settings parent');
    settingsParent = parent;
  }
  const realParent = path.resolve(fs.realpathSync(settingsParent),
    path.relative(settingsParent, path.dirname(selected)));
  if (realParent !== profile && !realParent.startsWith(profile + path.sep))
    throw new Error('Native settings parent escapes the isolated profile');
  // lstat also catches dangling symlinks, which existsSync would miss.
  try {
    if (fs.lstatSync(selected).isSymbolicLink() ||
        fs.realpathSync(selected) !== path.join(realParent, path.basename(selected)))
      throw new Error('Native settings file must not be a symlink');
  } catch (error) {
    if ((error as NodeJS.ErrnoException).code !== 'ENOENT') throw error;
  }

  app.setPath('userData', userData);
  app.setPath('sessionData', sessionData);
  if (settingsIndex === -1) engineArgs.push('--settings', selected);
  else engineArgs[settingsIndex + 1] = selected;
}
let window: BrowserWindow | null = null;
const UI_DIR = path.resolve(__dirname, '../../studio/dist');
let active: { process: ChildProcessWithoutNullStreams; expected: boolean } | null = null;
let shuttingDown = false;
let restartInProgress = false;
let lastHello: string | null = null;
let engineReady = false;
let readyAt: bigint | null = null;
let loggedHelloTime = false;
let loggedAudioTime = false;
let patchPath: string | null = null;
let menuId = 0;
const pending = new Set<string>();
const failures: number[] = [];

function notify(message: object | string): void {
  if (window && !window.isDestroyed() && !window.webContents.isDestroyed()) {
    window.webContents.send('wirescape:message', typeof message === 'string' ? message : JSON.stringify(message));
  }
}
function refusal(id: string | null, code: string, message: string): void {
  notify({ v: 0, re: id, ok: false, gen: '0', code, message });
}
function decodeLine(bytes: Buffer): string | null {
  if (bytes.length > LIMIT) return null;
  try { return new TextDecoder('utf-8', { fatal: true }).decode(bytes); } catch { return null; }
}
function handleEngineLine(raw: string, owner: ChildProcessWithoutNullStreams): void {
  if (active?.process !== owner) return;
  let obj: Record<string, unknown>;
  try {
    obj = JSON.parse(raw);
    if (!obj || typeof obj !== 'object' || Array.isArray(obj) || obj.v !== 0) return;
  } catch { return; }
  if (obj.type === 'hello') {
    if (typeof obj.session !== 'string') return;
    engineReady = true;
    lastHello = raw;
    if (!loggedHelloTime && readyAt !== null) {
      loggedHelloTime = true;
      console.error('boot-to-first-hello-ms=' + Number(process.hrtime.bigint() - readyAt) / 1e6);
    }
  } else if (!engineReady) return;
  const status = obj.status as { audio?: { state?: unknown } } | undefined;
  if (obj.type === 'status' && !loggedAudioTime && readyAt !== null &&
      status?.audio?.state === 'running') {
    loggedAudioTime = true;
    console.error('boot-to-audio-running-ms=' + Number(process.hrtime.bigint() - readyAt) / 1e6);
  }
  if (typeof obj.type === 'string' && obj.type.startsWith('app.')) return;
  if (typeof obj.re === 'string') pending.delete(obj.re);
  notify(raw); // Preserve the child's wire line; only app.* is main-owned.
}
function drainLines(proc: ChildProcessWithoutNullStreams): void {
  let fragments: Buffer[] = [];
  let size = 0;
  let dropping = false;
  proc.stdout.on('data', (chunk: Buffer) => {
    let start = 0;
    while (start < chunk.length) {
      const lf = chunk.indexOf(10, start);
      const end = lf < 0 ? chunk.length : lf;
      let part = chunk.subarray(start, end);
      if (!dropping) {
        if (size + part.length > LIMIT + 1) {
          fragments = [];
          size = 0;
          dropping = true;
        } else {
          fragments.push(part);
          size += part.length;
        }
      }
      if (lf >= 0) {
        if (!dropping) {
          let line = Buffer.concat(fragments, size);
          if (line.length && line[line.length - 1] === 13) line = line.subarray(0, -1);
          const raw = decodeLine(line);
          if (raw !== null) handleEngineLine(raw, proc);
        }
        fragments = [];
        size = 0;
        dropping = false;
      }
      start = end + 1;
    }
  });
}
function command(raw: string): void {
  if (Buffer.byteLength(raw, 'utf8') > LIMIT) return;
  let obj: Record<string, unknown>;
  try {
    obj = JSON.parse(raw);
    if (!obj || typeof obj !== 'object' || Array.isArray(obj) || obj.v !== 0 ||
        typeof obj.type !== 'string' || typeof obj.id !== 'string' ||
        !/^[\x21-\x7e]{1,64}$/.test(obj.id)) return;
  } catch { return; }
  const id = obj.id as string;
  if (obj.type === 'app.restartEngine') {
    void restartEngine().then(() => notify({ v: 0, re: id, ok: true, gen: '0' }))
      .catch(() => refusal(id, 'unavailable', 'Could not restart engine'));
    return;
  }
  if (obj.type.startsWith('app.')) { refusal(id, 'invalid', 'Unknown app command'); return; }
  if (!active || !engineReady || active.process.stdin.destroyed) {
    refusal(id, 'unavailable', 'Engine is disconnected');
    return;
  }
  if (pending.has(id)) { refusal(id, 'duplicate', 'Request id already pending'); return; }
  if (pending.size >= MAX_PENDING || active.process.stdin.writableLength + Buffer.byteLength(raw) + 1 > MAX_STDIN_BUFFER) {
    refusal(id, 'busy', 'Engine queue full');
    return;
  }
  pending.add(id);
  try { active.process.stdin.write(raw + '\n', 'utf8'); }
  catch { pending.delete(id); refusal(id, 'unavailable', 'Engine pipe failed'); }
}
function engineExecutable(): string {
  if (enginePath) return enginePath;
  const filename = process.platform === 'win32' ? 'wirescape-engine.exe' : 'wirescape-engine';
  const installed = path.join(app.getAppPath(), filename);
  if (fs.existsSync(installed)) return installed;
  return path.resolve(app.getAppPath(), '../../build/native', filename);
}
function startEngine(): void {
  engineReady = false;
  lastHello = null;
  pending.clear();
  const executable = engineExecutable();
  // Only the explicit test harness may run a Node protocol stub.
  const stub = process.env.WIRESCAPE_TEST_STUB === '1';
  const child = spawn(stub ? process.execPath : executable, stub ? [executable, ...engineArgs] : engineArgs, {
    // Node's default stdio is three pipes; no socket or shell is involved.
    windowsHide: true,
    env: stub ? { ...process.env, ELECTRON_RUN_AS_NODE: '1' } : process.env
  });
  const state = { process: child, expected: false };
  active = state;
  drainLines(child);
  child.stdin.on('error', () => {
    if (active !== state) return;
    for (const id of pending) refusal(id, 'unavailable', 'Engine pipe failed');
    pending.clear();
  });
  child.stderr.on('data', (data: Buffer) => console.error('wirescape-engine:', data.toString('utf8').slice(0, 2048)));
  child.on('error', err => console.error('wirescape-engine spawn:', err.message));
  child.once('close', (code, signal) => {
    // One line per exit so an owner log separates crashes (e.g. 3221225477) from restarts.
    console.error(`engine-exit code=${code} signal=${signal} expected=${state.expected}`);
    if (active !== state) return;
    active = null;
    engineReady = false;
    lastHello = null;
    for (const id of pending) refusal(id, 'unavailable', 'Engine disconnected');
    pending.clear();
    notify({ v: 0, type: 'app.engineExited', exitCode: code, signal, expected: state.expected });
    if (!state.expected && !shuttingDown && !restartInProgress) {
      const now = Date.now();
      failures.push(now);
      while (failures.length && now - failures[0] > RESTART_WINDOW_MS) failures.shift();
      if (failures.length <= 3) setTimeout(() => {
        if (!active && !shuttingDown && !restartInProgress) startEngine();
      }, 250);
      else void showEngineFailure();
    }
  });
}
async function stopEngine(): Promise<void> {
  const state = active;
  if (!state) return;
  state.expected = true;
  const child = state.process;
  if (!child.stdin.destroyed) child.stdin.end(); // EOF is the protocol's graceful shutdown.
  await new Promise<void>(resolve => {
    if (child.exitCode !== null || child.signalCode !== null) { resolve(); return; }
    const timeout = setTimeout(() => { if (!child.killed) child.kill(); }, 2000);
    child.once('close', () => { clearTimeout(timeout); resolve(); });
  });
}
async function restartEngine(resetFailures = false): Promise<void> {
  if (shuttingDown || restartInProgress) throw new Error('Restart unavailable');
  restartInProgress = true;
  try {
    await stopEngine();
    if (resetFailures) failures.length = 0;
    startEngine();
  } finally { restartInProgress = false; }
}
async function showEngineFailure(): Promise<void> {
  if (!window || window.isDestroyed() || shuttingDown) return;
  const { response } = await dialog.showMessageBox(window, {
    type: 'error', title: 'Wirescape engine stopped',
    message: 'The engine exited repeatedly. Audio and MIDI are disconnected.',
    buttons: ['Restart engine', 'Quit'], defaultId: 0, cancelId: 1
  });
  if (response === 0) {
    failures.length = 0;
    if (!active && !shuttingDown) startEngine();
  } else app.quit();
}
function savedBounds(): Electron.Rectangle | undefined {
  try {
    const file = path.join(app.getPath('userData'), 'window-state.json');
    if (fs.statSync(file).size > 2048) return;
    const value: unknown = JSON.parse(fs.readFileSync(file, 'utf8'));
    if (!value || typeof value !== 'object') return;
    const b = value as Record<string, number>;
    if (![b.x, b.y, b.width, b.height].every(Number.isInteger) ||
        b.width < 640 || b.height < 480 || b.width > 10000 || b.height > 10000) return;
    if (!screen.getAllDisplays().some(d => b.x < d.bounds.x + d.bounds.width &&
        b.x + b.width > d.bounds.x && b.y < d.bounds.y + d.bounds.height &&
        b.y + b.height > d.bounds.y)) return;
    return { x: b.x, y: b.y, width: b.width, height: b.height };
  } catch { return; }
}
function saveBounds(): void {
  if (!window || window.isDestroyed() || window.isMinimized() || window.isFullScreen()) return;
  try {
    const file = path.join(app.getPath('userData'), 'window-state.json');
    fs.writeFileSync(file, JSON.stringify(window.getBounds()), { mode: 0o600 });
  } catch { /* An unwritable preference must not block closing. */ }
}
function senderAllowed(event: Electron.IpcMainEvent | Electron.IpcMainInvokeEvent): boolean {
  return !!window && !window.isDestroyed() && event.sender === window.webContents &&
    event.senderFrame === window.webContents.mainFrame;
}
async function patchDialogPath(save: boolean): Promise<string> {
  if (patchPath) return patchPath;
  const directory = path.join(app.getPath('userData'), 'patches');
  try {
    await fs.promises.mkdir(directory, { recursive: true, mode: 0o700 });
  } catch { throw new Error('Could not create patch folder'); }
  return save ? path.join(directory, 'Untitled.json') : directory;
}
async function libraryPatch(request: unknown): Promise<{ text: string; name: string } | null> {
  try {
    if (!request || typeof request !== 'object') throw new Error();
    const { library, name } = request as { library: unknown; name?: unknown };
    if (library === 'reset') { patchPath = null; originalV1 = undefined; return null; }
    if (library !== 'list' && library !== 'read') throw new Error();
    const directory = path.join(app.getPath('userData'), 'patches');
    await fs.promises.mkdir(directory, { recursive: true, mode: 0o700 });
    if ((await fs.promises.lstat(directory)).isSymbolicLink()) throw new Error();
    const root = await fs.promises.realpath(directory);
    if (library === 'list') {
      const names: string[] = [];
      const dir = await fs.promises.opendir(root);
      let examined = 0;
      for await (const entry of dir) {
        if (++examined > 1024 || names.length >= 256) break;
        if (entry.isFile() && entry.name.length <= 240 && /\.json$/i.test(entry.name)) names.push(entry.name);
      }
      return { name: 'User patches', text: JSON.stringify(names.sort()) };
    }
    if (typeof name !== 'string' || !name || name.length > 240 || /[\\/\x00]/.test(name) ||
        path.basename(name) !== name || !/\.json$/i.test(name)) throw new Error();
    const filename = path.join(root, name);
    if ((await fs.promises.lstat(filename)).isSymbolicLink() ||
        path.dirname(await fs.promises.realpath(filename)) !== root) throw new Error();
    const source = await readNativeFile(filename);
    patchPath = filename;
    originalV1 = source.value.version === 1
      ? { bytes: source.bytes, object: JSON.stringify(source.value) } : undefined;
    return { name, text: source.text };
  } catch { throw new Error('Could not access user patch (JSON files, 2 MiB maximum)'); }
}

// File identity is bound only after a supported native document has been read.
// The renderer remains the editable-document owner; these checks only guard I/O.
let originalV1: { bytes: Buffer; object: string } | undefined;
function nativeFile(text: string): { version: number; preserved?: unknown } {
  const value = JSON.parse(text);
  if (!value || value.format !== 'wirescape.native.patch' || ![1, 2].includes(value.version) ||
      (value.modules !== undefined && !Array.isArray(value.modules)) ||
      (value.connections !== undefined && !Array.isArray(value.connections))) throw new Error('Unsupported patch');
  const object = (item: unknown): boolean => !!item && typeof item === 'object' && !Array.isArray(item);
  if (value.ui !== undefined && (!object(value.ui) ||
      ['positions', 'view', 'labels', 'cableColors'].some(key => value.ui[key] !== undefined && !object(value.ui[key])))) {
    throw new Error('Invalid patch UI containers');
  }
  const v1 = value.version === 1;
  if (v1 && (!Array.isArray(value.modules) || !Array.isArray(value.connections))) throw new Error('Invalid v1 patch');
  const ids = new Set<number>();
  for (const m of value.modules ?? []) {
    if (!m || !Number.isSafeInteger(m.id) || m.id <= 0 || ids.has(m.id) ||
        typeof (v1 ? m.type : m.module) !== 'string' ||
        (v1 && m.values != null && typeof m.values !== 'object')) throw new Error('Invalid patch module');
    ids.add(m.id);
  }
  const endpoint = (e: any): boolean => !!e && Number.isSafeInteger(e.module) && e.module > 0 && typeof e.port === 'string';
  for (const c of value.connections ?? []) {
    const readable = c && (v1
      ? ['audio', 'control', 'signal', 'modulation', 'event'].includes(c.kind) &&
        Number.isSafeInteger(c.fromNode) && c.fromNode > 0 && typeof c.fromPort === 'string' &&
        Number.isSafeInteger(c.toNode) && c.toNode > 0 && typeof c.toPort === 'string'
      : ['signal', 'modulation', 'event'].includes(c.kind) && endpoint(c.from) && endpoint(c.to));
    if (!readable) throw new Error('Invalid patch connection');
  }
  // JSON.parse accepts duplicate keys. Refuse them before any open/save binding;
  // decoded keys detect escaped aliases, including nested objects inside arrays.
  const stack: (Set<string> | null)[] = [];
  const tokens = /"(?:\\[\s\S]|[^"\\])*"|[{}[\]]/g;
  for (const token of text.matchAll(tokens)) {
    const word = token[0];
    if (word === '{' || word === '[') {
      if (stack.length >= 64) throw new Error('Patch nesting limit');
      stack.push(word === '{' ? new Set() : null);
    } else if (word === '}' || word === ']') stack.pop();
    else {
      let end = token.index! + word.length;
      while (/\s/.test(text[end] ?? '') && end < text.length) end++;
      if (text[end] === ':') {
        const keys = stack[stack.length - 1]!;
        const key = JSON.parse(word) as string;
        if (keys.has(key)) throw new Error('Duplicate patch key');
        keys.add(key);
      }
    }
  }
  return value;
}
async function readNativeFile(filename: string): Promise<{ bytes: Buffer; text: string; value: ReturnType<typeof nativeFile> }> {
  const handle = await fs.promises.open(filename, fs.constants.O_RDONLY | (fs.constants.O_NOFOLLOW ?? 0));
  let bytes: Buffer;
  try {
    if (!(await handle.stat()).isFile()) throw new Error('Invalid patch file');
    bytes = Buffer.allocUnsafe(LIMIT + 1);
    const { bytesRead } = await handle.read(bytes, 0, LIMIT + 1, 0);
    if (bytesRead > LIMIT) throw new Error('Patch exceeds 2 MiB limit');
    bytes = bytes.subarray(0, bytesRead);
  } finally { await handle.close(); }
  const text = new TextDecoder('utf-8', { fatal: true }).decode(bytes);
  return { bytes, text, value: nativeFile(text) };
}
async function backupV1(filename: string, bytes: Buffer): Promise<void> {
  const parsed = path.parse(filename);
  const backup = path.join(parsed.dir, parsed.name + '.v1' + parsed.ext);
  const staged = backup + '.' + randomUUID() + '.tmp';
  try {
    await fs.promises.writeFile(staged, bytes, { flag: 'wx', mode: 0o600 });
    try { await fs.promises.link(staged, backup); }
    catch (error) {
      if ((error as NodeJS.ErrnoException).code !== 'EEXIST' ||
          !(await fs.promises.lstat(backup)).isFile()) throw error;
    }
  } finally { await fs.promises.rm(staged, { force: true }).catch(() => {}); }
}
async function openPatch(): Promise<{ text: string; name: string } | null> {
  if (!window) throw new Error('Window unavailable');
  const result = await dialog.showOpenDialog(window, {
    title: 'Open patch', defaultPath: await patchDialogPath(false),
    properties: ['openFile'], filters: [{ name: 'Wirescape patch', extensions: ['json'] }]
  });
  if (result.canceled || !result.filePaths.length) return null;
  try {
    const filename = result.filePaths[0];
    const source = await readNativeFile(filename);
    patchPath = filename;
    originalV1 = source.value.version === 1
      ? { bytes: source.bytes, object: JSON.stringify(source.value) } : undefined;
    return { text: source.text, name: path.basename(filename) };
  } catch { throw new Error('Could not read a supported UTF-8 native patch (2 MiB maximum)'); }
}
async function savePatch(input: unknown): Promise<{ name: string } | null> {
  if (!window || !input || typeof input !== 'object') throw new Error('Invalid patch');
  const { text, saveAs } = input as { text: unknown; saveAs: unknown };
  if (typeof text !== 'string' || typeof saveAs !== 'boolean' ||
      Buffer.byteLength(text, 'utf8') > LIMIT) throw new Error('Invalid patch or 2 MiB limit exceeded');
  let filename = !saveAs ? patchPath : null;
  if (!filename) {
    const result = await dialog.showSaveDialog(window, {
      title: 'Save patch', defaultPath: await patchDialogPath(true),
      filters: [{ name: 'Wirescape patch', extensions: ['json'] }]
    });
    if (result.canceled || !result.filePath) return null;
    filename = result.filePath;
  }
  const output = nativeFile(text);
  let previous: Awaited<ReturnType<typeof readNativeFile>> | undefined;
  try { previous = await readNativeFile(filename); }
  catch (error) { if ((error as NodeJS.ErrnoException).code !== 'ENOENT') throw new Error('Refusing to overwrite an unsupported patch'); }
  const original = previous?.value.version === 1 ? previous.bytes
    : originalV1 && JSON.stringify(output.preserved) === originalV1.object ? originalV1.bytes : undefined;
  try { if (original) await backupV1(filename, original); }
  catch { throw new Error('Could not create original patch backup; target was kept'); }
  const temp = path.join(path.dirname(filename), '.' + path.basename(filename) + '.' + randomUUID() + '.tmp');
  try {
    await fs.promises.writeFile(temp, text, { flag: 'wx', mode: 0o600 });
    await fs.promises.rename(temp, filename);
    patchPath = filename;
    return { name: path.basename(filename) };
  } catch {
    throw new Error('Could not save patch');
  } finally { await fs.promises.rm(temp, { force: true }).catch(() => {}); }
}
function configureMenu(): void {
  const intent = (type: string, options: Record<string, unknown> = {}) => notify({ v: 0, type, ...options });
  const menu = Menu.buildFromTemplate([
    { label: 'File', submenu: [
      { id: 'open-patch', label: 'Open patch…', accelerator: 'CmdOrCtrl+O', click: () => intent('app.menuOpenPatch') },
      { id: 'save-patch', label: 'Save patch', accelerator: 'CmdOrCtrl+S', click: () => intent('app.menuSavePatch', { saveAs: false }) },
      { id: 'save-as', label: 'Save as…', accelerator: 'CmdOrCtrl+Shift+S', click: () => intent('app.menuSavePatch', { saveAs: true }) },
      { type: 'separator' },
      { role: 'quit', accelerator: 'CmdOrCtrl+Q' }
    ] },
    { label: 'View', submenu: [
      { label: 'Toggle fullscreen', accelerator: 'F11', click: () => window?.setFullScreen(!window.isFullScreen()) },
      { label: 'Reload UI', accelerator: 'CmdOrCtrl+R', click: () => window?.reload() }
    ] },
    { label: 'Engine', submenu: [
      { label: 'Restart engine', click: () => { void restartEngine(true).catch(() => showEngineFailure()); } },
      { label: 'Panic', click: () => command(JSON.stringify({ v: 0, id: 'menu-' + (++menuId), type: 'panic' })) }
    ] }
  ]);
  Menu.setApplicationMenu(menu);
}
function createWindow(): void {
  window = new BrowserWindow({
    width: 1280, height: 800, minWidth: 640, minHeight: 480,
    ...savedBounds(), title: 'Wirescape',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true, nodeIntegration: false, sandbox: true
    }
  });
  window.webContents.setWindowOpenHandler(() => ({ action: 'deny' }));
  window.webContents.on('will-navigate', event => event.preventDefault());
  window.webContents.on('before-input-event', (event, input) => {
    if (input.key === 'Escape' && window?.isFullScreen()) {
      window.setFullScreen(false);
      event.preventDefault();
    }
  });
  window.on('close', saveBounds);
  window.on('closed', () => { window = null; });
  if (initialFullscreen) window.setFullScreen(true);
  void window.loadURL('app://wirescape/index.html').catch(() => {
    void dialog.showMessageBox({ type: 'error', message: 'Build the Studio UI before launching Wirescape.' });
  });
}
app.whenReady().then(() => {
  readyAt = process.hrtime.bigint();
  // Same-origin, local-only assets: unlike file://, fetch() can load catalogue JSON.
  // Reject path traversal and foreign hosts; never dispatch HTTP/network requests.
  protocol.handle('app', request => {
    try {
      const url = new URL(request.url);
      if (url.hostname !== 'wirescape' || (request.method !== 'GET' && request.method !== 'HEAD'))
        return new Response('Not found', { status: 404 });
      const pathname = decodeURIComponent(url.pathname);
      const filename = path.resolve(UI_DIR, '.' + pathname);
      if (!filename.startsWith(UI_DIR + path.sep) || !fs.existsSync(filename))
        return new Response('Not found', { status: 404 });
      return net.fetch(pathToFileURL(filename).toString());
    } catch { return new Response('Invalid path', { status: 400 }); }
  });
  ipcMain.on('wirescape:ready', event => { if (senderAllowed(event) && lastHello) notify(lastHello); });
  ipcMain.on('wirescape:escape', event => {
    if (senderAllowed(event) && window?.isFullScreen()) window.setFullScreen(false);
  });
  ipcMain.on('wirescape:send', (event, raw: unknown) => {
    if (senderAllowed(event) && typeof raw === 'string') command(raw);
  });
  ipcMain.handle('wirescape:open', (event, request: unknown) => {
    if (!senderAllowed(event)) throw new Error('Unavailable');
    return request === undefined ? openPatch() : libraryPatch(request);
  });
  ipcMain.handle('wirescape:save', (event, input: unknown) => {
    if (!senderAllowed(event)) throw new Error('Unavailable');
    return savePatch(input);
  });
  createWindow();
  configureMenu();
  startEngine();
});
app.on('window-all-closed', () => app.quit());
app.on('before-quit', event => {
  if (shuttingDown) return;
  event.preventDefault();
  shuttingDown = true;
  void stopEngine().finally(() => app.quit());
});
