import {copy,uid,index,grid,addLine,flatten,prefabFor} from './model.js';
const $=id=>document.getElementById(id);
const colors=['#a9ce90','#e7b66a','#86c9c1','#df9b8a','#c8cc86','#b7c8d2'];
const zones=p=>p.map.activityZones || [];
const npcs=p=>p.map.npcs || [];
const visitorZones=p=>zones(p).filter(z=>z.visitors?.enabled);
export function makeVisitors() {
  return {enabled:true,families:['025-172'],intervalMinMs:8000,intervalMaxMs:15000,stayMinMs:10000,stayMaxMs:20000};
}
export function previewActors(project,timeline,elapsed) {
  if(!timeline)return npcs(project).filter(n=>n.spawn).map(n=>({n,pose:[n.spawn[0]+.5,n.spawn[1]+.5,n.direction,0,0]}));
  const t=elapsed/timeline.stepMs,i=Math.min(Math.floor(t),timeline.frames.length-1),f=t-i;
  const a=timeline.frames[i],b=timeline.frames[Math.min(i+1,timeline.frames.length-1)];
  return timeline.actors.flatMap((n,j)=>{
    const pose=[...a[j]];
    if(pose[5]===0)return [];
    if(b[j][5]!==0&&b[j][6]===pose[6]){
      pose[0]+=(b[j][0]-pose[0])*f;pose[1]+=(b[j][1]-pose[1])*f;
      if(pose.length>8)pose[8]+=(b[j][8]-pose[8])*f;
    }
    return [{n:n.visitor?{...n,sprite:`pokemon-${String(pose[6]).padStart(3,'0')}`} : n,pose}];
  });
}
export function paintZone(zone,cells,erase=false,size=[16,12]) {
  const {index,width,height}=grid({map:{size}});
  const mask=new Set(zone.cells.map(([x,y])=>index(x,y)));
  for(const [x,y] of cells) if(x>=0&&x<width&&y>=0&&y<height) {
    if(erase)mask.delete(index(x,y));else mask.add(index(x,y));
  }
  zone.cells=[...mask].sort((a,b)=>a-b).map(i=>[i%width,Math.floor(i/width)]);
}
export function deleteZone(project,id) {
  if(npcs(project).some(n=>n.scope==='zones'&&n.zoneIds.includes(id)))
    throw Error('这个区域仍有 NPC 使用，请先调整它们的活动范围');
  project.map.activityZones=zones(project).filter(z=>z.id!==id);
  for(const n of npcs(project))n.zoneIds=n.zoneIds.filter(v=>v!==id);
}
export function makeNpc(zoneId,sprite='pokemon-025') {
  return {id:uid('npc'),name:'新 NPC',sprite,spawn:null,direction:0,behavior:'wander',
    scope:zoneId?'zones':'map',zoneIds:zoneId?[zoneId]:[],stepMs:600,waitMinMs:1000,waitMaxMs:3000,patrol:[]};
}
export function spriteFrame(sprite,pose,elapsed) {
  const moving=!!pose[4],frames=sprite.actions[moving?'walking':'idle'][pose[2]];
  return frames[Math.floor(elapsed/(moving?160:280))%frames.length];
}
export function spriteBounds(sprite,frame,pose,size) {
  const ratio=size/sprite.scale,width=frame.width*ratio,height=frame.height*ratio;
  return {left:pose[0]*size-width/2,top:pose[1]*size-(frame.height/2+frame.groundOffsetY)*ratio-(pose[8]||0)*size/26,width,height,ratio};
}
export class NpcEditor {
  constructor(env) {
    this.e=env;this.zoneId=null;this.npcId=null;this.mode='rectangle';this.npcMode='place';
    this.drag=null;this.timeline=null;this.elapsed=0;this.running=false;this.request=0;this.images={};this.pending=false;
    const bind=(id,fn)=>$(id).addEventListener('click',()=>{try{fn();}catch(e){env.toast(e.message);}});
    bind('addZone',()=>{
      if(zones(env.project()).length>=16)throw Error('最多 16 个活动区域');
      this.zoneId=uid('zone');
      env.mutate(p=>(p.map.activityZones ||= []).push({id:this.zoneId,name:`区域 ${zones(p).length+1}`,color:colors[zones(p).length%colors.length],cells:[]}));
      this.mode='rectangle';env.setTool('zone');
    });
    bind('deleteZone',()=>env.mutate(p=>deleteZone(p,this.zoneId)));
    for(const [id,mode] of [['zoneRectangle','rectangle'],['zonePaint','paint'],['zoneErase','erase']])bind(id,()=>{this.mode=mode;env.setTool('zone');this.refresh();});
    $('zoneSelect').onchange=()=>{this.zoneId=$('zoneSelect').value;env.setTool('zone');this.refresh();env.render();};
    $('zoneName').onchange=()=>this.editZone(z=>{if(!$('zoneName').value.trim())throw Error('请填写区域名称');z.name=$('zoneName').value.trim();});
    $('zoneColor').onchange=()=>this.editZone(z=>z.color=$('zoneColor').value);
    $('visitorsEnabled').onchange=()=>this.editZone(z=>{z.visitors ||= makeVisitors();z.visitors.enabled=$('visitorsEnabled').checked;});
    $('visitorSearch').oninput=()=>this.refreshVisitors();
    bind('visitorAll',()=>this.editZone(z=>z.visitors.families=this.e.catalog.npcFamilies.map(f=>f.id)));
    bind('visitorNone',()=>this.editZone(z=>z.visitors.families=[]));
    for(const [id,key,other,isMin] of [
      ['visitorIntervalMin','intervalMinMs','intervalMaxMs',true],['visitorIntervalMax','intervalMaxMs','intervalMinMs',false],
      ['visitorStayMin','stayMinMs','stayMaxMs',true],['visitorStayMax','stayMaxMs','stayMinMs',false]])
      $(id).onchange=()=>this.editZone(z=>{
        const value=Number($(id).value)*1000;
        const max=key.startsWith('stay')&&this.e.project().map.market?90000:60000;
        if(!Number.isInteger(value)||value<1000||value>max||value%100)throw Error(`时间请填写 1～${max/1000} 秒`);
        if(this.e.project().map.market)z.visitors.marketJourney=true;
        if(isMin&&value>z.visitors[other]||!isMin&&value<z.visitors[other])throw Error('最短时间不能大于最长时间');
        z.visitors[key]=value;
      });
    $('marketEnabled').onchange=()=>this.editMarket(p=>{
      if($('marketEnabled').checked)p.map.market={maxGroups:3,maxVisitors:6,soloPercent:60,firstMinMs:2000,firstMaxMs:4000,performances:true};
      else {delete p.map.market;for(const z of zones(p))if(z.visitors){z.visitors.marketJourney=false;z.visitors.stayMinMs=Math.min(z.visitors.stayMinMs,60000);z.visitors.stayMaxMs=Math.min(z.visitors.stayMaxMs,60000);}}
    });
    for(const [id,key,scale,lo,hi] of [['marketGroups','maxGroups',1,1,3],['marketVisitors','maxVisitors',1,1,6],['marketSolo','soloPercent',1,0,100],['marketFirstMin','firstMinMs',1000,100,60000],['marketFirstMax','firstMaxMs',1000,100,60000]])
      $(id).onchange=()=>this.editMarket(p=>{const value=Number($(id).value)*scale;if(!Number.isInteger(value)||value<lo||value>hi||(scale===1000&&value%100))throw Error('请填写范围内的数值');p.map.market[key]=value;});
    $('marketPerformances').onchange=()=>this.editMarket(p=>p.map.market.performances=$('marketPerformances').checked);
    $('npcReception').onchange=()=>this.editNpc(n=>{const direction=Number($('npcReception').value);if(direction===255)delete n.reception;else n.reception={...(n.reception||{lookDirections:[0,1,2,3],customerPoints:[]}),direction};});
    bind('npcCustomer',()=>{this.npcMode='customer';env.setTool('npc');env.toast('点击道路格子添加或移除顾客点，结伴需要两个相邻点');});
    bind('npcAutoCustomer',()=>this.editNpc(n=>n.reception.customerPoints=[]));
    bind('addNpc',()=>{
      if(npcs(env.project()).length>=16)throw Error('最多配置 16 个 NPC');
      const n=makeNpc(this.zone()?.id);n.name=`NPC ${npcs(env.project()).length+1}`;this.npcId=n.id;
      env.mutate(p=>(p.map.npcs ||= []).push(n));this.npcMode='place';env.setTool('npc');env.toast('点击地图设置出生点');
    });
    bind('deleteNpc',()=>env.mutate(p=>p.map.npcs=npcs(p).filter(n=>n.id!==this.npcId)));
    $('npcSelect').onchange=()=>{this.npcId=$('npcSelect').value;this.npcMode='place';env.setTool('npc');this.refresh();env.render();};
    for(const [id,key] of [['npcName','name'],['npcSprite','sprite'],['npcBehavior','behavior'],['npcScope','scope']])
      $(id).onchange=()=>this.editNpc(n=>{const v=$(id).value.trim();if(!v)throw Error('请填写 NPC 名称');n[key]=v;});
    for(const [id,key,lo,hi] of [['npcDirection','direction',0,3],['npcStep','stepMs',200,2000],['npcWaitMin','waitMinMs',100,60000],['npcWaitMax','waitMaxMs',100,60000]])
      $(id).onchange=()=>this.editNpc(n=>{
        const v=Number($(id).value);
        if(!Number.isInteger(v)||v<lo||v>hi||(key!=='direction'&&v%100))throw Error('请使用范围内的有效整数，时间以 100 毫秒递增');
        if(key==='waitMinMs'&&v>n.waitMaxMs||key==='waitMaxMs'&&v<n.waitMinMs)throw Error('最短停留不能大于最长停留');n[key]=v;
      });
    bind('npcPlace',()=>{this.npcMode='place';env.setTool('npc');env.toast('点击或拖动 NPC 设置出生点');});
    bind('npcPatrol',()=>{this.npcMode='patrol';env.setTool('npc');env.toast('依次点击巡逻点，NPC 会自动沿通路寻找路径');});
    bind('npcPopPatrol',()=>this.editNpc(n=>n.patrol.pop()));
    bind('checkActivity',()=>this.check());bind('playActivity',()=>this.play());bind('resetActivity',()=>this.reset());
    for(const id of ['zonesVisible','npcsVisible'])$(id).onchange=()=>env.render();
  }
  async load() {
    this.chatImage=new Image();
    await new Promise((resolve,reject)=>{this.chatImage.onload=resolve;this.chatImage.onerror=()=>reject(Error('聊天气泡素材加载失败'));this.chatImage.src='/art/market-chat.png';});
    await Promise.all(this.e.catalog.npcSprites.map(s=>new Promise((resolve,reject)=>{
      const im=new Image();im.onload=()=>{this.images[s.id]=im;resolve();};im.onerror=()=>reject(Error('NPC 精灵素材加载失败：'+s.name));im.src=s.url;
    })));
  }
  zone(p=this.e.project()){return zones(p).find(z=>z.id===this.zoneId);}
  npc(p=this.e.project()){return npcs(p).find(n=>n.id===this.npcId);}
  editMarket(fn){try{this.e.mutate(fn);}catch(e){this.e.toast(e.message);this.refresh();}}
  editZone(fn){try{this.e.mutate(p=>{const z=this.zone(p);if(!z)throw Error('请先新增或选择活动区域');fn(z);});}catch(e){this.e.toast(e.message);this.refresh();}}
  editNpc(fn){try{this.e.mutate(p=>{const n=this.npc(p);if(!n)throw Error('请先新增或选择 NPC');fn(n);});}catch(e){this.e.toast(e.message);this.refresh();}}
  toolChanged(tool){if(tool==='zone'||tool==='npc'){$('activityPanel').open=true;const workspace=document.querySelector('.workspace');workspace.classList.remove('hide-right');workspace.classList.add('inspector-open');this.e.fit();}this.refresh();}
  invalidate(){this.request++;this.timeline=null;this.running=false;this.elapsed=0;this.pending=false;$('activityIssues').textContent='';this.refresh();}
  refresh() {
    const p=this.e.project();
    if(!this.zone())this.zoneId=zones(p)[0]?.id || null;
    if(!this.npc())this.npcId=npcs(p)[0]?.id || null;
    const options=(id,values,current,empty)=>{
      const select=$(id);select.replaceChildren();
      for(const v of values){const o=document.createElement('option');o.value=v.id;o.textContent=v.name;select.append(o);}
      if(!values.length){const o=document.createElement('option');o.textContent=empty;select.append(o);}
      select.value=current || '';
    };
    options('zoneSelect',zones(p),this.zoneId,'尚无区域');options('npcSelect',npcs(p),this.npcId,'尚无 NPC');
    const z=this.zone(),n=this.npc();$('zoneFields').hidden=!z;$('npcFields').hidden=!n;
    options('npcSprite',this.e.catalog.npcSprites.filter(s=>!s.legacy||s.id===n?.sprite)
      .map(s=>({...s,name:s.legacy?s.name+'（旧版人物）':s.name})),n?.sprite,'尚无精灵');
    const set=(id,value)=>{if(document.activeElement!==$(id))$(id).value=value;};
    if(z){set('zoneName',z.name);set('zoneColor',z.color);$('zoneInfo').textContent=`${z.cells.length} 格 · ${npcs(p).filter(n=>n.scope==='zones'&&n.zoneIds.includes(z.id)).length} 个固定 NPC 使用`;}
    this.refreshVisitors();
    const market=p.map.market;$('marketEnabled').checked=!!market;$('marketFields').hidden=!market;
    if(market){
      for(const [id,key,scale] of [['marketGroups','maxGroups',1],['marketVisitors','maxVisitors',1],['marketSolo','soloPercent',1],['marketFirstMin','firstMinMs',1000],['marketFirstMax','firstMaxMs',1000]])set(id,market[key]/scale);
      $('marketPerformances').checked=!!market.performances;
    }
    for(const [id,mode] of [['zoneRectangle','rectangle'],['zonePaint','paint'],['zoneErase','erase']])$(id).classList.toggle('active',this.mode===mode&&this.e.tool()==='zone');
    if(n){
      for(const [id,key] of [['npcName','name'],['npcSprite','sprite'],['npcBehavior','behavior'],['npcDirection','direction'],['npcScope','scope'],['npcStep','stepMs'],['npcWaitMin','waitMinMs'],['npcWaitMax','waitMaxMs']])set(id,n[key]);
      set('npcReception',n.reception?.direction??255);$('npcReceptionFields').hidden=!n.reception;
      if(n.reception){
        $('npcCustomerInfo').textContent=n.reception.customerPoints?.length?`顾客点：${n.reception.customerPoints.map(v=>`(${v})`).join('、')}`:'自动选择接待侧的道路点';
        const choices=$('npcLookDirections');choices.replaceChildren();
        for(const [direction,name] of ['下','左','上','右'].entries()){
          const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=(n.reception.lookDirections||[0,1,2,3]).includes(direction);
          input.onchange=()=>this.editNpc(n=>{const values=n.reception.lookDirections||[0,1,2,3];const next=input.checked?[...values,direction]:values.filter(v=>v!==direction);if(!next.length)throw Error('至少保留一个观望方向');n.reception.lookDirections=next;});
          label.append(input,document.createTextNode('观望'+name));choices.append(label);
        }
      }
      const box=$('npcZoneChoices');box.replaceChildren();box.hidden=n.scope!=='zones';
      if(!zones(p).length)box.textContent='请先创建区域，或选择全地图自由活动';
      for(const z of zones(p)){
        const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=n.zoneIds.includes(z.id);
        input.onchange=()=>this.editNpc(n=>{n.zoneIds=input.checked?[...n.zoneIds,z.id]:n.zoneIds.filter(id=>id!==z.id);});
        label.append(input,document.createTextNode(z.name));box.append(label);
      }
      $('npcSpawnInfo').textContent=n.spawn?`出生点（${n.spawn.join(', ')}）· NPC 工具可拖动`:'尚未放置 · 点击地图设置出生点';
      $('patrolFields').hidden=n.behavior!=='patrol';$('npcPatrolInfo').textContent=`${n.patrol.length}/32 个巡逻点：${n.patrol.map(v=>`(${v})`).join(' → ')}`;
    }
    $('addZone').disabled=zones(p).length>=16;$('addNpc').disabled=npcs(p).length>=16;
    $('playActivity').disabled=this.pending||(!npcs(p).length&&!visitorZones(p).length);
    $('playActivity').textContent=this.pending?'准备预览…':this.running?'暂停活动':this.timeline?(this.elapsed>=120000?'重新预览':'继续预览'):'预览活动';
    this.timeLabel();
  }
  refreshVisitors(){
    const z=this.zone(),v=z?.visitors;$('visitorsEnabled').checked=!!v?.enabled;$('visitorFields').hidden=!v?.enabled;
    if(!v?.enabled)return;
    const m=this.e.project().map,entry=m.entry,edges={top:'上',bottom:'下',left:'左',right:'右'};
    $('visitorPorts').textContent=`使用地图出入口：${edges[entry.edge]}侧（${entry.point.join(', ')}）${m.paths.length?`及 ${m.paths.length} 个路线出口`:''}，原路离开。坐标从 0 开始。`;
    for(const [id,key] of [['visitorIntervalMin','intervalMinMs'],['visitorIntervalMax','intervalMaxMs'],['visitorStayMin','stayMinMs'],['visitorStayMax','stayMaxMs']])
      if(document.activeElement!==$(id))$(id).value=v[key]/1000;
    $('visitorPoolSummary').textContent=`候选组合 · 已选 ${v.families.length} 组`;
    const box=$('visitorChoices'),query=$('visitorSearch').value.trim().toLowerCase();box.replaceChildren();
    for(const family of this.e.catalog.npcFamilies.filter(f=>f.name.toLowerCase().includes(query)||f.id.includes(query))){
      const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=v.families.includes(family.id);
      input.onchange=()=>this.editZone(z=>{z.visitors.families=input.checked?[...z.visitors.families,family.id]:z.visitors.families.filter(id=>id!==family.id);});
      label.append(input,document.createTextNode(family.name));box.append(label);
    }
  }
  timeLabel(){$('activityTime').textContent=this.timeline?`${(this.elapsed/1000).toFixed(1)} / 120 秒 · ${this.running?'活动中':'已暂停'} · 不改变出生点`:'预览不改变出生点 · 每段 120 秒';}
  pointerDown(cell) {
    const tool=this.e.tool();if(tool!=='zone'&&tool!=='npc')return false;
    this.reset(false);
    if(tool==='zone'){
      if(!this.zone())throw Error('请先点击“新增区域”');
      this.e.history().begin();this.drag={type:'zone',start:cell,last:cell,before:copy(this.zone().cells)};this.pointerMove(cell);return true;
    }
    const hit=npcs(this.e.project()).find(n=>n.spawn?.[0]===cell.x&&n.spawn?.[1]===cell.y);
    if(this.npcMode==='place'&&hit){this.npcId=hit.id;this.refresh();}
    const n=this.npc();if(!n)throw Error('请先新增 NPC');
    if(this.npcMode==='customer'){
      if(!n.reception)throw Error('请先设置主要接待朝向');
      this.editNpc(n=>{const points=n.reception.customerPoints||[];const exists=points.some(([x,y])=>x===cell.x&&y===cell.y);if(!exists&&points.length>=8)throw Error('最多 8 个顾客点');n.reception.customerPoints=exists?points.filter(([x,y])=>x!==cell.x||y!==cell.y):[...points,[cell.x,cell.y]];});return true;
    }
    if(this.npcMode==='patrol'&&n.behavior==='patrol'){
      if(n.patrol.length>=32)throw Error('巡逻最多 32 个点');
      this.editNpc(n=>n.patrol.push([cell.x,cell.y]));return true;
    }
    this.e.history().begin();this.drag={type:'npc'};n.spawn=[cell.x,cell.y];this.e.render();return true;
  }
  pointerMove(cell) {
    if(!this.drag)return false;
    if(this.drag.type==='npc')this.npc().spawn=[cell.x,cell.y];
    else {
      const z=this.zone(),a=this.drag.start;
      if(this.mode==='rectangle'){
        z.cells=copy(this.drag.before);const cells=[];
        for(let y=Math.min(a.y,cell.y);y<=Math.max(a.y,cell.y);y++)for(let x=Math.min(a.x,cell.x);x<=Math.max(a.x,cell.x);x++)cells.push([x,y]);
        paintZone(z,cells,false,this.e.project().map.size);
      }else{paintZone(z,addLine([[this.drag.last.x,this.drag.last.y]],[cell.x,cell.y],this.e.project().map.size.reduce((a,b)=>a+b,0)),this.mode==='erase',this.e.project().map.size);this.drag.last=cell;}
    }
    this.e.render();return true;
  }
  finish(cancel=false){
    if(!this.drag)return false;this.drag=null;
    if(cancel)this.e.history().cancel();else if(this.e.history().finish())this.e.changed();
    this.refresh();this.e.render();return true;
  }
  reset(render=true){this.running=false;this.elapsed=0;this.timeline=null;this.pending=false;this.request++;this.refresh();if(render)this.e.render();}
  async check(){
    const token=this.request;
    try{const data=await this.e.json('/api/npc-check',copy(this.e.project()));if(token!==this.request)return;this.showIssues(data.issues);}
    catch(e){if(token===this.request)this.e.toast(e.message);}
  }
  showIssues(issues){$('activityIssues').textContent=issues.length?issues.map(v=>v.message).join('；'):'活动配置检查通过';}
  async play(){
    if(this.drag)return;
    if(this.running){this.running=false;this.refresh();return;}
    if(this.timeline&&this.elapsed<120000){this.running=true;this.last=performance.now();this.refresh();return;}
    const token=++this.request;this.pending=true;this.refresh();
    try{
      const data=await this.e.json('/api/npc-preview',copy(this.e.project()));
      if(token!==this.request)return;
      this.timeline=data;this.elapsed=0;this.running=true;this.last=performance.now();this.showIssues(data.issues);
    }catch(e){if(token===this.request)this.e.toast(e.message);}
    finally{if(token===this.request){this.pending=false;this.refresh();this.e.render();}}
  }
  tick(now){if(!this.running)return;this.elapsed=Math.min(120000,this.elapsed+Math.min(250,now-this.last));this.last=now;
    if(this.elapsed===120000)this.running=false;this.timeLabel();if(!this.running)this.refresh();this.e.render();}
  draw(ctx,size,layers,owners,visible,drawTile){
    const p=this.e.project(),tool=this.e.tool();
    const {width:columns,height:rows,index}=grid(p);
    if($('zonesVisible').checked)for(const z of zones(p)){
      ctx.fillStyle=z.color+(z.id===this.zoneId?'35':'18');ctx.strokeStyle=z.color;ctx.lineWidth=z.id===this.zoneId?1.5:0.6;
      const mask=new Set(z.cells.map(c=>index(...c)));
      for(const [x,y] of z.cells){ctx.fillRect(x*size,y*size,size,size);ctx.beginPath();
        for(const [dx,dy,x1,y1,x2,y2] of [[0,-1,0,0,1,0],[1,0,1,0,1,1],[0,1,0,1,1,1],[-1,0,0,0,0,1]])
          if(x+dx<0||x+dx>=columns||y+dy<0||y+dy>=rows||!mask.has(index(x+dx,y+dy))){ctx.moveTo((x+x1)*size,(y+y1)*size);ctx.lineTo((x+x2)*size,(y+y2)*size);}
        ctx.stroke();
      }
    }
    if(!$('npcsVisible').checked)return;
    const actors=previewActors(p,this.timeline,this.elapsed).sort((a,b)=>a.pose[1]-b.pose[1]);
    for(const {n,pose} of actors){
      const [x,y]=pose,im=this.images[n.sprite],sprite=this.e.catalog.npcSprites.find(s=>s.id===n.sprite);if(!im||!sprite)continue;
      const frame=spriteFrame(sprite,pose,this.elapsed),{left,top,width,height,ratio}=spriteBounds(sprite,frame,pose,size);
      const bubbleSize=21*size/26,bubbleX=x*size-bubbleSize/2,bubbleY=top+(frame.visibleTop||0)*ratio-2*size/26-bubbleSize;
      ctx.imageSmoothingEnabled=false;ctx.fillStyle='#0004';ctx.beginPath();ctx.ellipse(x*size,y*size-2,9*ratio,3*ratio,0,0,7);ctx.fill();
      ctx.drawImage(im,frame.x,frame.y,frame.width,frame.height,left,top,width,height);
      if(pose[7]&&this.chatImage)ctx.drawImage(this.chatImage,bubbleX,bubbleY,bubbleSize,bubbleSize);
      ctx.save();ctx.beginPath();const clipTop=pose[7]?Math.min(top,bubbleY):top;const clipLeft=pose[7]?Math.min(left,bubbleX):left;const clipRight=pose[7]?Math.max(left+width,bubbleX+bubbleSize):left+width;ctx.rect(clipLeft,clipTop,clipRight-clipLeft,top+height-clipTop);ctx.clip();
      for(let z=1;z<3;z++)if(visible[z])layers[z].forEach((key,i)=>{
        const a=this.e.catalog.assets[key];if(!a)return;
        const o=p.map.objects.find(o=>o.id===owners[z][i]),pref=o&&prefabFor(p,o.prefab);
        const depth=a.depth?.[z]||0;
        const ground=o&&pref.anchorMode!=='runtime'?o.y+pref.anchor[1]:Math.floor(i/columns)+depth-(a.ladder?.[z] ? .5 : 0);
        if(depth&&y<ground)drawTile(ctx,key,i%columns*size,Math.floor(i/columns)*size,size,this.e.animation());
      });ctx.restore();
      if(tool==='npc'||tool==='zone'){
        ctx.fillStyle=n.id===this.npcId?'#f4d989':'#e9eee4';ctx.font=`${Math.max(9,size*.3)}px "PingFang SC",sans-serif`;
        ctx.fillText(n.visitor?sprite.name.split(' · ')[1]:n.name,x*size-size*.5,top-3);
        const spawn=this.npc()?.id===n.id?this.npc().spawn:null;
        if(spawn){ctx.strokeStyle='#f4d989';ctx.strokeRect(spawn[0]*size+2,spawn[1]*size+2,size-4,size-4);}
      }
    }
    const n=this.npc();
    if(tool==='npc'&&n?.reception){
      ctx.strokeStyle='#f4d989';ctx.fillStyle='#f4d989';
      for(const [x,y] of n.reception.customerPoints||[]){ctx.strokeRect(x*size+3,y*size+3,size-6,size-6);ctx.fillText('客',(x+.25)*size,(y+.6)*size);}
    }
    if(tool==='npc'&&n?.behavior==='patrol'){
      ctx.strokeStyle='#e7b66a';ctx.fillStyle='#e7b66a';ctx.beginPath();
      n.patrol.forEach(([x,y],i)=>{if(i)ctx.lineTo((x+.5)*size,(y+.5)*size);else ctx.moveTo((x+.5)*size,(y+.5)*size);});ctx.stroke();
      n.patrol.forEach(([x,y],i)=>{ctx.fillRect((x+.5)*size-3,(y+.5)*size-3,6,6);ctx.fillText(String(i+1),(x+.5)*size+4,(y+.5)*size);});
    }
  }
}
