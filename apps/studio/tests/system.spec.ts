// SPDX-License-Identifier: GPL-3.0-only
import { test, expect, type Page } from '@playwright/test';
import { boot } from './helpers';
import { mockBridge } from './mock-bridge/bridge';

/** Phase 5: system status and settings against the fake engine. */

const engine = (page: Page, fn: string, ...args: unknown[]) =>
  page.evaluate(({ fn, args }) => (window.__wirescapeTest as any).engine[fn](...args), { fn, args });
const debug = (page: Page, key: string, on: boolean) =>
  page.evaluate(({ key, on }) => { (window.__wirescapeTest as any).engine.debug[key] = on; }, { key, on });

test.describe('system', () => {
  test('the motion preference starts off when the OS requests reduced motion', async ({ page }) => {
    await page.emulateMedia({ reducedMotion: 'reduce' });
    await boot(page);
    await expect(page.locator('body')).toHaveClass(/motion-off/);
  });

  test('a MIDI device plugged in and out is announced and shown', async ({ page }) => {
    await boot(page);
    await debug(page, 'extraMidiDevice', true);
    await expect(page.locator('.toast', { hasText: 'Keystation 49 connected' })).toBeVisible();
    await expect(page.getByTestId('midi-Keystation 49|midi:2')).toBeVisible();
    await debug(page, 'extraMidiDevice', false);
    await expect(page.locator('.toast', { hasText: 'Keystation 49 disconnected' })).toBeVisible();
    await expect(page.getByTestId('midi-Keystation 49|midi:2')).toHaveCount(0);
  });

  test('losing the audio device is announced, and so is its return', async ({ page }) => {
    await boot(page);
    await expect(page.getByTestId('status-audio')).toHaveText('RUNNING');
    await debug(page, 'unplugAudio', true);
    await expect(page.getByTestId('status-audio')).toHaveText('DEVICE LOST');
    await expect(page.locator('.toast', { hasText: 'Audio device lost' })).toBeVisible();
    await debug(page, 'unplugAudio', false);
    await expect(page.getByTestId('status-audio')).toHaveText('RUNNING', { timeout: 5000 });
    await expect(page.locator('.toast', { hasText: 'Audio is back' })).toBeVisible();
  });

  test('sample rate and buffer change the latency shown', async ({ page }) => {
    await boot(page);
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-rate')).toBeVisible();
    await page.getByTestId('audio-buffer').selectOption('512');
    await page.getByTestId('audio-rate').selectOption('44100');
    await expect(page.getByTestId('status-audio-detail')).toContainText('44.1 kHz · 512 frames · 23.2 ms');
  });

  test('audio settings tolerate no devices and missing optional capabilities', async ({ page }) => {
    await boot(page);
    const errors: string[] = [];
    page.on('pageerror', e => errors.push(e.message));
    await page.evaluate(() => {
      const e = window.__wirescapeTest.engine;
      e.listAudioDevices = async () => [];
    });
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-no-devices')).toBeVisible();
    await expect(page.getByTestId('audio-device')).toHaveCount(0);
    await page.getByTestId('settings-modal-close').click();
    await page.evaluate(() => {
      const e = window.__wirescapeTest.engine;
      e.listAudioDevices = async () => [{ id: 'audio:0', name: 'Minimal device', kind: 'audio' }];
      e.setSampleRate = undefined;
      e.setBufferFrames = undefined;
    });
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-device')).toBeVisible();
    await expect(page.getByTestId('audio-rate')).toHaveCount(0);
    await expect(page.getByTestId('audio-buffer')).toHaveCount(0);
    expect(errors).toEqual([]);
  });

  test('device selection refreshes its supported rates and uses contract buffer setter', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => {
      const e = window.__wirescapeTest.engine;
      const list = e.listAudioDevices.bind(e);
      e.listAudioDevices = async () => (await list()).map((d: any, i: number) => ({
        ...d, sampleRates: i === 0 ? [44100, 48000] : [96000],
      }));
      const buffer = e.setBufferFrames.bind(e);
      e.setBufferFrames = (n: number) => { e.contractBufferCalled = n; buffer(n); };
    });
    await page.getByTestId('open-settings').click();
    const second = await page.getByTestId('audio-device').locator('option').nth(1).getAttribute('value');
    await page.getByTestId('audio-device').selectOption(second!);
    await expect(page.getByTestId('audio-rate').locator('option')).toHaveCount(1);
    await expect(page.getByTestId('audio-rate').locator('option')).toHaveAttribute('value', '96000');
    await page.getByTestId('audio-buffer').selectOption('512');
    expect(await page.evaluate(() => window.__wirescapeTest.engine.contractBufferCalled)).toBe(512);
  });

  test('audio route: mock bridge disables mono and hides unsupported capability controls', async ({ page }) => {
    await mockBridge(page);
    await boot(page);
    await expect(page.getByTestId('status-engine')).toHaveText('engine: real');
    await page.evaluate(() => {
      const e = window.__wirescapeTest.engine as any;
      e.listAudioDevices = async () => [
        { id: 'USB Stereo', name: 'USB Stereo', kind: 'audio', index: 3,
          backend: 'ALSA', outputChannels: 2, supported: true, sampleRates: [48000] },
        { id: 'Mono DAC', name: 'Mono DAC', kind: 'audio', index: 4,
          backend: 'ALSA', outputChannels: 1, supported: false, unsupportedReason: 'mono-output' },
      ];
    });
    await page.getByTestId('open-settings').click();
    const options = page.getByTestId('audio-device').locator('option');
    await expect(options.filter({ hasText: 'Mono DAC' })).toBeDisabled();
    await expect(options.filter({ hasText: 'Mono DAC' })).toContainText('mono-output');
    await expect(options.filter({ hasText: 'USB Stereo' })).toContainText('ALSA · 2 channels');
    await expect(page.getByTestId('audio-route-status')).toContainText('ALSA · 2 channels');
    await expect(page.getByTestId('audio-rate')).toHaveCount(0);
    await expect(page.getByTestId('audio-buffer')).toHaveCount(0);
    const wire = await page.evaluate(() => (window as any).__mockBridge.sent as any[]);
    expect(wire.some(m => m.type === 'selectAudioDevice' && m.deviceId === 'Mono DAC')).toBe(false);
  });

  test('audio route: rate refusal names the shared-mode rate without restarting', async ({ page }) => {
    await mockBridge(page);
    await boot(page);
    await page.evaluate(() => {
      (window as any).__mockBridge.status({ reason: 'rate-unsupported' });
      window.__wirescapeTest.engine.listAudioDevices = async () => [{
        id: 'USB Stereo', name: 'USB Stereo', kind: 'audio', backend: 'Windows WASAPI',
        sampleRates: [48000], defaultSampleRate: 48000,
      }];
    });
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-rate-reason')).toHaveText('This device only runs at 48 kHz in shared mode.');
    await expect(page.getByTestId('status-audio')).toHaveText('RUNNING');
    const wire = await page.evaluate(() => (window as any).__mockBridge.sent as any[]);
    expect(wire.some(m => ['selectAudioDevice', 'setSampleRate', 'app.restartEngine'].includes(m.type))).toBe(false);
  });

  test('audio route: stale remembered identity never silently selects the first output', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => {
      const e = window.__wirescapeTest.engine as any;
      e.status.audio.deviceId = 'disconnected:old';
      e.status.audio.deviceName = 'Disconnected DAC';
      e.pushStatus();
      e.routeCalls = [];
      e.selectAudioDevice = (id: string) => { e.routeCalls.push(id); };
    });
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-route-status')).toContainText('Disconnected DAC');
    await expect(page.getByTestId('audio-device')).toHaveValue('');
    await expect(page.getByTestId('audio-rate')).toBeDisabled();
    await expect(page.getByTestId('audio-buffer')).toBeDisabled();
    expect(await page.evaluate(() => (window.__wirescapeTest.engine as any).routeCalls)).toEqual([]);
    await page.getByTestId('audio-device').selectOption('usb:1');
    await expect(page.getByTestId('audio-route-status')).toContainText('awaiting engine status');
    await expect(page.getByTestId('audio-buffer')).toBeEnabled();
    expect(await page.evaluate(() => (window.__wirescapeTest.engine as any).routeCalls)).toEqual(['usb:1']);
  });

  test('audio route: duplicate physical identities are not offered as selectable', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => {
      const e = window.__wirescapeTest.engine as any;
      e.status.audio.deviceId = 'ambiguous';
      e.status.audio.deviceName = 'Twin DAC';
      e.pushStatus();
      e.listAudioDevices = async () => [
        { id: 'ambiguous', name: 'Twin DAC A', kind: 'audio' },
        { id: 'ambiguous', name: 'Twin DAC B', kind: 'audio' },
        { id: 'other', name: 'Known output', kind: 'audio' },
      ];
    });
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-device')).toHaveValue('');
    await expect(page.getByTestId('audio-route-status')).toContainText('ambiguous');
    const duplicates = page.getByTestId('audio-device').locator('option[value="ambiguous"]');
    await expect(duplicates).toHaveCount(2);
    await expect(duplicates.nth(0)).toBeDisabled();
    await expect(duplicates.nth(1)).toBeDisabled();
  });

  test('audio route: unsupported displayed rate never triggers an implicit stream restart', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => {
      const e = window.__wirescapeTest.engine as any;
      // The route is active in this scenario; the displayed 48 kHz is not supported.
      e.status.audio.state = 'running';
      e.pushStatus();
      e.listAudioDevices = async () => [
        { id: 'hw:0', name: 'Built-in Audio', kind: 'audio', sampleRates: [96000] },
      ];
      e.rateCalls = [];
      e.setSampleRate = (hz: number) => { e.rateCalls.push(hz); };
    });
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('audio-route-status')).toContainText('Active physical route');
    await expect(page.getByTestId('audio-rate').locator('option')).toHaveCount(1);
    await expect(page.getByTestId('audio-rate')).toHaveValue('');
    expect(await page.evaluate(() => (window.__wirescapeTest.engine as any).rateCalls)).toEqual([]);
    await page.getByTestId('audio-rate').selectOption('96000');
    expect(await page.evaluate(() => (window.__wirescapeTest.engine as any).rateCalls)).toEqual([96000]);
  });

  test('OSC: off by default, local by default, refuses a bad port, shows messages', async ({ page }) => {
    await boot(page);
    await page.getByTestId('open-settings').click();
    await expect(page.getByTestId('status-osc')).toBeHidden();
    await page.getByTestId('osc-enabled').click();
    await expect(page.getByTestId('status-osc')).toHaveText('OSC 9000 · LOCAL');
    await page.getByTestId('osc-port').fill('80');
    await page.getByTestId('osc-port').dispatchEvent('change');
    await expect(page.locator('.toast', { hasText: '1024 to 65535' })).toBeVisible();
    await expect(page.getByTestId('osc-port')).toHaveValue('9000');
    await page.getByTestId('osc-port').fill('9100');
    await page.getByTestId('osc-port').dispatchEvent('change');
    await page.getByTestId('osc-network').click();
    await expect(page.getByTestId('status-osc')).toHaveText('OSC 9100 · NETWORK');
    await expect(page.getByTestId('osc-monitor').locator('.osc-line').first()).toContainText('/wirescape/', { timeout: 5000 });
  });

  test('slow frames switch the Pi profile on, once', async ({ page }) => {
    await boot(page);
    await page.evaluate(() => (window.__wirescapeTest as any).setPref({ profile: 'normal', autoProfile: true }));
    await page.keyboard.press('`');
    await page.getByTestId('debug-slowFrames').click();
    await expect(page.locator('html')).toHaveAttribute('data-profile', 'pi', { timeout: 9000 });
    await expect(page.locator('.toast', { hasText: 'Pi profile is on' })).toBeVisible();
  });

  test('panic, about and the update check', async ({ page }) => {
    await boot(page);
    await engine(page, 'noteOn', 60, 0.8);
    await page.getByTestId('panic').click();
    await expect.poll(() => page.evaluate(() => (window.__wirescapeTest as any).ticker.last?.heldNotes?.length ?? 0)).toBe(0);
    await page.getByTestId('open-settings').click();
    await page.getByTestId('check-updates').click();
    await expect(page.getByTestId('update-status')).toContainText('Up to date', { timeout: 3000 });
  });
});

