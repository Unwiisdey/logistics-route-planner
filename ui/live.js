(() => {
  const root = document.getElementById('logistics-preview');
  const view = name=>root.querySelector(`[data-view="${name}"]`);
  const field = name=>root.querySelector(`[data-field="${name}"]`);
  const action = name=>root.querySelector(`[data-action="${name}"]`);
  const set = (name,value)=>{if(view(name))view(name).textContent=value;};
  const esc = value=>String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const clock = value=>{if(value===null||value===undefined)return '—';const s=Math.round(value*60);return `${String(Math.floor(s/3600)).padStart(2,'0')}:${String(Math.floor(s/60)%60).padStart(2,'0')}:${String(s%60).padStart(2,'0')}`;};
  const hm=value=>clock(value).slice(0,5);
  const taskLabel=task=>task.kind==='return'?`返仓 · ${task.node_id}`:`${task.kind==='pickup'?'取货':'送货'} · ${task.order_id}${task.kind==='pickup'?`（${task.node_id}）`:''}`;
  const names={driving:'行驶中',waiting:'等待窗口',unloading:'卸货中',idle:'空闲'};
  const data={nodes:[],edges:[],orders:[],traversals:[]},nodes=new Map(),edges=new Map();
  const state={highlight:true,road:'',weights:false,running:false,timer:null,busy:false,current:null};
  let pending=Promise.resolve();
  function stop(){state.running=false;if(state.timer)clearTimeout(state.timer);state.timer=null;action('play').textContent='开始仿真';}
  function controls(){for(const key of ['reset','step'])action(key).disabled=state.busy||!state.current;action('play').disabled=!state.current;root.querySelector('[type=submit]').disabled=state.busy||!state.current;}
  function reason(text){
    if(text.includes('pickup would exceed'))return '无法插入：取货会超过车辆载重上限。';
    if(text.includes('time window missed'))return '无法插入：新订单或已有订单将错过时间窗口。';
    if(text.includes('duplicate order ID'))return '订单编号已存在，请更换编号。';
    if(text.includes('unreachable'))return '无法到达取货或配送地点。';
    return text;
  }
  async function command(body){
    const run=async()=>{
      state.busy=true;controls();
      try{
        const response=await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
        const payload=await response.json();if(!response.ok||!payload.ok)throw new Error(payload.error||'请求失败');
        apply(payload);return payload;
      }catch(error){stop();set('notice',`操作未完成：${error.message}`);throw error;}
      finally{state.busy=false;controls();}
    };
    const result=pending.then(run,run);pending=result.catch(()=>{});return result;
  }
  const option=(value,text)=>{const e=document.createElement('option');e.value=value;e.textContent=text;return e;};
  function apply(payload){
    state.current=payload;Object.assign(data,payload);nodes.clear();edges.clear();
    data.nodes.forEach(n=>nodes.set(n.id,n));data.edges.forEach(e=>edges.set(e.id,e));
    if(!root.querySelector('[name=warehouse]').options.length){
      [...nodes.values()].sort((a,b)=>a.id.localeCompare(b.id)).forEach(n=>{if(n.type==='warehouse'||n.type==='transfer_station')root.querySelector('[name=warehouse]').append(option(n.id,`${n.id}（${n.type==='warehouse'?'仓库':'中转站'}）`));if(n.type==='delivery_point')root.querySelector('[name=destination]').append(option(n.id,n.id));});
      root.querySelector('[name=destination]').value='D05';
      [...edges.values()].sort((a,b)=>a.id.localeCompare(b.id)).forEach(e=>field('road').append(option(e.id,`${e.id} · ${e.from_id}→${e.to_id}`)));
    }
    render();if(payload.frame.finished){stop();set('notice','所有已接订单完成，已返仓。仍可手动提交新订单。');}
  }
  /*DRAW_NETWORK*/
  function render(){
    const payload=state.current;if(!payload)return;const f=payload.frame,segment=payload.segment;
    set('clock',clock(f.time));set('network-count',`${nodes.size} 个节点 · ${edges.size} 条有向道路`);set('status',f.finished?'配送完成':names[f.status]);
    set('position',f.node_id||`${f.from_id} → ${f.to_id}`);set('remaining',segment?`${segment.remaining.toFixed(2)} 分钟`:'—');
    set('segment-total',segment?`${segment.total.toFixed(2)} 分钟（入边时锁定）`:'—');
    set('road-now',segment?`${edges.get(segment.id).time.toFixed(2)} 分钟`:'—');
    set('segment-arrival',segment?clock(segment.arrival_at):'—');
    set('last-trip',payload.last_trip?`${payload.last_trip.road_id}：${payload.last_trip.actual_minutes.toFixed(2)} 分钟`:'尚未完成道路');
    set('next',f.next_order||'—');set('eta',clock(f.next_arrival));set('return',clock(f.estimated_return));
    set('load',`${f.load.toFixed(1)} / ${data.capacity} kg`);view('load-bar').style.width=`${f.load/data.capacity*100}%`;root.querySelector('.lp-load').setAttribute('aria-valuenow',f.load);
    set('completed',`${f.delivered_count} / ${data.orders.filter(o=>o.accepted).length}`);set('late',f.late_count);set('penalty',f.penalty_points);set('revision',`生效路线 v${f.route_revision}`);set('congestion',`拥堵 ${data.edges.filter(e=>e.traffic_status==='congested').length} 条`);
    root.querySelector('.lp-footer > span').textContent=`C++ 实时交互 · 单车 / 载重 ${data.capacity} kg / 迟到分钟向上取整 × ${payload.penalty_rule.rate_per_minute} 分/分钟`;
    action('route').textContent=`路线高亮：${state.highlight?'开':'关'}`;action('route').setAttribute('aria-pressed',state.highlight);
    set('path',f.future_nodes.length?f.future_nodes.join(' → '):'已完成全部已接订单。');
    set('current-task',payload.current_task?taskLabel(payload.current_task):'全部任务完成');
    set('pickup-points',(payload.pickup_points||[]).sort((a,b)=>a.node_id.localeCompare(b.node_id)).map(p=>`${p.node_id} ${p.waiting_orders.length} 单`).join(' · '));
    set('task-path',payload.task_path.length?payload.task_path.map(taskLabel).join(' → '):'全部任务完成');
    const selected=edges.get(state.road);set('road-detail',selected?`${selected.from_id} → ${selected.to_id} · ${selected.distance.toFixed(1)} km · 当前 ${selected.time.toFixed(2)} 分钟 · ${selected.traffic_status==='congested'?'拥堵':'畅通'}`:'地图标签表示道路当前预计耗时，不是本车剩余耗时。');
    set('live-time',`模拟时间 ${clock(f.time)} · 已运行 ${(f.time-480).toFixed(1)} 分钟`);
    view('orders').innerHTML=data.orders.map(o=>{let label='配送中',cls='',sub=o.priority==='urgent'?'紧急':'普通';if(!o.accepted){label='已拒绝';cls='rejected';sub=reason(o.reason);}else if(o.completed!==null){label=o.late_minutes>0?'超时完成':'已完成';cls='done';sub=`完成 ${hm(o.completed)}`;}else if(o.service!==null){label='卸货中';}else if(o.pickup===null){label='待取货';}return `<tr><td>${esc(o.id)}<small>${esc(o.pickup_node_id||o.warehouse_id)} → ${esc(o.delivery_node_id)} · ${esc(sub)}</small></td><td>${o.quantity} kg</td><td>${hm(o.time_window.earliest)}–${hm(o.time_window.latest)}</td><td><span class="lp-order-state ${cls}">${label}</span></td></tr>`;}).join('');
    const kinds={start:'开始配送',tick:'时间推进',traffic_enabled:'启用路况模拟',arrive:'到达节点',pickup:'取货完成',wait:'等待时间窗口',unload_on_time:'开始卸货',unload_late:'超时卸货',delivered:'配送完成',returned:'返回仓库',order_accepted:'接受动态订单',order_rejected:'拒绝动态订单',route_activated:'新路线生效',replan:'路况评估与区间修复',window_risk:'预计超时风险'};
    set('event-count',`最近 ${payload.events.length} 条`);
    view('events').innerHTML=payload.events.slice(-6).reverse().map(e=>`<li><time>${clock(e.time)}</time>${esc(kinds[e.kind]||e.kind)}<small>${esc(e.kind==='order_rejected'?reason(e.detail):e.kind==='replan'?'评估受影响区间；当前道路保持入边时耗时。':e.detail)}</small></li>`).join('');
    const r=payload.replan;view('replan-detail').innerHTML=r?`最近一次区间修复 · ${r.searched} 个区间<p>调整前：${esc(r.before.join(' → '))}</p><p>调整后：${esc(r.after.join(' → '))}</p>`:'手动插单会立即试算，接受后按当前阶段完成位置衔接新路线。';
    draw(f,edges);
  }
  async function tick(){
    if(!state.running)return;
    try{await command({cmd:'advance',minutes:Math.min(10,.25*Number(field('speed').value))});}
    catch{return;}
    if(state.running)state.timer=setTimeout(tick,250);
  }
  action('play').addEventListener('click',()=>{if(state.running){stop();return;}state.running=true;action('play').textContent='暂停仿真';set('notice','正在运行 C++ 仿真。路况每 30 秒模拟时间随机变化。');tick();});
  action('step').addEventListener('click',async()=>{stop();try{await command({cmd:'advance',minutes:1});set('notice','已由 C++ 仿真推进 1 分钟。');}catch{}});
  action('reset').addEventListener('click',async()=>{stop();try{await command({cmd:'reset'});view('form-result').textContent='';set('notice','已重置为 3 笔初始订单；后续订单由你手动添加。');}catch{}});
  action('route').addEventListener('click',()=>{state.highlight=!state.highlight;render();});
  field('road').addEventListener('change',e=>{state.road=e.target.value;render();});field('weights').addEventListener('change',e=>{state.weights=e.target.checked;render();});
  root.querySelector('.lp-form-box').addEventListener('toggle',event=>{if(event.target.open)stop();});
  view('order-form').addEventListener('submit',async event=>{
    event.preventDefault();stop();const input=new FormData(event.currentTarget),minutes=value=>{const [h,m]=String(value).split(':').map(Number);return h*60+m;};
    const order={pickup_node_id:input.get('warehouse'),destination:input.get('destination'),quantity:Number(input.get('quantity')),priority:input.get('priority'),earliest:minutes(input.get('earliest')),latest:minutes(input.get('latest'))};
    const id=String(input.get('id')||'').trim();if(id)order.id=id;
    const result=view('form-result');let error='';if(!Number.isFinite(order.quantity)||order.quantity<=0)error='货物量必须大于 0。';else if(!Number.isFinite(order.earliest)||!Number.isFinite(order.latest)||order.latest<order.earliest)error='窗口结束时间必须不早于开始时间。';
    if(error){result.className='lp-error';result.setAttribute('role','alert');result.textContent=error;return;}
    result.textContent='C++ 正在检查取货、载重及全部剩余订单窗口…';
    try{const response=await command({cmd:'insert',order}),insertion=response.insertion;result.className=insertion.accepted?'lp-success':'lp-error';result.setAttribute('role',insertion.accepted?'status':'alert');result.textContent=insertion.accepted?`已接受 ${insertion.id}，订单列表及后续路线已更新；货物将在取货时装车。`:`${insertion.id} 已拒绝。${reason(insertion.reason)}`;set('notice',result.textContent);}
    catch(error){result.className='lp-error';result.textContent=`提交失败：${error.message}`;}
  });
  document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});window.addEventListener('pagehide',stop);
  let resizeQueued=false;new ResizeObserver(()=>{if(state.current&&!resizeQueued){resizeQueued=true;requestAnimationFrame(()=>{resizeQueued=false;draw(state.current.frame,edges);});}}).observe(view('map'));
  set('notice','正在连接本机 C++ 仿真服务…');controls();
  fetch('/api/state').then(response=>response.json()).then(payload=>{if(!payload.ok)throw new Error(payload.error);apply(payload);controls();set('notice','已连接 C++。展开“手动添加动态订单”即可提交。');}).catch(error=>{set('notice',`连接失败：${error.message}。请使用项目根目录 Start-UI.cmd 启动。`);});
})();
