const {chromium}=require(process.env.DEMO_PLAYWRIGHT || 'playwright');
const assert=require('node:assert/strict');
const path=require('node:path');
const fs=require('node:fs');
(async()=>{
  const browser=await chromium.launch({channel:process.env.DEMO_BROWSER_CHANNEL || (process.platform==='win32'?'msedge':undefined),headless:true});
  try{
    const page=await browser.newPage({viewport:{width:1280,height:1400},colorScheme:'light'});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto('file:///'+path.join(__dirname,'dist/replay.html').replaceAll('\\','/'));
    const text=name=>page.locator(`[data-view="${name}"]`).innerText();
    assert.equal(await text('clock'),'08:00:00');
    assert.equal(await text('completed'),'0 / 3');
    assert.equal(await page.locator('.lp-map svg circle,.lp-map svg rect,.lp-map svg polygon').count(),31);
    await page.getByRole('button',{name:'08:02 紧急插单',exact:true}).click();
    assert.equal(await text('completed'),'0 / 4');
    assert((await text('orders')).includes('待取货'));
    assert((await text('events')).includes('接受紧急订单'));
    await page.getByRole('button',{name:'08:03 超载拒单',exact:true}).click();
    assert((await text('events')).includes('拒绝订单 OVERLOAD01'));
    await page.getByRole('button',{name:'08:04 窗口拒单',exact:true}).click();
    assert((await text('events')).includes('拒绝订单 IMPOSSIBLE01'));
    await page.getByRole('button',{name:'配送结束',exact:true}).click();
    assert.equal(await text('clock'),'10:56:08');assert.equal(await text('completed'),'4 / 4');
    assert.equal(await text('status'),'配送完成');assert.equal(await text('penalty'),'0');
    assert.equal(await text('position'),'W01');assert.equal(await text('load'),'0 / 100 kg');
    await page.getByRole('button',{name:'载入示例 / 重置',exact:true}).click();
    await page.getByText('＋ 新增订单 · 表单预览',{exact:true}).click();
    await page.locator('[name=quantity]').fill('2000');
    await page.getByRole('button',{name:'校验输入',exact:true}).click();
    assert((await text('form-result')).includes('超过车辆上限'));
    await page.locator('[name=quantity]').fill('15');
    await page.locator('[name=earliest]').fill('12:00');await page.locator('[name=latest]').fill('08:00');
    await page.getByRole('button',{name:'校验输入',exact:true}).click();
    assert((await text('form-result')).includes('结束时间必须不早于开始时间'));
    await page.locator('[name=earliest]').fill('08:00');await page.locator('[name=latest]').fill('12:00');
    await page.getByRole('button',{name:'校验输入',exact:true}).click();
    assert((await text('form-result')).includes('本次未提交订单'));
    await page.locator('[data-field=road]').selectOption('E003');
    assert((await text('road-detail')).includes('W01 → D06'));
    await page.locator('[data-field=weights]').check();
    await page.getByRole('button',{name:'路线高亮：开',exact:true}).click();
    assert.equal(await page.locator('[data-action=route]').getAttribute('aria-pressed'),'false');
    await page.getByRole('button',{name:'路线高亮：关',exact:true}).click();
    await page.getByRole('button',{name:'单步 +1 分钟',exact:true}).click();
    assert.equal(await text('clock'),'08:01:00');
    await page.getByRole('button',{name:'开始回放',exact:true}).click();
    await page.waitForFunction(()=>document.querySelector('[data-view=clock]').textContent!=='08:01:00');
    await page.getByRole('button',{name:'暂停回放',exact:true}).click();
    const paused=await text('clock');await page.waitForTimeout(350);assert.equal(await text('clock'),paused);
    // Rewind must also rewind the road history and order statuses.
    await page.locator('[data-field=timeline]').fill('0');
    assert.equal(await text('clock'),'08:00:00');assert.equal(await text('completed'),'0 / 3');
    await page.getByText('＋ 新增订单 · 表单预览',{exact:true}).click();
    await page.locator('[data-field=road]').selectOption('');await page.locator('[data-field=weights]').uncheck();
    await page.getByRole('button',{name:'08:05 拥堵调整',exact:true}).click();
    for(const width of [1280,1024,390,320]){
      await page.setViewportSize({width,height:1400});
      await page.waitForTimeout(100);
      const sizes=await page.evaluate(()=>({scroll:document.documentElement.scrollWidth,width:innerWidth,map:document.querySelector('.lp-map').clientWidth}));
      assert(sizes.scroll<=sizes.width,`horizontal overflow at ${width}: ${JSON.stringify(sizes)}`);
      await page.screenshot({path:path.join(__dirname,`preview-${width}.png`),fullPage:true});
    }
    await page.emulateMedia({colorScheme:'dark'});
    await page.setViewportSize({width:1024,height:1400});await page.screenshot({path:path.join(__dirname,'preview-dark.png'),fullPage:true});
    assert.deepEqual(errors,[]);
    const result={passed:true,checks:['offline page','real insertion/rejections','final totals','rewind','play/pause','step','form validation','road inspection','route toggle','weights','320/390/1024/1280 layout','dark mode','no JavaScript errors']};
    fs.writeFileSync(path.join(__dirname,'verification.json'),JSON.stringify(result,null,2));console.log(JSON.stringify(result));
  }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});

