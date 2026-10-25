const {chromium}=require(process.env.DEMO_PLAYWRIGHT || 'playwright');
const assert=require('node:assert/strict');
const path=require('node:path');
(async()=>{
 const browser=await chromium.launch({channel:process.platform==='win32'?'msedge':undefined,headless:true});
 try {
  const page=await browser.newPage({viewport:{width:1440,height:1100}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const call=async(cmd,extra={})=>{const d=await(await page.request.post('http://127.0.0.1:8765/api/command',{data:{cmd,...extra}})).json();assert(d.ok,d.error);return d;};
  await call('reset');await page.goto('http://127.0.0.1:8765/');
  await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('已连接'));
  assert.equal(await page.locator('.vehicle-card').count(),2);
  assert((await page.locator('#network-count').innerText()).includes('82'));
  const points=await page.locator('[name=warehouse] option').evaluateAll(list=>list.map(o=>o.value));
  assert.deepEqual([...points].sort(),['T01','T02','T03','W01','W02']);
  await page.locator('[name=warehouse]').selectOption('T02');await page.locator('[name=destination]').selectOption('D18');
  await page.locator('[name=id]').fill('HUB-DEMO');await page.locator('[name=latest]').fill('12:00');
  await page.getByRole('button',{name:'提交并自动分配',exact:true}).click();
  await page.waitForFunction(()=>document.getElementById('form-result').textContent.includes('已接受 HUB-DEMO'));
  let state=await(await page.request.get('http://127.0.0.1:8765/api/state')).json();const order=state.orders.find(o=>o.id==='HUB-DEMO');
  assert(order.accepted && order.pickup_node_id==='T02');
  assert((await page.locator(`[data-vehicle=${order.vehicle_id}]`).innerText()).includes('取货 · HUB-DEMO（T02）'));
  assert((await page.locator('#pickup-points').innerText()).includes('T02 1 单'));
  assert.equal(state.latest_insertion.id,'HUB-DEMO');
  await page.reload();await page.waitForFunction(()=>document.getElementById('connection').textContent.includes('已连接'));
  assert((await page.locator('#replan-detail').innerText()).includes('插入 HUB-DEMO'));
  await page.screenshot({path:path.resolve('docs/maps/ui-hub.png'),fullPage:true});
  for(let i=0;i<150&&!state.finished;i++)state=await call('advance',{minutes:5});
  assert(state.finished);assert(state.vehicles.every(v=>v.load===0&&v.node_id==='W01'&&v.tasks.length===0));
  assert.deepEqual(state.pickup_points.find(p=>p.node_id==='T02').waiting_orders,[]);
  const actual=state.orders.find(o=>o.id==='HUB-DEMO');assert(actual.pickup!==null&&actual.completed!==null);
  assert.deepEqual(errors,[]);
  await call('reset');await call('insert',{order:{id:'HUB-DEMO',pickup_node_id:'T02',destination:'D18',quantity:5,earliest:480,latest:720,priority:'urgent'}});
  console.log('PASS: formal fleet hub pickup, station counts, refresh retains route comparison, physical delivery and return');
 }finally{await browser.close()}
})().catch(e=>{console.error(e);process.exit(1)});
