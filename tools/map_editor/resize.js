import {copy,dimensions,prefabFor} from './model.js';

export function resizeMap(project,size,ground='outside:385') {
  if (!Array.isArray(size)||size.length!==2||!size.every(Number.isInteger)||size[0]<16||size[1]<12||size.some(v=>v>64))
    throw Error('宽度须为 16～64 格，高度须为 12～64 格，且必须是整数');
  const [oldW,oldH]=dimensions(project), [width,height]=size, m=project.map;
  if(width===oldW&&height===oldH)return;
  const outside=([x,y])=>x>=width||y>=height;
  const boundaryOutside=([x,y])=>x>width||y>height;
  const problems=[];
  const cropped=m.layers.reduce((n,layer,z)=>n+layer.filter((v,i)=>outside([i%oldW,Math.floor(i/oldW)])&&v!==null&&(z!==0||v!==ground)).length,0);
  if(cropped)problems.push(`${cropped} 个地形或装饰图块`);
  const objects=m.objects.filter(o=>{const p=prefabFor(project,o.prefab);return o.x+p.width>width||o.y+p.height>height;});
  if(objects.length)problems.push(`${objects.length} 个完整物件`);
  const zones=(m.activityZones||[]).filter(z=>z.cells.some(outside));
  if(zones.length)problems.push(`活动范围：${zones.map(z=>z.name).join('、')}`);
  const actors=(m.npcs||[]).filter(n=>(n.spawn&&outside(n.spawn))||n.patrol.some(outside)||(n.reception?.customerPoints||[]).some(outside));
  if(actors.length)problems.push(`NPC 出生点、巡逻点或顾客点：${actors.map(n=>n.name).join('、')}`);
  const paths=m.paths.filter(p=>p.points.some(p.kind==='roadNetwork'?boundaryOutside:outside));
  if(paths.length)problems.push(`${paths.length} 条路线`);
  const croppedNetwork=m.roadNetwork?.nodes.some(boundaryOutside);
  const referencedNetwork=m.paths.some(p=>p.kind==='roadNetwork');
  if(croppedNetwork&&referencedNetwork)problems.push('路线引用的路网（请重新识别，或删除原道路路线）');
  const relocated=e=>{
    const next=copy(e);
    if(e.edge==='right'&&e.point[0]===oldW-1)next.point[0]=width-1;
    if(e.edge==='bottom'&&e.point[1]===oldH-1)next.point[1]=height-1;
    return next;
  };
  const endpoints=[m.entry,...m.paths.flatMap(p=>[p.exit,...(p.roadEntry?[p.roadEntry]:[])])];
  if(endpoints.some(e=>outside(relocated(e).point)))problems.push('入口或出口');
  const entryJunction=JSON.stringify(m.junction)===JSON.stringify(m.entry.point);
  if(!entryJunction&&outside(m.junction))problems.push('分岔点');
  if(problems.length)throw Error('无法缩小，边界外仍有'+problems.join('；')+'。请先移动或清理，再调整尺寸。');
  m.layers=m.layers.map((layer,z)=>Array.from({length:width*height},(_,i)=>{
    const x=i%width,y=Math.floor(i/width);
    return x<oldW&&y<oldH?layer[y*oldW+x]:(z===0?ground:null);
  }));
  if(m.lakeGround)m.lakeGround=Object.fromEntries(Object.entries(m.lakeGround).flatMap(([i,key])=>{
    const x=Number(i)%oldW,y=Math.floor(Number(i)/oldW);
    return x<width&&y<height?[[String(y*width+x),key]]:[];
  }));
  m.entry=relocated(m.entry);
  if(entryJunction)m.junction=copy(m.entry.point);
  for(const p of m.paths){p.exit=relocated(p.exit);if(p.roadEntry)p.roadEntry=relocated(p.roadEntry);}
  m.size=[...size];
  if(croppedNetwork&&!referencedNetwork)delete m.roadNetwork;
  // Derived graph/source snapshots remain intact, but now compare as stale.
}
