// SPDX-License-Identifier: GPL-3.0-only
import { readFileSync } from 'node:fs';
import { expect, test, type Page } from '@playwright/test';
import { boot } from '../helpers';

// Read the same repository-root fixture the native checker consumes. A missing
// case or a changed command/reply shape fails here, not in a duplicated stub.
type Wire = Record<string, any>;
type GoldenCase = { command: Wire; reply: Wire };
const cases: Record<string, GoldenCase> = {};
const pushes: Record<string, Wire[]> = {};
let hello: Wire | undefined;
let current = '';
let previous: Wire | null = null;
const fixture = readFileSync(new URL('../../../../tests/protocol/golden_v0.ndjson', import.meta.url), 'utf8');
for (const raw of fixture.split(/\r?\n/)) {
  const line = raw.trim();
  if (line.startsWith('# T')) { current = line.slice(2).trim(); previous = null; continue; }
  if (!line || line.startsWith('#') || !/^[<>@] /.test(line)) continue;
  // T14 deliberately contains invalid JSON: it is checked on the native side,
  // since the renderer bridge accepts objects rather than raw stdin bytes.
  let wire: Wire;
  try { wire = JSON.parse(line.slice(2)); } catch {
    if (current !== 'T14' || line[0] !== '>') throw new Error('Malformed golden line: ' + line);
    previous = null;
    continue;
  }
  if (line[0] === '>') {
    previous = wire;
  } else if (line[0] === '<') {
    if (wire.type === 'hello') {
      if (!hello) hello = wire;
    } else if (wire.re !== undefined && previous?.id === wire.re) {
      if (cases[wire.re]) throw new Error('Duplicate golden request: ' + wire.re);
      cases[wire.re] = { command: previous, reply: wire };
      previous = null;
    } else {
      (pushes[current] ??= []).push(wire);
    }
  }
}
if (!hello || !cases.r2 || !cases.r14b || !pushes.T13) {
  throw new Error('Golden v0 fixture is incomplete');
}

/** The mock replies for covered commands come exclusively from golden_v0.ndjson. */
async function goldenBridge(page: Page): Promise<void> {
  await page.addInitScript(({ examples, events, startup }) => {
    type Message = Record<string, any>;
    const listeners = new Set<(m: Message) => void>();
    const completed: string[] = [];
    const errors: string[] = [];
    const sent: Message[] = [];
    const refusals: Message[] = [];
    let generation = 0;
    let sessionNumber = 1;
    let selected = '';
    const session = () => 'golden-' + sessionNumber;
    const emit = (value: Message) => { for (const cb of listeners) cb(value); };
    const copy = (value: Message) => JSON.parse(JSON.stringify(value));
    const materialize = (sample: Message) => {
      const result = copy(sample);
      if (result.session === '*') result.session = session();
      if (result.gen === '=') result.gen = String(generation);
      if (result.message === '*') result.message = 'Fixture refusal';
      if (result.engine?.version === '*') result.engine.version = 'test';
      if (result.engine?.sha === '*') result.engine.sha = 'test-sha';
      if (result.telemetry?.time === '*') result.telemetry.time = 0.4;
      return result;
    };
    const sayHello = () => emit(materialize(startup));
    const answer = (cmd: Message, sample: Message, name: string) => {
      if (sample.gen === '+1') generation++;
      const reply = materialize({ ...sample, re: cmd.id, gen: sample.gen === '+1'
        ? String(generation) : sample.gen });
      emit(reply);
      if (name) completed.push(name);
    };
    const same = (actual: Message, expected: Message) => {
      const a = copy(actual), b = copy(expected);
      delete a.id; delete b.id; // RealEngine allocates its own correlation IDs.
      // PatchDoc may fill display-only UI fields, never alter the execution view.
      if (a.type === 'loadGraph' && b.patch?.ui === undefined) delete a.patch.ui;
      return JSON.stringify(a) === JSON.stringify(b);
    };
    const send = (cmd: Message) => {
      sent.push(copy(cmd));
      const name = selected;
      selected = '';
      if (name) {
        const example = examples[name];
        if (!example) { errors.push('Missing golden case ' + name); return; }
        if (!same(cmd, example.command))
          errors.push(name + ': command differs: ' + JSON.stringify(cmd));
        setTimeout(() => answer(cmd, example.reply, name), 0);
        return;
      }
      // Only application startup/autosave loads and passive device queries
      // use a fallback; every explicitly exercised case uses the transcript.
      if (cmd.type === 'loadGraph') {
        answer(cmd, examples.r2.reply, '');
      } else if (cmd.type === 'listAudioDevices') {
        answer(cmd, examples.r9.reply, '');
      } else if (cmd.type === 'listMidiDevices') {
        answer(cmd, examples.r10a.reply, '');
      } else {
        errors.push('Unscripted command ' + cmd.type);
      }
    };
    (window as any).wirescape = {
      send,
      onMessage(cb: (m: Message) => void) {
        listeners.add(cb);
        setTimeout(sayHello, 0);
        return () => listeners.delete(cb);
      },
      openPatch: async () => null,
      savePatch: async () => null,
      platform: 'linux',
    };
    (window as any).__goldenBridge = {
      sent, errors, completed, refusals,
      use: (name: string) => { selected = name; },
      push: (tag: string) => {
        for (const event of events[tag] ?? []) emit(materialize(event));
      },
      exit: () => {
        emit({ v: 0, type: 'app.engineExited', exitCode: null, signal: 'SIGTERM', expected: false });
        sessionNumber++;
        generation = 0;
        setTimeout(sayHello, 0);
      },
      generation: () => String(generation),
      session,
    };
  }, { examples: cases, events: pushes, startup: hello });
}

test('golden v0: RealEngine maps commands, replies, refusals and pushes through the shared transcript', async ({ page }) => {
  await goldenBridge(page);
  await boot(page);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');

  await page.evaluate(() => {
    (window as any).__goldenBridge.refusals = [];
    window.__wirescapeTest.engine.onRefusal((value: unknown) => {
      (window as any).__goldenBridge.refusals.push(value);
    });
  });

  const ids = [
    'r2', 'r3', 'r4a', 'r4b', 'r5', 'r6',
    'pre7a', 'pre7b', 'r7', 'r8a', 'r8b', 'r8c',
    'r9', 'r9b', 'r10a', 'r10b', 'pre12', 'r14b',
  ];
  for (const name of ids) {
    const cmd = cases[name]!.command;
    await page.evaluate(async ({ name, cmd }) => {
      const engine = window.__wirescapeTest.engine;
      (window as any).__goldenBridge.use(name);
      switch (cmd.type) {
        case 'loadGraph': await engine.loadGraph(cmd.patch); break;
        case 'addModule': engine.addModule(cmd.moduleId, cmd.moduleType, cmd.params); break;
        case 'connect': engine.connect(cmd.connection); break;
        case 'setParam': engine.setParam(cmd.moduleId, cmd.paramId, cmd.value); break;
        case 'setModAmount': engine.setModAmount(cmd.connection, cmd.amount); break;
        case 'noteOn': engine.noteOn(cmd.note, cmd.velocity); break;
        case 'noteOff': engine.noteOff(cmd.note); break;
        case 'panic': engine.panic(); break;
        case 'listAudioDevices': await engine.listAudioDevices(); break;
        case 'selectAudioDevice': engine.selectAudioDevice(cmd.deviceId); break;
        case 'listMidiDevices': await engine.listMidiDevices(); break;
        case 'setMidiEnabled': engine.setMidiEnabled(cmd.deviceId, cmd.on); break;
        default: throw new Error('No EngineClient adapter for ' + cmd.type);
      }
    }, { name, cmd });
    await expect.poll(() => page.evaluate(n =>
      (window as any).__goldenBridge.completed.includes(n), name)).toBe(true);
  }
  const result = await page.evaluate(() => ({
    errors: (window as any).__goldenBridge.errors,
    refusals: (window as any).__goldenBridge.refusals,
    sent: (window as any).__goldenBridge.sent,
    gen: (window as any).__goldenBridge.generation(),
  }));
  expect(result.errors).toEqual([]);
  expect(result.refusals.map((r: Wire) => r.detail.code)).toEqual([
    'unknown-module', 'kind-mismatch', 'busy',
  ]);
  expect(result.sent.find((r: Wire) => r.type === 'selectAudioDevice')).toMatchObject({
    deviceId: 'USB Stereo',
  });
  expect(result.sent.find((r: Wire) => r.type === 'setMidiEnabled')).toMatchObject({
    deviceId: 'M8 MIDI 1|M8:M8 MIDI 1', on: false,
  });
  expect(result.sent.find((r: Wire) => r.type === 'setModAmount')).toMatchObject({
    amount: -0.5, connection: { amount: 0.2 },
  });

  // T11 / T13 are unsolicited messages. Unsupported metrics stay empty.
  await page.evaluate(() => {
    (window as any).__goldenBridge.push('T11');
    (window as any).__goldenBridge.push('T13');
  });
  await expect.poll(() => page.evaluate(() => window.__wirescapeTest.ticker.last?.heldNotes)).toEqual([60]);
  const tele = await page.evaluate(() => window.__wirescapeTest.ticker.last);
  expect(tele?.meters[7]).toEqual([0.1, 0.1]);
  expect(tele?.scope).toEqual({});
  expect(tele?.effective).toEqual({});
  expect(tele?.outputs).toBeUndefined();

  // T15: only the new process session may complete the restoration load.
  await page.evaluate(async patch => {
    await window.__wirescapeTest.load(patch, 'Golden restore');
  }, cases.r15!.command.patch);
  await page.evaluate(() => {
    (window as any).__goldenBridge.use('r15');
    (window as any).__goldenBridge.exit();
  });
  await expect.poll(() => page.evaluate(() =>
    (window as any).__goldenBridge.completed.includes('r15'))).toBe(true);
  await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
  expect(await page.evaluate(() => (window as any).__goldenBridge.errors)).toEqual([]);
});
