// SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0
// Requires a local Chromium instance with --remote-debugging-port. No hardware access.
import fs from 'node:fs';
import assert from 'node:assert/strict';
const file=process.argv[2];
assert(file, 'Usage: node check_comparison.mjs <html> [CDP port=9333] [output directory=/tmp]');
const output=process.argv[4]||'/tmp';
const targets=await(await fetch(`http://127.0.0.1:${process.argv[3]||9333}/json/list`)).json();
const ws=new WebSocket(targets.find(t=>t.type==='page').webSocketDebuggerUrl);
await new Promise((r,j)=>{ws.onopen=r;ws.onerror=j});
let seq=0;const waiting=new Map(),errors=[];
ws.onmessage=e=>{const m=JSON.parse(e.data);if(m.method==='Runtime.exceptionThrown')errors.push(m.params);if(m.id){const p=waiting.get(m.id);waiting.delete(m.id);m.error?p.reject(m.error):p.resolve(m.result)}};
const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++seq;waiting.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params}))});
async function ev(expression){const r=await call('Runtime.evaluate',{expression,returnByValue:true});if(r.exceptionDetails)throw r.exceptionDetails;return r.result.value;}
await call('Runtime.enable');await call('Page.enable');
await call('Emulation.setDeviceMetricsOverride',{width:1440,height:1050,deviceScaleFactor:1,mobile:false});
await call('Page.navigate',{url:'data:text/html;base64,'+Buffer.from(fs.readFileSync(file)).toString('base64')});
for(let i=0;i<50;i++){if(await ev("document.readyState==='complete'&&document.querySelectorAll('.board').length===D.boards.length"))break;await new Promise(r=>setTimeout(r,100));}
assert.deepEqual(errors,[]);
const D=await ev('D');assert(D.lifecycle||D.currentRound);
let modes=D.modes;const angles=D.angles;
let values=0,origins=0,selectors=0,staticValues=0,teValues=0;
for(const board of D.boards){
 const id=board.id;modes=board.modes||D.modes;
 await ev(`document.getElementById('tab-${id}').click()`);
 assert.deepEqual(await ev("Array.from(document.querySelectorAll('.board')).filter(b=>!b.hidden).map(b=>b.id)"),[id]);
 const fresh=board.cohorts.clean.present.filter(r=>D.currentRound?r.status==='PASS':r.origin==='新测 11-11');assert.equal(fresh.length,D.currentRound?board.cohorts.clean.present.filter(r=>r.status==='PASS').length:8);origins+=fresh.length;
 if(D.lifecycle)assert((await ev(`document.querySelector('#${id} .accounting').textContent`)).includes('本轮未重采 ELF'));
 else if(!D.unified)assert(!(await ev(`document.getElementById('${id}').textContent`)).includes('历史'));
 if(D.lifecycle)assert((await ev(`document.querySelector('#${id} .diagnostics-panel > summary').textContent`)).includes('历史 11-10'));
 for(const m of modes){
  await ev(`(()=>{for(const cls of ['memory-mode','scene-mode']){const s=document.querySelector('#${id} .'+cls);s.value='${m}';s.dispatchEvent(new Event('change'));}})()`);selectors+=2;
  for(const metric of ['free','used']){
   await ev(`(()=>{const s=document.querySelector('#${id} .memory-metric');s.value='${metric}';s.dispatchEvent(new Event('change'));})()`);
   const tables=await ev(`Array.from(document.querySelectorAll('#${id} .memory')).map(t=>({heap:t.dataset.heap,rows:Array.from(t.querySelectorAll('tbody tr')).map(tr=>({phase:tr.dataset.phase,cells:Array.from(tr.querySelectorAll('td[data-impl]')).map(c=>({value:c.querySelector('.value')?.textContent,delta:c.querySelector('.delta')?.className,text:c.textContent}))}))}))`);
   for(const table of tables)for(const tr of table.rows)for(let ai=0;ai<4;ai++)for(let ii=0;ii<2;ii++){
    const impl=['present','adapter'][ii],r=board.cohorts.clean[impl].find(r=>r.mode===m&&r.rotation===angles[ai]),cell=tr.cells[ai*2+ii];
    if(r.status!=='PASS'){assert.equal(cell.text,r.status==='FAIL'?'失败':'−');continue;}
    const key=table.heap+(tr.phase==='after_run'?'_min_free':'_free');
    const get=r=>(metric==='free'?r.memory[tr.phase][key]:r.memory[tr.phase][table.heap+'_total']-r.memory[tr.phase][key])/1024;
    assert(Math.abs(Number(cell.value)-get(r))<=0.0051);if(D.lifecycle)assert(cell.text.includes(r.origin));values++;
    const other=board.cohorts.clean.adapter.find(r=>r.mode===m&&r.rotation===angles[ai]);
    if(ii===0&&other.status==='PASS'){const diff=get(r)-get(other);assert(cell.delta.includes(diff===0?'same':((diff>0)===(metric==='free')?'up':'down')));}
   }
  }
  const scenes=await ev(`Array.from(document.querySelectorAll('#${id} .scenes tbody tr')).map(tr=>({name:tr.cells[0].textContent,cells:Array.from(tr.querySelectorAll('td[data-impl]')).map(c=>c.querySelector('.value')?.textContent)}))`);
  assert.equal(scenes.length,16);
  for(const tr of scenes)for(let ai=0;ai<4;ai++)for(let ii=0;ii<2;ii++){
   const r=board.cohorts.clean[['present','adapter'][ii]].find(r=>r.mode===m&&r.rotation===angles[ai]);
   if(r.status!=='PASS')continue;assert.equal(Number(tr.cells[ai*2+ii]),r.scene_scores.find(s=>s.name===tr.name).fps);values++;
  }
 }
 const fps=await ev(`Array.from(document.querySelectorAll('#${id} .fps tbody tr')).map(tr=>Array.from(tr.querySelectorAll('td[data-impl]')).map(c=>c.querySelector('.value')?.textContent))`);
 for(let mi=0;mi<modes.length;mi++)for(let ai=0;ai<4;ai++)for(let ii=0;ii<2;ii++){
  const r=board.cohorts.clean[['present','adapter'][ii]].find(r=>r.mode===modes[mi]&&r.rotation===angles[ai]);
  if(r.status!=='PASS')continue;assert.equal(Number(fps[mi][ai*2+ii]),r.lvgl_fps);values++;
 }
 assert(await ev(`Array.from(document.querySelectorAll('#${id} .transport td.a')).every(c=>c.textContent==='−')`));
 if(D.unified&&board.memoryAccounting){
  const actual=await ev(`Array.from(document.querySelectorAll('#${id} .static-memory tbody tr')).map(tr=>Array.from(tr.querySelectorAll('td')).slice(1).map(td=>({value:td.querySelector('.value')?.textContent,text:td.textContent,delta:td.querySelector('.delta')?.className})))`);
  const fields=[a=>a.iram_bytes,a=>a.dram_static_bytes,a=>a.sram_sections_bytes,a=>a.suite_baselines.before_init.internal_total,a=>a.suite_baselines.before_init.internal_total-a.suite_baselines.before_init.internal_free,a=>a.suite_baselines.after_init.internal_total-a.suite_baselines.after_init.internal_free];
  for(let f=0;f<fields.length;f++)for(let i=0;i<2;i++){
   const a=board.memoryAccounting[['present','adapter'][i]],cell=actual[f][i];
   if(!a){assert.equal(cell.text,'−');continue;}
   assert(Math.abs(Number(cell.value)-fields[f](a)/1024)<=0.0051);staticValues++;
  }
 }
 if(D.unified&&board.visualDiagnostic){
  const actual=await ev(`Array.from(document.querySelectorAll('#${id} .te-timing tbody tr')).map(tr=>Array.from(tr.cells).map(c=>c.textContent))`);
  const expected=board.cohorts.clean.present.filter(r=>r.te_meter);assert.equal(actual.length,expected.length);
  for(let i=0;i<actual.length;i++){assert.equal(actual[i][0],expected[i].mode);assert.equal(actual[i][1],expected[i].rotation+'°');for(const [column,key] of [[2,'wait_avg_us'],[3,'tx_avg_us']]){assert(Math.abs(Number(actual[i][column])-Number(expected[i].te_meter[key])/1000)<=0.0051);teValues++;}}
 }

 await ev("document.getElementById('collapse-all').click()");assert(await ev(`Array.from(document.querySelectorAll('#${id} details.panel')).every(p=>!p.open)`));
 await ev("document.getElementById('expand-all').click()");assert(await ev(`Array.from(document.querySelectorAll('#${id} details.panel')).every(p=>p.open)`));
}
assert.equal(origins,D.currentRound?D.boards.reduce((n,b)=>n+b.cohorts.clean.present.filter(r=>r.status==='PASS').length,0):24);assert.equal(values,D.currentRound?D.totals.clean*33:4356);
await ev("document.getElementById('tab-s3-rgb').click();document.querySelectorAll('#s3-rgb details.panel').forEach(p=>p.open=p.classList.contains('fps-panel')||p.classList.contains('memory-panel'));document.querySelector('#s3-rgb .memory-mode').value='DOUBLE_FULL';document.querySelector('#s3-rgb .memory-mode').dispatchEvent(new Event('change'));scrollTo(0,0)");
const shot=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(output+'/desktop.png',Buffer.from(shot.data,'base64'));
await call('Emulation.setDeviceMetricsOverride',{width:390,height:844,deviceScaleFactor:1,mobile:true});
for(const board of D.boards){await ev(`document.getElementById('tab-${board.id}').click()`);assert(await ev('document.documentElement.scrollWidth<=innerWidth'),board.id+' mobile overflow');}
await ev("document.getElementById('tab-s3-rgb').click()");
const mobile=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(output+'/mobile.png',Buffer.from(mobile.data,'base64'));
assert.deepEqual(errors,[]);
console.log(JSON.stringify({passed:true,values,staticValues,teValues,origins,selectors,errors:errors.length,desktop:1440,mobile:390}));
ws.close();
