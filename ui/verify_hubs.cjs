// Real-browser verification of the approved road layout and station pickup form.
const {chromium}=require(process.env.DEMO_PLAYWRIGHT || 'playwright');
const assert=require('node:assert/strict');
const path=require('node:path');
const {pathToFileURL}=require('node:url');
(async()=>{
 const browser=await chromium.launch({channel:process.platform==='win32'?'msedge':undefined,headless:true});
 try {
  const page=await browser.newPage({viewport:{width:1280,height:1400}});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto('http://127.0.0.1:8765/');
  await page.waitForFunction(()=>document.querySelector('[data-view=notice]').textContent.includes('已连接'));
  await page.getByRole('button',{name:'重置仿真',exact:true}).click();
  await page.waitForFunction(()=>document.querySelector('[data-view=notice]').textContent.includes('已重置'));
  assert((await page.locator('[data-view=network-count]').innerText()).includes('82'));
  const pickups=await page.locator('[name=warehouse] option').evaluateAll(list=>list.map(o=>o.value));
  assert.deepEqual([...pickups].sort(),['T01','T02','T03','W01','W02']);
  await page.locator('[name=warehouse]').selectOption('T02');
  await page.locator('[name=destination]').selectOption('D18');
  await page.locator('[name=id]').fill('HUB-DEMO');
  await page.locator('[name=quantity]').fill('5');
  await page.locator('[name=latest]').fill('12:00');
  await page.getByRole('button',{name:'提交动态订单',exact:true}).click();
  await page.waitForFunction(()=>document.querySelector('[data-view=form-result]').textContent.includes('已接受 HUB-DEMO'));
  assert((await page.locator('[data-view=task-path]').innerText()).includes('取货 · HUB-DEMO（T02）'));
  assert((await page.locator('[data-view=pickup-points]').innerText()).includes('T02 1 单'));
  let state=await (await page.request.get('http://127.0.0.1:8765/api/state')).json();
  assert.equal(state.orders.find(o=>o.id==='HUB-DEMO').pickup,null);
  await page.waitForFunction(()=>!document.querySelector('#logistics-preview [type=submit]').disabled);
  await page.screenshot({path:path.resolve('docs/maps/ui-hub.png'),fullPage:true});
  for(let i=0;i<150 && !state.frame.finished;i++){
   state=await (await page.request.post('http://127.0.0.1:8765/api/command',{data:{cmd:'advance',minutes:5}})).json();
   assert(state.ok,state.error);
  }
  const record=state.orders.find(o=>o.id==='HUB-DEMO');
  assert(record.pickup!==null && record.completed!==null);
  assert.equal(state.frame.node_id,'W01');
  assert.equal(state.frame.load,0);
  assert.deepEqual(state.pickup_points.find(p=>p.node_id==='T02').waiting_orders,[]);
  assert.deepEqual(errors,[]);
  const map=await browser.newPage({viewport:{width:1600,height:1100},deviceScaleFactor:1.5});
  await map.goto(pathToFileURL(path.resolve('docs/maps/network.svg')).href);
  await map.evaluate(()=>document.fonts.ready);
  await map.locator('svg').screenshot({path:path.resolve('docs/maps/network.png')});
  // Leave the app in a useful review state with a station pickup awaiting collection.
  await page.request.post('http://127.0.0.1:8765/api/command',{data:{cmd:'reset'}});
  await page.request.post('http://127.0.0.1:8765/api/command',{data:{cmd:'insert',order:{id:'HUB-DEMO',pickup_node_id:'T02',destination:'D18',quantity:5,earliest:480,latest:720,priority:'urgent'}}});
  console.log('PASS: new road layout, station selector, real station pickup/delivery, waiting list, home return and screenshots');
 } finally {await browser.close();}
})().catch(e=>{console.error(e);process.exit(1);});
