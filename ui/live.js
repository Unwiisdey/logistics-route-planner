(() => {
  'use strict';
  const $ = id => document.getElementById(id);
  const form = $('order-form');
  const esc = text => String(text ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const colors = {V01:'#3477de', V02:'#dc8b38'};
  const tints = {V01:'#edf4ff', V02:'#fff4e7'};
  const statuses = {driving:'行驶中',waiting:'等待窗口',unloading:'卸货中',idle:'空闲待单'};
  const minutes = n => `${Number(n).toFixed(2)} 分钟`;
  const clock = n => {
    if (n === null || n === undefined) return '—';
    const s = Math.round(n * 60), day = Math.floor(s / 86400), within = s % 86400;
    return `${day ? `第 ${day+1} 天 ` : ''}${String(Math.floor(within/3600)).padStart(2,'0')}:${String(Math.floor(within/60)%60).padStart(2,'0')}:${String(within%60).padStart(2,'0')}`;
  };
  const hm = n => n === null || n === undefined ? '—' : `${String(Math.floor(n/60)%24).padStart(2,'0')}:${String(Math.floor(n)%60).padStart(2,'0')}`;
  const timeValue = text => { const [h,m] = text.split(':').map(Number); return h*60+m; };
  const taskLabel = t => t.kind === 'return' ? `返仓 · ${t.node_id}` : `${t.kind === 'pickup' ? '取货' : '送货'} · ${t.order_id}${t.kind==='pickup'?`（${t.node_id}）`:''}`;
  const translate = reason => {
    const text = String(reason || '');
    if (text.includes('time fallback')) return '已使用更快的绕行路线，全部约束检查通过';
    if (text.includes('all remaining windows')) return '取货、载重与全部剩余时间窗口检查通过';
    if (text.includes('duplicate')) return '订单 ID 已使用，不能重复分配';
    if (text.includes('capacity') || text.includes('cargo exceeds')) return '取货后会超过该车载重上限';
    if (text.includes('already late')) return '该车当前服务已超时，不能增加新任务';
    if (text.includes('time window missed')) return '新单或已有订单的时间窗口无法满足';
    if (text.includes('unreachable')) return '取送节点不可达';
    if (text.includes('released')) return '订单尚未到发布时间';
    if (text.includes('all remaining')) return '取货、载重与全部剩余时间窗口检查通过';
    return text;
  };
  function failureLines(reason) {
    const entries = String(reason).split(/(?=V0[12]:)/).filter(Boolean);
    return entries.map(line => `${line.startsWith('V01:') ? 'V01：' : line.startsWith('V02:') ? 'V02：' : ''}${translate(line)}`).join('；');
  }
  const state = {data:null,busy:false,playing:false,timer:null,focus:'all',road:'',weights:'none',filter:'all',lastChange:null,expanded:new Set(),timesDirty:false};
  function notify(text, error=false) { $('notice').textContent=text; $('notice').classList.toggle('error',error); }
  function controls() {
    for (const id of ['play','step','reset','jam']) $(id).disabled=state.busy || !state.data;
    form.querySelector('[type=submit]').disabled=state.busy || !state.data;
    $('play').textContent=state.playing?'Ⅱ 暂停仿真':'▶ 开始仿真';
    $('step').disabled=state.busy || !state.data || state.playing;
    $('speed').disabled=state.busy;
  }
  function stop() { state.playing=false; clearTimeout(state.timer); state.timer=null; controls(); }
  async function command(cmd, extra={}) {
    if (state.busy) return null;
    state.busy=true; controls();
    try {
      const response=await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cmd,...extra})});
      const result=await response.json();
      if (!response.ok || !result.ok) throw new Error(result.error || '请求失败');
      state.data=result; render(); return result;
    } catch(error) { stop(); notify(`操作未完成：${error.message}`,true); return null; }
    finally { state.busy=false; controls(); }
  }
  async function tick() {
    if (!state.playing) return;
    const result=await command('advance',{minutes:Number($('speed').value)});
    if (result?.finished) { stop(); notify('两辆车均已空车返仓；仍可提交新订单，或重置再次体验。'); }
    if (state.playing) state.timer=setTimeout(tick,650);
  }
  function initOptions(data) {
    const options=items=>items.map(([value,label])=>`<option value="${esc(value)}">${esc(label)}</option>`).join('');
    if (!form.elements.warehouse.options.length) {
      form.elements.warehouse.innerHTML=options(data.nodes.filter(n=>(n.type==='warehouse'||n.type==='transfer_station')).map(n=>[n.id,`${n.id}（${n.type==='warehouse'?'仓库':'中转站'}）`]));
      form.elements.destination.innerHTML=options(data.nodes.filter(n=>n.type==='delivery_point').map(n=>[n.id,n.id]));
      form.elements.warehouse.value='W01';
      form.elements.destination.value='D05';
      $('road').innerHTML=options(data.edges.map(e=>[e.id,`${e.id}`]));
      state.road=data.vehicles[0].future_roads[0] || data.edges[0].id;
      $('road').value=state.road;
    }
  }
  const truck = '<svg viewBox="0 0 28 24" aria-hidden="true"><path d="M3 5h13v12H3zM16 9h5l4 5v3h-9" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"/><circle cx="8" cy="18" r="2.5" fill="currentColor"/><circle cx="21" cy="18" r="2.5" fill="currentColor"/></svg>';
  function renderVehicles(data) {
    $('vehicles').innerHTML=data.vehicles.map(v=>{
      const future=v.future_nodes.length?v.future_nodes.join(' → '):'已返仓，等待新订单';
      const summary=v.future_nodes.slice(0,7).join(' → ')+(v.future_nodes.length>7?' …':'');
      const position=v.edge_id?`${esc(v.from_id)} → ${esc(v.to_id)}`:esc(v.node_id || '—');
      const stats=v.segment?`<div>入边锁定耗时 <strong>${minutes(v.segment.total)}</strong></div><div>当前剩余 <strong>${minutes(v.segment.remaining)}</strong></div>`:
        `<div>当前阶段剩余 <strong>${v.finished?'—':minutes(v.remaining)}</strong></div><div>实际返仓 <strong>${hm(v.completed_at)}</strong></div>`;
      return `<article class="panel vehicle-card ${state.focus===v.id?'focused':''}" data-vehicle="${v.id}" role="button" tabindex="0" aria-label="查看 ${v.id} 路线" style="--car:${colors[v.id]};--tint:${tints[v.id]}">
        <div class="vehicle-head"><div class="vehicle-avatar">${truck}</div><div><h2 class="vehicle-title">${v.id}</h2><div class="vehicle-sub">独立载重 · 固定订单归属</div></div><span class="status-badge">${statuses[v.status]}</span></div>
        <div class="vehicle-body"><div class="vehicle-position">${position}<span class="muted">${v.edge_id?`道路 ${esc(v.edge_id)}`:v.finished?'空闲待单':'服务节点'}</span></div>
        <div class="vehicle-facts">${stats}<div>下一单 ETA <strong>${v.finished?'—':hm(v.next_arrival)}</strong></div><div>${v.finished?'实际返仓':'预计返仓'} <strong>${hm(v.finished?v.completed_at:v.return_eta)}</strong></div></div>
        <div class="load-header"><span>当前载重</span><strong>${v.load.toFixed(1)} / ${v.capacity} kg</strong></div><div class="load-bar"><span style="width:${Math.max(0,Math.min(100,v.load/v.capacity*100))}%"></span></div>
        <div class="route-block"><h3>后续道路路径</h3><details class="road-path" data-expand="${v.id}" ${state.expanded.has(v.id)?'open':''}><summary>${esc(summary || future)}</summary><p>${esc(future)}</p></details>
        <div class="current-task"><span class="task-caption">当前任务</span><span>${esc(v.current_task?taskLabel(v.current_task):'空闲，等待新订单')}</span></div>
        <h3 style="margin-top:7px">任务路径</h3><div class="task-path">${v.tasks.length?v.tasks.map((t,i)=>`${i?'<span class="task-arrow">→</span>':''}<span class="task-chip">${esc(taskLabel(t))}</span>`).join(''):'<span class="muted">全部任务完成</span>'}</div></div>
        <div class="vehicle-totals"><span>完成 ${v.delivered} / ${v.total_orders} 单</span><span>超时 ${v.late} 单</span><span>处罚 ${v.penalty} 分</span></div></div></article>`;
    }).join('');
  }
  function draw(data) {
    const nodeMap=new Map(data.nodes.map(n=>[n.id,n])), edgeMap=new Map(data.edges.map(e=>[e.id,e]));
    const xs=data.nodes.map(n=>n.x),ys=data.nodes.map(n=>n.y);
    const minX=Math.min(...xs),minY=Math.min(...ys),maxX=Math.max(...xs),maxY=Math.max(...ys);
    const p=n=>({x:67+(n.x-minX)/(maxX-minX || 1)*765,y:508-(n.y-minY)/(maxY-minY || 1)*438});
    const roadPath=e=>{
      const a=p(nodeMap.get(e.from_id)),b=p(nodeMap.get(e.to_id)),dx=b.x-a.x,dy=b.y-a.y,len=Math.hypot(dx,dy)||1;
      const nx=-dy/len*4,ny=dx/len*4;
      return {d:`M${a.x+nx},${a.y+ny} L${b.x+nx},${b.y+ny}`,mid:{x:(a.x+b.x)/2+nx*2.7,y:(a.y+b.y)/2+ny*2.7},a,b};
    };
    let out='<svg viewBox="0 0 900 575" role="img" aria-label="同一网络中的 V01 蓝色车辆与 V02 橙色车辆"><defs><pattern id="grid" width="32" height="32" patternUnits="userSpaceOnUse"><path d="M32 0H0V32" fill="none" stroke="#e8eef5" stroke-width=".7"/></pattern><filter id="car-shadow" x="-50%" y="-50%" width="200%" height="220%"><feDropShadow dx="0" dy="2" stdDeviation="3" flood-color="#254568" flood-opacity=".17"/></filter>';
    for(const [key,color] of [['base','#bdcbd8'],['V01',colors.V01],['V02',colors.V02],['jam','#ce5b61']]) out+=`<marker id="arrow-${key}" viewBox="0 0 8 8" refX="6" refY="4" markerWidth="6" markerHeight="6" orient="auto" markerUnits="userSpaceOnUse"><path d="M1 1L7 4L1 7Z" fill="${color}"/></marker>`;
    out+='</defs><rect width="900" height="575" fill="url(#grid)"/><rect x="36" y="38" width="828" height="498" rx="22" fill="#f9fbfd" fill-opacity=".4" stroke="#e5edf5" stroke-dasharray="5 6"/><text x="55" y="26" fill="#9dafc0" font-size="10" letter-spacing="1.5">CITY DELIVERY NETWORK</text>';
    for(const e of data.edges){
      const road=roadPath(e), jam=e.traffic_status==='congested', selected=e.id===state.road;
      out+=`<path d="${road.d}" stroke="${selected?'#8b6db5':jam?'#d88083':'#c6d3df'}" opacity="${jam?'.78':'.6'}" fill="none" stroke-width="${selected?3:jam?2.8:1.2}" marker-end="url(#arrow-${jam?'jam':'base'})"/>`;
    }
    for(const v of data.vehicles){
      const opacity=state.focus==='all'||state.focus===v.id?1:.23;
      for(const id of new Set(v.traversals)){ const e=edgeMap.get(id);if(e)out+=`<path d="${roadPath(e).d}" fill="none" stroke="${colors[v.id]}" stroke-width="3" opacity="${opacity*.18}"/>`; }
      for(const id of new Set(v.future_roads)){const e=edgeMap.get(id);if(e)out+=`<path d="${roadPath(e).d}" fill="none" stroke="${colors[v.id]}" stroke-width="${state.focus===v.id?3.8:2.7}" opacity="${opacity*.85}" stroke-dasharray="7 5" marker-end="url(#arrow-${v.id})"/>`;}
      if(v.edge_id && edgeMap.has(v.edge_id))out+=`<path d="${roadPath(edgeMap.get(v.edge_id)).d}" fill="none" stroke="${colors[v.id]}" stroke-width="5" opacity="${opacity}" marker-end="url(#arrow-${v.id})"/>`;
    }
    for(const e of data.edges){
      const r=roadPath(e);
      out+=`<path class="road-hit" data-road="${esc(e.id)}" d="${r.d}"><title>${esc(e.id)} · ${esc(e.from_id)} → ${esc(e.to_id)} · 当前 ${e.time.toFixed(2)} 分钟</title></path>`;
      if(state.weights!=='none')out+=`<text x="${r.mid.x}" y="${r.mid.y}" text-anchor="middle" font-size="9" fill="${e.traffic_status==='congested'?'#b85d64':'#6d8498'}" style="paint-order:stroke;stroke:#f8fafc;stroke-width:3px;pointer-events:none">${state.weights==='time'?e.time.toFixed(1):e.distance.toFixed(1)}</text>`;
    }
    for(const n of data.nodes){const a=p(n),special=n.type!=='delivery_point';
      if(n.type==='warehouse')out+=`<rect x="${a.x-10}" y="${a.y-10}" width="20" height="20" rx="5" fill="#dce8f2" stroke="#7797b5" stroke-width="1.4"/><path d="M${a.x-5} ${a.y+4}V${a.y-2}L${a.x} ${a.y-6}L${a.x+5} ${a.y-2}V${a.y+4}Z" fill="#7897b1"/>`;
      else if(n.type==='transfer_station')out+=`<path d="M${a.x} ${a.y-8}L${a.x+8} ${a.y}L${a.x} ${a.y+8}L${a.x-8} ${a.y}Z" fill="#f1e9dc" stroke="#b6a284" stroke-width="1.4"/>`;
      else out+=`<circle cx="${a.x}" cy="${a.y}" r="5" fill="#fff" stroke="#a2b4c5" stroke-width="1.5"/>`;
      out+=`<text x="${a.x}" y="${a.y+(special?25:20)}" text-anchor="middle" class="node-label">${esc(n.id)}</text>`;
    }
    const positions=data.vehicles.map(v=>p(v));
    data.vehicles.forEach((v,i)=>{let a={...positions[i]};const overlap=Math.hypot(positions[0].x-positions[1].x,positions[0].y-positions[1].y)<32;
      if(overlap){a.x+=i===0?-17:17;a.y+=i===0?-12:12;}
      const opacity=state.focus==='all'||state.focus===v.id?1:.6;
      out+=`<g class="vehicle-marker" data-car="${v.id}" opacity="${opacity}" transform="translate(${a.x},${a.y})" filter="url(#car-shadow)"><title>${v.id} · ${statuses[v.status]} · ${v.load} kg</title><circle r="16" fill="${colors[v.id]}" stroke="white" stroke-width="3"/><path d="M-9-6H2V4H-9ZM2-3H7L10 1V4H2" fill="none" stroke="white" stroke-width="1.4"/><circle cx="-5" cy="5" r="2" fill="white"/><circle cx="6" cy="5" r="2" fill="white"/><rect x="-20" y="-36" width="40" height="17" rx="5" fill="white" stroke="${colors[v.id]}" stroke-opacity=".2"/><text y="-24" text-anchor="middle" font-size="10" fill="${colors[v.id]}" font-weight="600">${v.id}</text></g>`;
    });
    $('map').innerHTML=out+'</svg>';
  }
  function renderRoad(data) {
    const e=data.edges.find(e=>e.id===state.road);if(!e)return;
    $('road').value=state.road;
    $('road-info').textContent=`${e.from_id} → ${e.to_id} · ${e.time.toFixed(2)} 分钟 · ${e.distance.toFixed(1)} km · ${e.traffic_status==='congested'?'拥堵':'畅通'}`;
  }
  function renderOrders(data) {
    $('order-count').textContent=`${data.orders.length} 单记录`;
    $('orders').innerHTML=data.orders.map(o=>{
      let status='待取货',cls='';
      if(!o.accepted){status='已拒绝';cls='reject';}
      else if(o.completed!==null){status=o.late_minutes>0?'超时完成':'已完成';cls=o.late_minutes>0?'late':'done';}
      else if(o.service!==null){status=o.late_minutes>0?'超时卸货':'卸货中';cls=o.late_minutes>0?'late':'';}
      else if(o.arrival!==null){status='等待窗口';}
      else if(o.pickup!==null){status='已装车';}
      return `<tr data-order="${esc(o.id)}"><td>${esc(o.id)}<small>${esc(o.warehouse)} → ${esc(o.destination)}${o.priority==='urgent'?' · 紧急':''}</small></td><td>${o.vehicle_id?`<span class="owner ${o.vehicle_id}">${o.vehicle_id}</span>`:'<span class="muted">未分配</span>'}</td><td>${o.quantity} kg</td><td>${o.window?`${hm(o.window.earliest)}—${hm(o.window.latest)}`:'不限'}</td><td><span class="order-status ${cls}" ${!o.accepted?`title="${esc(failureLines(o.reason))}"`:''}>${status}</span>${!o.accepted?`<small>${esc(translate(o.reason))}</small>`:''}</td><td>${o.penalty||0} 分${o.late_minutes>0?`<small>迟到 ${o.late_minutes.toFixed(2)} 分</small>`:''}</td></tr>`;
    }).join('');
  }
  function renderEvents(data) {
    const entries=data.events.filter(e=>state.filter==='all'||(state.filter==='order'?['assigned','rejected'].includes(e.kind):state.filter==='traffic'?e.kind==='traffic':e.vehicle_id===state.filter)).slice(0,35);
    $('events').innerHTML=entries.length?entries.map(e=>{
      const text=e.kind==='assigned'?`分配订单 ${e.order_id} → ${e.vehicle_id}`:e.kind==='rejected'?`拒绝订单 ${e.order_id}`:
        e.kind==='traffic'?`共享路况更新 · ${e.count} 条道路`:
        e.kind==='pickup'?`取货完成 · ${e.detail}`:e.kind.startsWith('delivered')?`配送完成 · ${e.detail}`:
        e.kind==='returned'?'空车返仓，等待新订单':`时间窗口风险 · ${e.detail}`;
      const detail=e.kind==='assigned'?translate(e.detail):e.kind==='rejected'?failureLines(e.detail):e.kind==='traffic'?'两车读取同一批道路权重；当前在途耗时保持锁定。':'';
      return `<li><time>${clock(e.time)}</time><span class="event-source ${e.vehicle_id}">${e.vehicle_id||'车队'}</span><div>${esc(text)}${detail?`<small>${esc(detail)}</small>`:''}</div></li>`;
    }).join(''):'<li class="muted">暂无此类事件</li>';
  }
  function renderReplan(data) {
    const recent=data.vehicles.filter(v=>v.replan).sort((a,b)=>b.replan.time-a.replan.time)[0];
    const insert=data.latest_insertion||state.lastChange;
    if(insert && (!recent||insert.time>=recent.replan.time)){
      $('replan-detail').innerHTML=`<div class="replan-kind">${esc(insert.vehicle)} · 插入 ${esc(insert.id)}</div><p class="muted">${clock(insert.time)} · 已有订单归属不变</p><div class="compare-route"><strong>插入前</strong>${esc(insert.before.join(' → '))}</div><div class="compare-route after"><strong>插入后 · 完成当前道路 / 服务后衔接</strong>${esc(insert.after.join(' → '))}</div>`;
    } else if(recent) {
      const r=recent.replan;
      $('replan-detail').innerHTML=`<div class="replan-kind">${recent.id} · 拥堵触发 ${r.searched} 个区间修复</div><p class="muted">${clock(r.time)} · 按耗时重新搜索受影响区间</p><div class="compare-route"><strong>修复前</strong>${esc(r.before.join(' → '))}</div><div class="compare-route after"><strong>修复后</strong>${esc(r.after.join(' → '))}</div>`;
    } else $('replan-detail').innerHTML='<div class="empty-state">出现拥堵或插入订单后，<br>在这里查看路线调整结果。</div>';
  }
  function render() {
    const d=state.data;if(!d)return;initOptions(d);
    $('clock').textContent=clock(d.time);
    const delivered=d.vehicles.reduce((s,v)=>s+v.delivered,0),total=d.orders.filter(o=>o.accepted).length;
    $('completed').innerHTML=`${delivered} <small>/ ${total} 单</small>`;
    $('active').innerHTML=`${d.vehicles.filter(v=>!v.finished).length} <small>/ 2 辆</small>`;
    $('congested').innerHTML=`${d.edges.filter(e=>e.traffic_status==='congested').length} <small>条</small>`;
    $('penalty').innerHTML=`${d.vehicles.reduce((s,v)=>s+v.penalty,0)} <small>分</small>`;
    $('late-note').textContent=`超时 ${d.vehicles.reduce((s,v)=>s+v.late,0)} 单 · 向上取整计分钟`;
    $('connection').textContent='双车核心已连接';
    $('network-count').textContent=`${d.nodes.length} 个节点 · ${d.edges.length} 条有向道路`;
    $('pickup-points').textContent='中转站待取：'+(d.pickup_points||[]).map(p=>`${p.node_id} ${p.waiting_orders.length} 单`).join(' · ');
    if(!state.timesDirty){form.elements.earliest.value=hm(d.time);form.elements.latest.value=hm(d.time+60);}
    renderVehicles(d);draw(d);renderRoad(d);renderOrders(d);renderEvents(d);renderReplan(d);
    document.querySelectorAll('[data-focus]').forEach(b=>{b.classList.toggle('selected',b.dataset.focus===state.focus);b.setAttribute('aria-pressed',String(b.dataset.focus===state.focus));});
  }
  function focus(id) { state.focus=id;render(); }
  $('focus-group').addEventListener('click',e=>{const b=e.target.closest('[data-focus]');if(b)focus(b.dataset.focus);});
  $('vehicles').addEventListener('click',e=>{if(e.target.closest('details'))return;const card=e.target.closest('[data-vehicle]');if(card)focus(card.dataset.vehicle);});
  $('vehicles').addEventListener('toggle',e=>{if(e.target.dataset.expand){if(e.target.open)state.expanded.add(e.target.dataset.expand);else state.expanded.delete(e.target.dataset.expand);}},true);
  $('map').addEventListener('click',e=>{const car=e.target.closest('[data-car]');if(car){focus(car.dataset.car);return;}const road=e.target.closest('[data-road]');if(road){state.road=road.dataset.road;draw(state.data);renderRoad(state.data);}});
  $('road').addEventListener('change',()=>{state.road=$('road').value;draw(state.data);renderRoad(state.data);});
  $('weights').addEventListener('change',()=>{state.weights=$('weights').value;draw(state.data);});
  $('log-filter').addEventListener('change',()=>{state.filter=$('log-filter').value;renderEvents(state.data);});
  $('play').addEventListener('click',()=>{if(state.playing)stop();else{state.playing=true;controls();notify('仿真运行中：两辆车随同一时钟推进，路况每 30 秒共享更新。');tick();}});
  $('step').addEventListener('click',async()=>{const d=await command('advance',{minutes:1});if(d)notify(`已推进到 ${clock(d.time)}，两车位置与预计到达时间已更新。`);});
  $('reset').addEventListener('click',async()=>{stop();state.lastChange=null;state.timesDirty=false;state.focus='all';const d=await command('reset');if(d){$('form-result').textContent='';form.elements.id.value='';notify('已重置：V01 装载 40 kg，V02 装载 20 kg，共 3 笔初始订单。');}});
  $('jam').addEventListener('click',async()=>{stop();const road=state.road;const d=await command('traffic',{road_id:road});if(d)notify(`已将 ${road} 当前耗时增加 80%；两车共享更新，受影响的后续区间自动修复。`);});
  form.elements.earliest.addEventListener('input',()=>state.timesDirty=true);
  form.elements.latest.addEventListener('input',()=>state.timesDirty=true);
  form.addEventListener('submit',async e=>{
    e.preventDefault();stop();if(!form.reportValidity())return;
    const value=new FormData(form),before=state.data.vehicles.map(v=>({id:v.id,path:v.future_nodes}));
    const earliest=timeValue(value.get('earliest')),latest=timeValue(value.get('latest'));
    if(latest<earliest){$('form-result').className='form-result error';$('form-result').textContent='窗口结束不能早于开始。';return;}
    const d=await command('insert',{order:{id:value.get('id').trim(),pickup_node_id:value.get('warehouse'),destination:value.get('destination'),quantity:Number(value.get('quantity')),priority:value.get('priority'),earliest,latest}});
    if(!d)return;const i=d.insertion,result=$('form-result');result.className=`form-result${i.accepted?'':' error'}`;
    if(i.accepted){
      state.focus=i.vehicle_id;state.lastChange={time:d.time,id:i.id,vehicle:i.vehicle_id,before:before.find(v=>v.id===i.vehicle_id).path,after:i.path};render();
      result.innerHTML=`<strong>已接受 ${esc(i.id)} → ${i.vehicle_id}</strong><small>${esc(translate(i.reason))}。已有订单归属保持不变。</small>`;
      form.elements.id.value='';
      notify(`${i.id} 已分配至 ${i.vehicle_id}，查看右侧当前任务与取送任务路径。`);
    }else{result.innerHTML=`<strong>${esc(i.id)} 已拒绝</strong><small>${esc(failureLines(i.reason))}</small>`;notify('两车均未找到可行插入；现有任务与载重保持不变。');}
  });
  async function init(){
    try{const response=await fetch('/api/state');const d=await response.json();if(!response.ok||!d.ok)throw new Error(d.error||'连接失败');state.data=d;render();controls();notify('双车核心已连接。可播放、单步推进，或手动提交紧急订单体验自动分配。');}
    catch(error){notify(`暂未连接本地仿真服务：${error.message}`,true);$('connection').textContent='连接失败';}
  }
  $('vehicles').addEventListener('keydown',e=>{if(e.target.matches('.vehicle-card')&&(e.key==='Enter'||e.key===' ')){e.preventDefault();focus(e.target.dataset.vehicle);}});
  document.addEventListener('visibilitychange',()=>{if(document.hidden)stop();});
  window.addEventListener('pagehide',stop);
  init();
})();
