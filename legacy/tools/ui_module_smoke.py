"""Browser smoke test: custom folder-module loads without modifying main HTML per plugin.

No device, Surge, localhost browser navigation, or network required.
Requires Playwright Chromium. Run: python tools/ui_module_smoke.py
"""
import asyncio
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from wirescape import extensions
from modules.sequencer.plugin import register
from wirescape.schema import DEFINITIONS
from wirescape.server import schema_payload
from playwright.async_api import async_playwright

async def test():
    if 'sequencer' not in DEFINITIONS:register()
    extensions.ACTIVE['sequencer']=extensions.scan()['sequencer']
    schema=schema_payload()
    html=(ROOT/'web'/'index.html').read_text().replace('<script src="client.js" defer></script>','')
    html=html.replace('<head>','<head><base href="http://wirescape.example/">',1)
    stub='''window.__wirescape_sent=[];window.WebSocket=class FakeWebSocket {
        static OPEN=1;constructor(){this.readyState=1;window.__mockws=this;
           setTimeout(()=>{this.onopen?.();this.onmessage?.({data:JSON.stringify({type:'hello',version:'0.5.0',
             surge_ready:true,surge_error:'',status:{running:false},schema:__SCHEMA__})})},25)}
        send(payload){window.__wirescape_sent.push(JSON.parse(payload));}
    };'''.replace('__SCHEMA__',json.dumps(schema))
    async with async_playwright() as pw:
        browser=await pw.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
        page=await browser.new_page(viewport={'width':1600,'height':1000})
        errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
        async def asset(route):
            name=route.request.url.rsplit('/',1)[-1].split('?')[0]
            if name in ('editor.css','editor.js'):
                typ='text/css' if name.endswith('.css') else 'application/javascript'
                await route.fulfill(status=200,body=(ROOT/'modules'/'sequencer'/name).read_bytes(),content_type=typ)
            else:await route.abort()
        await page.route('**/module-assets/**',asset)
        await page.set_content(html,wait_until='load')
        await page.add_script_tag(content=stub)
        await page.add_script_tag(content=(ROOT/'web'/'client.js').read_text())
        await page.wait_for_selector('[data-add=sequencer]')
        await page.evaluate('window.__wirescapeTest.addFromLibrary("sequencer")')
        await page.wait_for_selector('.seq-pads')
        assert await page.locator('.seq-pad').count()==16
        state=await page.evaluate('window.__wirescapeTest.state()')
        s=[m for m in state['modules'] if m['type']=='sequencer']
        assert len(s)==1 and len(s[0]['module_state']['steps'])==16
        await page.locator('.seq-pad').first.click()
        await page.wait_for_selector('.seq-step-tab')
        assert await page.locator('#deepBody select[aria-label="ROOT"]').count()==1
        await page.locator('#deepBody select[aria-label="ROOT"]').select_option('7')
        await page.wait_for_timeout(50)
        cur=await page.evaluate('window.__wirescapeTest.state()')
        changed=next(m for m in cur['modules'] if m['type']=='sequencer')['module_state']['steps'][0]['root']
        assert changed==7
        sent=await page.evaluate('window.__wirescape_sent')
        assert any(m['type']=='module_state' and m['state']['steps'][0]['root']==7 for m in sent)
        assert not errors,errors
        (ROOT/'build_reports').mkdir(exist_ok=True)
        await page.screenshot(path=str(ROOT/'build_reports/module_ui.png'),full_page=True)
        print(json.dumps({'pass':True,'module_asset_loaded':True,'step_pads':16,
                          'state_saved':True,'state_message_sent':True,'js_errors':errors},indent=2))
        await browser.close()

if __name__=='__main__':asyncio.run(test())
