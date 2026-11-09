// SPDX-License-Identifier: GPL-3.0-only
// Actual Electron dialogs, preload and file writes; device-free protocol fixture.
const { test, expect } = require('../../studio/node_modules/@playwright/test');
const { _electron: electron } = require('../../studio/node_modules/playwright');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const main = path.resolve(__dirname, '../build/main.js');

test('draft and playing views never inherit one another\'s native Save target', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'wirescape-draft-files-'));
  const aFile = path.join(dir, 'A.json'), bFile = path.join(dir, 'B.json');
  for (const folder of ['user', 'session']) fs.mkdirSync(path.join(dir, folder));
  const patch = marker => ({ format: 'wirescape.native.patch', version: 2,
    modules: [], connections: [], marker, opaque: { keep: marker } });
  fs.writeFileSync(aFile, JSON.stringify(patch('A')));
  fs.writeFileSync(bFile, JSON.stringify(patch('B')));
  const stub = path.join(dir, 'engine.cjs'), wrapper = path.join(dir, 'app.cjs');
  fs.writeFileSync(stub, `const readline = require('node:readline');
const session = 'files-' + process.pid; let gen = 0, refused = false;
const send = x => process.stdout.write(JSON.stringify({v:0,session,...x})+'\\n');
send({type:'hello',protocol:0,engine:{version:'test',sha:'fixture'},capabilities:[],
 catalogue:{modules:[]},audio:{state:'no-device',deviceId:null,deviceName:null,
 sampleRate:48000,bufferFrames:256,latencyMs:0,cpuPercent:0,dropouts:0},midi:{devices:[],activity:{}}});
const input=readline.createInterface({input:process.stdin});
input.on('line',line=>{const c=JSON.parse(line);
 if(c.type==='loadGraph'&&c.patch.marker==='B'&&!refused){refused=true;send({re:c.id,ok:false,code:'busy',message:'fixture refusal',gen:String(gen)});return;}
 if(c.type==='loadGraph')gen++;send({re:c.id,ok:true,gen:String(gen)});});
input.on('close',()=>process.exit(0));\n`);
  fs.writeFileSync(wrapper, `const {app}=require('electron');
app.setPath('userData',${JSON.stringify(path.join(dir, 'user'))});
app.setPath('sessionData',${JSON.stringify(path.join(dir, 'session'))});
require(${JSON.stringify(main)});\n`);
  const env = { ...process.env, WIRESCAPE_TEST_STUB: '1' }; delete env.ELECTRON_RUN_AS_NODE;
  let app;
  try {
    app = await electron.launch({ executablePath: require('electron'),
      args: ['--no-sandbox', wrapper, '--engine', stub, '--null-audio', '--settings', path.join(dir, 'settings.json')], env });
    const page = await app.firstWindow();
    await expect(page.locator('body')).toHaveAttribute('data-ready', 'true');
    const choose = async file => app.evaluate(({ dialog }, filename) => {
      dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [filename] });
      dialog.showSaveDialog = async () => {
        globalThis.__saveDialogs = (globalThis.__saveDialogs ?? 0) + 1;
        return { canceled: false, filePath: filename };
      };
    }, file);
    await choose(aFile); await page.getByTestId('open-patch').click();
    await expect(page.getByTestId('patch-name')).toHaveText('A.json');
    await choose(bFile); await page.getByTestId('open-patch').click();
    await expect(page.getByTestId('open-refused-draft')).toBeVisible();
    await page.getByTestId('open-refused-draft').click();
    await page.getByTestId('save-patch').click();
    await expect.poll(() => app.evaluate(() => globalThis.__saveDialogs)).toBe(1);
    await expect(page.getByTestId('draft-not-running')).toBeVisible();
    await expect.poll(() => fs.readFileSync(bFile, 'utf8')).toContain('  \"marker\": \"B\"');
    const bSaved = fs.readFileSync(bFile, 'utf8');
    expect(JSON.parse(bSaved).marker).toBe('B');
    await page.getByTestId('back-to-playing').click();
    await choose(aFile); await page.getByTestId('save-patch').click();
    await expect.poll(() => app.evaluate(() => globalThis.__saveDialogs)).toBe(2);
    await expect(page.getByTestId('patch-name')).toHaveText('A.json');
    expect(fs.readFileSync(bFile, 'utf8')).toBe(bSaved);
    const aSaved = fs.readFileSync(aFile, 'utf8');
    expect(JSON.parse(aSaved).marker).toBe('A');
    await page.getByTestId('open-refused-draft').click();
    await choose(bFile); await page.getByTestId('save-patch').click();
    await expect.poll(() => app.evaluate(() => globalThis.__saveDialogs)).toBe(3);
    expect(fs.readFileSync(aFile, 'utf8')).toBe(aSaved);
    expect(JSON.parse(fs.readFileSync(bFile, 'utf8')).opaque).toEqual({ keep: 'B' });
  } finally { if (app) await app.close(); fs.rmSync(dir, { recursive: true, force: true }); }
});
