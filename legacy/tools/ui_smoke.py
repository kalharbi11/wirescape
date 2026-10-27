"""Regression browser test, no real Surge or audio hardware needed.

Run: python tools/ui_smoke.py --chromium /usr/bin/chromium
Browsers can be provided via Playwright's default installation on Windows.
"""
import argparse
import asyncio
import json
from pathlib import Path
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from wirescape.server import schema_payload
from playwright.async_api import async_playwright

async def run(screenshot=None,chromium=None):
    template=(ROOT/'web'/'index.html').read_text(encoding='utf8').replace('<script src="client.js" defer></script>','')
    schema=json.dumps(schema_payload())
    ws_mock='''window.__wirescape_sent=[];window.__wirescape_ws=null;
    window.WebSocket=class MockWebSocket {
      static OPEN=1;
      constructor(url){this.readyState=1;window.__wirescape_ws=this;setTimeout(()=>{
        this.onopen?.();this.onmessage?.({data:JSON.stringify({
          type:'hello',version:'0.4.0',surge_ready:true,surge_error:'',
          status:{running:false},schema:__SCHEMA__})});},25);}
      send(msg){window.__wirescape_sent.push(JSON.parse(msg));}
    };'''.replace('__SCHEMA__',schema)
    async with async_playwright() as p:
        kw={'headless':True,'args':['--no-sandbox']}
        if chromium:kw['executable_path']=chromium
        browser=await p.chromium.launch(**kw)
        page=await browser.new_page(viewport={'width':1920,'height':1080})
        errors=[]
        page.on('pageerror',lambda err:errors.append(str(err)))
        try:
            await page.set_content(template,wait_until='load')
            await page.add_script_tag(content=ws_mock)
            await page.add_script_tag(content=(ROOT/'web'/'client.js').read_text(encoding='utf8'))
            await page.wait_for_function("document.querySelector('#engineBadge')?.textContent==='SURGE / READY'")
            knobs=page.locator('.module[data-type="chaos"] .knob')
            assert await knobs.count()==30, f'Expected 30 live CHAOS knobs, got {await knobs.count()}'
            assert await page.get_by_role('button',name='FOCUS',exact=True).count()==0
            assert await page.locator('[data-presets-for="node-2"] option').count()>=6
            # Front knob still sends DSP parameter control and visually changes.
            fold=page.locator('.module[data-id="node-2"] .knob[aria-label="FOLD"]')
            await fold.focus();await page.keyboard.press('End')
            last=await page.evaluate('window.__wirescape_sent.slice(-1)[0]')
            assert last=={'type':'control','id':'node-2','name':'FOLD','value':100},last
            # Expanded editor stays bound to CHAOS even when sliders have focus.
            await page.get_by_role('button',name='OPEN ENGINE ↗').click()
            await page.get_by_role('button',name='RAW A',exact=True).click()
            filters=page.locator('.deep-body .param-group').filter(has=page.locator('summary:text-is("FILTERS")'))
            await filters.locator('summary').click()
            type_select=filters.locator('select[aria-label="Filter 1 Type"]')
            assert await type_select.count()==1,'Filter 1 Type must be a dropdown'
            options=await type_select.locator('option').count()
            assert options==36, f'36 filter categories, got {options}'
            await type_select.select_option('10')
            last=await page.evaluate('window.__wirescape_sent.slice(-1)[0]')
            assert last['type']=='raw' and last['name']=='Filter 1 Type' and abs(last['value']-10/35)<1e-6,last
            # Even with a type-dropdown in focus, musical A/W/S keys still play.
            await type_select.focus()
            prev=await page.evaluate('window.__wirescape_sent.length')
            await page.keyboard.down('a');await page.keyboard.up('a')
            fired=await page.evaluate('(start)=>window.__wirescape_sent.slice(start).filter(x=>x.type==="note")',prev)
            assert [x['on'] for x in fired]==[True,False],fired
            # A range slider never hijacks musical keyboard notes either.
            await filters.locator('input[aria-label="Filter 1 Cutoff"]').focus()
            before=await page.evaluate('window.__wirescape_sent.length')
            await page.keyboard.down('a');await page.keyboard.up('a')
            diff=await page.evaluate('(start)=>window.__wirescape_sent.slice(start)',before)
            noteMsgs=[x for x in diff if x['type']=='note']
            assert noteMsgs==[{'type':'note','id':'node-2','note':60,'on':True},
                              {'type':'note','id':'node-2','note':60,'on':False}],noteMsgs
            # Multisynth keyboard routing: arm both, then exclusive solo the second.
            await page.evaluate('window.__wirescapeTest.addFromLibrary("chaos")')
            second=await page.evaluate('window.__wirescapeTest.state().modules.filter(m=>m.type==="chaos").at(-1).id')
            await page.evaluate('(id)=>document.querySelector(`[data-id="${id}"] [data-keyboard="arm"]`).click()',second)
            before=await page.evaluate('window.__wirescape_sent.length')
            await page.keyboard.down('s');await page.keyboard.up('s')
            notes=await page.evaluate('(start)=>window.__wirescape_sent.slice(start).filter(x=>x.type==="note")',before)
            assert {x['id'] for x in notes}=={'node-2',second},notes
            await page.evaluate('(id)=>document.querySelector(`[data-id="${id}"] [data-keyboard="solo"]`).click()',second)
            before=await page.evaluate('window.__wirescape_sent.length')
            await page.keyboard.down('d');await page.keyboard.up('d')
            notes=await page.evaluate('(start)=>window.__wirescape_sent.slice(start).filter(x=>x.type==="note")',before)
            assert len(notes)==2 and {x['id'] for x in notes}=={second},notes
            # Loading a user sound restores mode, knob, and raw state in patch.
            await page.evaluate('''() => window.__wirescape_ws.onmessage({data:JSON.stringify({
              type:'preset_loaded',id:'node-2',preset:{
              values:{FOLD:42,A_ATTACK:55,B_RELEASE:87},advanced:{BREAK:0.7},
              raw:{'A|Filter 1 Type':10/35},mode:1}})})''')
            state=await page.evaluate('window.__wirescapeTest.state()')
            c=next(m for m in state['modules'] if m['id']=='node-2')
            assert c['values']['A_ATTACK']==55 and c['mode']==1 and 'A|Filter 1 Type' in c['raw']
            # Double click any module instead of a dedicated Focus button.
            before=await page.locator('#zoomDisplay').inner_text()
            await page.evaluate('''() => document.querySelector('[data-id="node-2"] .module-title').dispatchEvent(
                new MouseEvent('dblclick',{bubbles:true}))''')
            after=await page.locator('#zoomDisplay').inner_text()
            assert after and after!=before,(before,after)
            assert not errors,errors
            if screenshot:
                # ensure visible enlarged module is centered in viewport
                await page.screenshot(path=str(screenshot),full_page=True)
            return {'pass':True,'front_knobs':30,'typed_filter_options':36,'keyboard_in_deep_editor':True,
                    'multiple_keyboard_targets':True,'solo_keyboard':True,'preset_snapshot':True,
                    'double_click_focus':True,'browser_errors':errors}
        finally:await browser.close()


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--screenshot',type=Path)
    parser.add_argument('--chromium',default=None)
    args=parser.parse_args()
    try:result=asyncio.run(run(args.screenshot,args.chromium))
    except Exception as exc:result={'pass':False,'error':repr(exc)}
    print(json.dumps(result,indent=2))
    return 0 if result['pass'] else 1

if __name__=='__main__':sys.exit(main())
