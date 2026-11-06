import { test, expect, type Page } from '@playwright/test';
import { boot } from './helpers';

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
    await expect(page.getByTestId('midi-midi:2')).toBeVisible();
    await debug(page, 'extraMidiDevice', false);
    await expect(page.locator('.toast', { hasText: 'Keystation 49 disconnected' })).toBeVisible();
    await expect(page.getByTestId('midi-midi:2')).toHaveCount(0);
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

