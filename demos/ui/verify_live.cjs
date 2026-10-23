const {chromium}=require(process.env.DEMO_PLAYWRIGHT || 'playwright');
const assert=require('node:assert/strict');
const path=require('node:path');
const fs=require('node:fs');
(async()=>{
  const browser=await chromium.launch({channel:process.env.DEMO_BROWSER_CHANNEL || (process.platform==='win32'?'msedge':undefined),headless:true});
  try{
    const page=await browser.newPage({viewport:{width:1280,height:1500},colorScheme:'light'});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto('http://127.0.0.1:8765/');
    await page.waitForFunction(()=>document.querySelector('[data-view=notice]').textContent.includes('已连接 C++'));
    const text=name=>page.locator(`[data-view="${name}"]`).innerText();
    await page.getByRole('button',{name:'重置仿真',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=notice]').textContent.includes('已重置'));
    assert.equal(await text('completed'),'0 / 3');
    assert((await text('segment-total')).includes('6.00'));
    const oldPath=await text('path');
    await page.locator('[name=id]').fill('WEB-01');
    await page.getByRole('button',{name:'提交动态订单',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=form-result]').textContent.includes('已接受 WEB-01'));
    assert.equal(await text('completed'),'0 / 4');assert.notEqual(await text('path'),oldPath);
    assert((await text('orders')).includes('待取货'));assert.equal(await text('load'),'60.0 / 100 kg');
    await page.locator('[name=id]').fill('WEB-OVERLOAD');await page.locator('[name=quantity]').fill('2000');
    await page.getByRole('button',{name:'提交动态订单',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=form-result]').textContent.includes('WEB-OVERLOAD 已拒绝'));
    assert((await text('form-result')).includes('载重上限'));assert.equal(await text('completed'),'0 / 4');
    await page.locator('[name=id]').fill('WEB-WINDOW');await page.locator('[name=quantity]').fill('5');
    await page.locator('[name=destination]').selectOption('D25');await page.locator('[name=latest]').fill('08:01');
    await page.getByRole('button',{name:'提交动态订单',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=form-result]').textContent.includes('WEB-WINDOW 已拒绝'));
    assert((await text('form-result')).includes('时间窗口'));
    await page.locator('[data-field=weights]').check();await page.locator('[data-field=road]').selectOption('E003');
    await page.getByRole('button',{name:'单步 +1 分钟',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=clock]').textContent==='08:01:00');
    assert((await text('segment-total')).includes('6.00'));assert((await text('remaining')).includes('5.00'));
    await page.getByRole('button',{name:'开始仿真',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=clock]').textContent!=='08:01:00');
    await page.getByRole('button',{name:'暂停仿真',exact:true}).click();
    await page.waitForFunction(()=>!document.querySelector('[type=submit]').disabled);
    const paused=await text('clock');await page.waitForTimeout(400);assert.equal(await text('clock'),paused);
    await page.locator('[name=id]').fill('');await page.locator('[name=quantity]').fill('15');await page.locator('[name=latest]').fill('12:00');
    // Verify automatic IDs and a second insertion while already in transit.
    await page.locator('[name=destination]').selectOption('D04');
    await page.getByRole('button',{name:'提交动态订单',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=form-result]').textContent.includes('已接受 MANUAL'));
    assert.equal(await text('completed'),'0 / 5');
    for(const width of [1280,1024,390,320]){
      await page.setViewportSize({width,height:1500});await page.waitForTimeout(100);
      const sizes=await page.evaluate(()=>({scroll:document.documentElement.scrollWidth,width:innerWidth}));
      assert(sizes.scroll<=sizes.width,`overflow at ${width}`);
      await page.screenshot({path:path.join(__dirname,`live-preview-${width}.png`),fullPage:true});
    }
    // Exercise the same HTTP adapter through to actual completed deliveries.
    let payload;
    for(let i=0;i<100;i++){
      const response=await page.request.post('http://127.0.0.1:8765/api/command',{data:{cmd:'advance',minutes:5}});
      payload=await response.json();assert(payload.ok);assert(payload.frame.load<=100);
      if(payload.frame.finished)break;
    }
    assert(payload.frame.finished);assert.equal(payload.frame.delivered_count,5);assert.equal(payload.frame.load,0);
    assert(payload.orders.find(o=>o.id==='WEB-01').completed!==null);
    await page.reload();await page.waitForFunction(()=>document.querySelector('[data-view=completed]').textContent==='5 / 5');
    assert.equal(await text('position'),'W01');
    const external=await page.request.post('http://127.0.0.1:8765/api/command',{data:{cmd:'reset'},headers:{Origin:'https://example.com'}});
    assert.equal(external.status(),403);
    assert.deepEqual(errors,[]);
    // Leave the user's demo in a clean initial state.
    await page.getByRole('button',{name:'重置仿真',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=completed]').textContent==='0 / 3');
    const result={passed:true,checks:['real C++ manual insertion','route changes','pickup precedes loading','overload rejection','window rejection','automatic order IDs','play/pause','locked entry duration','5 accepted orders delivered and returned','page refresh retains server state','local origin enforcement','320/390/1024/1280 layout','no JavaScript errors']};
    fs.writeFileSync(path.join(__dirname,'live-verification.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
  }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});

