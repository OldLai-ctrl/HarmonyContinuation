'use strict';
// Isolated design prototype. Captured Phase 6 recommendations; no worker or host connection.
const $ = s => document.querySelector(s);
const params = new URLSearchParams(location.search);
const state = { scheme: params.get('scheme') === 'B' ? 'B' : 'A', sample:params.get('sample')==='R02'?'R02':'B08', dense:params.get('density')!=='full', preset:'Legacy', intent:'RESOLVE', selected:1, detail:0, scope:'full', exportScope:'full', busy:false, playing:false, generation:0, bpm:120 };
let context, voices=[], timer, startedAt=0, totalSeconds=0, toastTimer;
const rows=()=>window.PHASE7_DATA.filter(r=>r.preset===state.preset&&r.sample===state.sample);
const candidate=()=>rows().find(r=>r.rank===state.selected);
const route = s => s.split(' | ').map(x=>x.split('@')[0].replaceAll('Ab','A♭').replaceAll('Bb','B♭'));
const pathHTML = s => route(s).join(' <i>→</i> ');
const pretty=s=>s.replaceAll('Ab','A♭').replaceAll('Bb','B♭');
const events=s=>s.split(' | ').map(x=>({label:pretty(x.split('@')[0]),qn:Number.parseFloat(x.split('@')[1])}));
const inputEvents=r=>events(r.fullPath.split(' -> ')[0]);
const sum=a=>a.reduce((v,e)=>v+e.qn,0);
function sourceMap(r){
 const trace=r.alignmentTrace.split('|').map(s=>s.split(':'));
 const substitutes=trace.filter(s=>s[2]==='SUBSTITUTE').length,skips=trace.filter(s=>s[2]==='TEMPLATE_DELETION').length;
 const cells=r.sourceDegrees.map((degree,i)=>{const step=trace.find(s=>s[1]===String(i));const matched=step&&step[0]!=='-';const phase=i<r.matchStart?'prefix':i>=r.continuationStart?'next':matched?'matched':'gap';const label=matched?`${step[2]==='SUBSTITUTE'?'≈ ':''}输入${Number(step[0])+1}`:phase==='next'?'续写':phase==='prefix'?'前文':'◇ 未覆盖';return `<div class="sourceCell ${phase}" data-source-index="${i}" title="来源第${i+1}和弦 ${degree} · ${label}"><b>${degree}</b><small>${i+1} · ${label}</small></div>`;}).join('');
 return `<div class="sourceContext"><div class="contextLabel"><span>来源位置 <b>输入对应第 ${r.matchStart+1}–${r.matchEnd+1} 和弦 / 共 ${r.sourceDegrees.length}</b></span><span>${substitutes?'≈ '+substitutes+' 处替代匹配':skips?'含输入未覆盖和弦':'逐项匹配'}${skips?' · ◇ '+skips+' 个未覆盖':''}</span></div><div class="sourceScroll"><div class="sourceGrid" style="grid-template-columns:repeat(${r.sourceDegrees.length},1fr);min-width:${r.sourceDegrees.length*64}px">${cells}</div></div></div>`;
}
function ribbon(r){
 const input=inputEvents(r),next=events(r.fullSuffix),all=[...input,...next],total=sum(all),boundary=sum(input),width=Math.max(580,total/Math.min(...all.map(e=>e.qn))*64);
 const cells=all.map((e,i)=>`<div class="clipCell ${i<input.length?'inputChord':'nextChord'}" data-chord-index="${i}" data-duration="${e.qn}"><b>${e.label}</b><small>${e.qn} QN <span>· ${e.qn/4} 小节</span></small></div>`).join('');
 const ticks=Array.from({length:Math.ceil(total/4)},(_,i)=>`<span style="left:${i*4/total*100}%">${i+1}</span>`).join('');
 return `<div class="clipSummary"><span><i class="legend inputLegend"></i>你的输入 · ${boundary} QN</span><span><i class="legend nextLegend"></i>推荐续写 · ${sum(next)} QN</span><span>4/4 · ${total/4} 小节</span></div><div class="clipScroll"><div class="clipTrack" style="min-width:${width}px" data-input-qn="${boundary}" data-total-qn="${total}"><div class="clipRuler" aria-label="片段内小节，从1计数">${ticks}<small>小节</small></div><div class="clipGrid" style="grid-template-columns:${all.map(e=>e.qn+'fr').join(' ')}">${cells}</div><div class="continuationBoundary" style="left:${boundary/total*100}%" title="输入结束，续写从此开始"><span>续写起点</span></div><div class="clipPlayhead" hidden></div></div></div>`;
}
function miniPosition(r){const trace=r.alignmentTrace.split('|').map(x=>x.split(':')),gaps=trace.filter(x=>x[2]==='TEMPLATE_DELETION').length,variants=trace.filter(x=>x[2]==='SUBSTITUTE').length;return `<span class="miniLocation" title="输入对应来源第${r.matchStart+1}–${r.matchEnd+1}个和弦；${gaps}个未覆盖、${variants}处替代。详情可看完整对应。">输入位置 <b>${r.matchStart+1}–${r.matchEnd+1}/${r.sourceDegrees.length}</b><span class="positionTicks" aria-hidden="true">${r.sourceDegrees.map((x,i)=>`<i class="${i>=r.matchStart&&i<=r.matchEnd?'inside':''} ${trace.some(x=>x[1]===String(i)&&x[2]==='TEMPLATE_DELETION')?'omitted':''}"></i>`).join('')}</span>${gaps?' · ◇'+gaps:variants?' · ≈'+variants:''}</span>`;}
function miniRibbon(r){const input=inputEvents(r),next=events(r.fullSuffix),boundary=sum(input),total=sum(next);return `<div class="miniChordRoute">${pathHTML(r.fullSuffix)}</div><div class="clipScroll miniRibbon"><div class="clipTrack" data-input-qn="${boundary}" data-offset-qn="${boundary}" data-total-qn="${total}"><div class="clipGrid" style="grid-template-columns:${next.map(e=>e.qn+'fr').join(' ')}">${next.map(e=>`<div class="clipCell nextChord" data-duration="${e.qn}" title="${e.label} · ${e.qn} QN"><small>${e.qn}</small></div>`).join('')}</div><div class="clipPlayhead" hidden></div></div></div>`;}
function card(r){return `<article class="candidate ${state.dense?'miniCard ':''}${state.selected===r.rank?'selectedCard':''}" data-rank="${r.rank}"><div class="cardMain"><button class="selectCard" data-select="${r.rank}" aria-label="选择推荐 ${r.rank}" aria-pressed="${state.selected===r.rank}">${state.selected===r.rank?'✓':String(r.rank).padStart(2,'0')}</button><div class="cardContent"><h3 class="progressionName">${r.nameZh||r.name}</h3><div class="cardMeta">${state.dense?miniPosition(r):`<b>收束 ${String(r.rank).padStart(2,'0')}</b><span>· ${events(r.fullSuffix).length} 个续写和弦</span>${state.selected===r.rank?'<span class="selectionLabel">✓ 已选中</span>':''}`}</div></div><div class="cardActions"><button class="audition" data-audition="${r.rank}">${state.playing&&state.selected===r.rank?'■ 停止':'▶ 试听'}</button><button class="dragHandle" draggable="true" data-drag="${r.rank}" title="拖至原型接收区；正式版接系统文件拖放">⠿ MIDI</button><button data-detail="${r.rank}" aria-expanded="${state.detail===r.rank}">详情 ${state.detail===r.rank?'−':'+'}</button></div></div><div class="musicStructure">${state.dense?miniRibbon(r):sourceMap(r)+ribbon(r)}</div>${state.detail===r.rank?(state.dense?`<div class="expandedStructure"><h4>完整来源与实际片段</h4>${sourceMap(r)}${ribbon(r)}</div>`:'')+details(r):''}</article>`;}
function toast(s){$('#toast').textContent=s;$('#toast').style.display='block';clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('#toast').style.display='none',4500);}
function scheme(){ $('#app').className=state.scheme==='A'?'compact':'spacious';$('#scheme').textContent=state.scheme==='A'?'切换到 B · Spacious':'切换到 A · Compact';document.title=`${state.scheme} · Apple ${state.scheme==='A'?'Compact':'Spacious'} — HarmonyContinuation`; }
function render(){
 $('#cards').classList.toggle('tileGrid',state.dense);
 $('#cardDensity').textContent=state.dense?'对比 A2 详细卡片':'切回 A3 精简卡片';
 $('#resultNote').textContent=state.dense?'时值条与上方和弦依次对应 · 数字 = QN':'块宽 ∝ QN 时值 · 灰色为输入，蓝色为续写 · 竖线标出续写起点';
 const unavailable=state.intent!=='RESOLVE';
 $('#cards').innerHTML=state.busy?'<div class="skeleton"><strong>◌ 正在读取录制样例…</strong><small>旧结果已清除 · 此处演示加载门禁，不运行推荐引擎</small></div>':unavailable?'<div class="skeleton"><strong>此快照未记录该条件下的推荐</strong><small>请切回收束查看真实 Top3。正式版由 Worker 计算并返回结果。</small></div>':rows().map(card).join('');
 $('#play').disabled=state.busy||unavailable;$('#more').disabled=state.busy||unavailable;
 $('#resultLabel').textContent=state.busy?'正在载入…':unavailable?'无录制快照':'TOP 3 · Factory V4';
 $('#playingTitle').textContent=unavailable||state.busy?'等待可用推荐':`${candidate().nameZh||candidate().name} · ${route(candidate().fullSuffix).join(' → ')}`;
 $('#play').textContent=state.playing?'■':'▶';$('#play').setAttribute('aria-label',state.playing?'停止试听':'播放选中推荐');
 $('#playingState').textContent=state.playing?'正在试听 · '+(state.scope==='full'?'原进行 + 续写':'仅续写'):'准备试听';
 document.querySelectorAll('[data-intent]').forEach(b=>{b.classList.toggle('selected',b.dataset.intent===state.intent);b.setAttribute('aria-pressed',String(b.dataset.intent===state.intent));});
 document.querySelectorAll('[data-scope]').forEach(b=>{b.classList.toggle('selected',b.dataset.scope===state.scope);b.setAttribute('aria-pressed',String(b.dataset.scope===state.scope));});
}
function details(r){return `<div class="detail"><div><h3>Why · 匹配解释</h3><p>当前输入：${route(r.fullPath.split(' -> ')[0]).join(' → ')}。来源：${r.nameZh||r.name}。输入匹配源第 ${r.matchStart+1}–${r.matchEnd+1} 和弦；续写从第 ${r.continuationStart+1} 个和弦开始。</p><p>${r.alignmentTrace.includes('TEMPLATE_DELETION')?'◇ 表示匹配窗口内输入未覆盖的来源和弦，它不会加入实际试听或导出。':'来源位置展示完整模板；实际试听仍保留你的输入。'} ${r.alignmentTrace.includes('SUBSTITUTE')?'≈ 表示替代匹配，不意味着输入和弦与来源和弦完全相同。':'按录制 alignmentTrace 标示对应关系。'}</p><div class="inline"><button data-save="${r.rank}">保存 MIDI…</button><button data-note="收藏入口将复用现有 SaveMetadata 表单；此演示不写入用户曲库。">收藏到用户曲库</button><button data-note="固定为现有会话级操作；正式版沿用候选身份和 pinned 状态。">固定候选</button></div></div><div><h3>音乐信息</h3><label class="exportScopeLabel">MIDI 导出 <select id="exportScope"><option value="full" ${state.exportScope==='full'?'selected':' '}>完整进行</option><option value="suffix" ${state.exportScope==='suffix'?'selected':' '}>仅续写</option></select></label><dl><dt>来源</dt><dd>${r.candidateID}</dd><dt>匹配相似度</dt><dd>${(r.similarity*100).toFixed(1)}%</dd><dt>排名分 / 准入分</dt><dd>${r.score.toFixed(2)} / ${r.eligibilityScore.toFixed(2)}</dd><dt>原始时值</dt><dd>${r.fullSuffix.replaceAll('@',' · ').replaceAll('.000000QN',' QN')}</dd><dt>支持来源</dt><dd>${r.supportingIDs}</dd></dl><div class="inline"><button data-panel="enrichment">变体与升级</button><button data-note="正式版沿用 .hcrec.json 快照保存入口。当前原型只导出已记录 MIDI。">保存快照…</button></div></div></div>`;}
function stop(){voices.forEach(v=>{try{v.stop();}catch{}});voices=[];clearInterval(timer);state.playing=false;$('#playProgress').style.width='0%';render();}
// Read the exported Standard MIDI File itself. No chord-generation or recommendation logic.
function midiNotes(b64){
 const bytes=Uint8Array.from(atob(b64),c=>c.charCodeAt(0)); const view=new DataView(bytes.buffer); const division=view.getUint16(12); let pos=14;const events=[];
 const vl=()=>{let n=0,b;do{b=bytes[pos++];n=(n<<7)|(b&127);}while(b&128);return n;};
 while(pos<bytes.length){const tag=String.fromCharCode(...bytes.slice(pos,pos+4));const len=view.getUint32(pos+4);pos+=8;const end=pos+len;let tick=0,running=0;
 if(tag!=='MTrk'){pos=end;continue;}while(pos<end){tick+=vl();let status=bytes[pos];if(status>=128){pos++;running=status;}else status=running;
 if(status===255){pos++;const n=vl();pos+=n;continue;}if(status===240||status===247){const n=vl();pos+=n;continue;}
 const op=status&240,channel=status&15,a=bytes[pos++],b=(op===192||op===208)?0:bytes[pos++];if(op===144||op===128)events.push({qn:tick/division,on:op===144&&b>0,note:a,vel:b,channel});
 }pos=end;}
 const active=new Map(),notes=[];for(const e of events){const key=e.channel+':'+e.note;if(e.on)active.set(key,e);else if(active.has(key)){const begin=active.get(key);notes.push({note:e.note,start:begin.qn,duration:e.qn-begin.qn,velocity:begin.vel});active.delete(key);}}return notes;
}
async function play(rank=state.selected){
 stop();state.selected=rank;if(state.busy||state.intent!=='RESOLVE')return;
 try{context??=new AudioContext();await context.resume();const notes=midiNotes(candidate()[state.scope+'Base64']);if(!notes.length)throw Error('MIDI 中没有可用音符');
 const seconds=60/($('#tempoMode').value==='host'?120:state.bpm),origin=context.currentTime+.06;totalSeconds=Math.max(...notes.map(n=>n.start+n.duration))*seconds;startedAt=origin;state.playing=true;
 for(const n of notes){const osc=context.createOscillator(),gain=context.createGain(),s=origin+n.start*seconds,e=s+Math.max(.04,n.duration*seconds);osc.type='triangle';osc.frequency.value=440*Math.pow(2,(n.note-69)/12);gain.gain.setValueAtTime(0,s);gain.gain.linearRampToValueAtTime(.018*n.velocity/127,s+.012);gain.gain.exponentialRampToValueAtTime(.0001,e);osc.connect(gain).connect(context.destination);osc.start(s);osc.stop(e+.02);voices.push(osc);}
 render();timer=setInterval(()=>{const elapsed=Math.max(0,context.currentTime-startedAt);$('#playProgress').style.width=Math.min(100,100*elapsed/totalSeconds)+'%';const r=candidate(),input=inputEvents(r),all=[...input,...events(r.fullSuffix)];const positionQN=elapsed/seconds+(state.scope==='suffix'?sum(input):0);document.querySelectorAll(`.candidate[data-rank="${state.selected}"] .clipTrack`).forEach(track=>{const marker=track.querySelector('.clipPlayhead');const offset=Number(track.dataset.offsetQn||0);marker.hidden=positionQN<offset;marker.style.left=Math.max(0,Math.min(100,(positionQN-offset)/Number(track.dataset.totalQn)*100))+'%';let cursor=offset;track.querySelectorAll('.clipCell').forEach(cell=>{const duration=Number(cell.dataset.duration);cell.classList.toggle('isCurrent',positionQN>=cursor&&positionQN<cursor+duration);cursor+=duration;});});if(elapsed>=totalSeconds)stop();},50);
 }catch(e){stop();toast('试听未启动：'+e.message);}
}
function save(rank){const r=rows().find(r=>r.rank===rank);const a=document.createElement('a');a.href=r[state.exportScope+'Midi'];a.download=r.candidateID+'-'+state.exportScope+'.mid';a.click();toast('保存原始 MIDI · '+(state.exportScope==='full'?'完整进行':'仅续写')+' · QN 时值不变');}
function panel(name){
 const panels={
 input:['当前输入与导入',`<p>设计原型当前使用 ${state.sample} 的已录制 MIDI 输入。正式版在此保留 MIDI 文件导入、标准文件拖入、剪贴板和已有输入的确认；批量导入仍进入曲库工作流。</p><div class="panelGrid"><div><h4>已有进行</h4><p>${route(rows()[0].fullPath.split(' -> ')[0]).join(' / ')}<br>输入时值：${inputEvents(rows()[0]).map(e=>e.qn).join(' / ')} QN · 共 ${sum(inputEvents(rows()[0]))} QN</p></div><div><h4>调性与拍号</h4><p>C Major · 4/4<br>正式版保留自动调性、手动覆盖及候选调性。</p></div><div><h4>更多输入操作</h4><p>刷新宿主 / 复制原文 / 当前进行 MIDI / 打开快照 / 清除覆盖。原始结构与未知尾长状态继续保留。</p></div></div>`],
 settings:['高级设置',`<div class="panelGrid"><div><h4>推荐与匹配</h4><p>算法：${state.preset}（顶部直接切换）<br>FULL / SKELETON：主创作区<br>匹配深度、过滤与开发诊断按需展开。</p></div><div><h4>界面与实例</h4><label>语言 <select id="localeDemo"><option>简体中文</option><option>English · 入口示意</option></select></label><label>界面缩放 <select id="zoomDemo"><option value="1">100%</option><option value="1.25">125%</option><option value="1.5">150%</option></select></label><p>试听偏好按实例保留；本轮不改 Session Schema。</p></div><div><h4>Factory 与宿主</h4><p>Factory V4 · 基线 657 条<br>宿主 BPM：未连接<br>0.10.0-dev.7 / 8c27c27</p><button data-note="正式版复用 Controller::exportHostDiagnostics；此原型没有宿主诊断可导出。">诊断导出…</button><button data-note="HarmonyContinuation · Phase 7 设计原型。插件基线 dev.7，生产界面尚未改动。">About</button></div></div>`],
 library:['曲库与收藏',`<p>独立低频工作区保留 Factory / 用户曲库、搜索过滤、元数据检查、收藏保存、编辑删除、批量导入、MIDI 导出、快照恢复。关闭面板即可回到当前推荐，选择与滚动位置不丢失。</p><p>本原型仅有录制的 Factory 候选；不连接或改写真实 User Library。</p>`],
 color:['色彩偏好',`<p>正式版沿用现有 Off / Smooth / Colorful / Follow Input 排序和色彩提示开关；不改变匹配分数或算法。候选 identity 与固定项继续保留。</p><div class="inline"><button data-note="当前显示原始录制排序（Off）。">Off</button><button data-note="Smooth 的排序需由现有 ColorPreferenceReranker 返回；本快照未包含该结果。">Smooth</button><button data-note="Colorful 的排序需由现有 ColorPreferenceReranker 返回；本快照未包含该结果。">Colorful</button><button data-note="Follow Input 的排序需由现有 ColorPreferenceReranker 返回；本快照未包含该结果。">Follow Input</button></div>`],
 enrichment:['变体与升级进行',`<p>沿用现有 Enrichment 的候选分组、固定、Why、试听、MIDI 导出与快照。升级对象是当前输入；此入口不会把续写候选静默替换成另一对象。</p><p>本次录制数据未包含 Enrichment 结果。正式版在该面板中接入现有业务结果，底部试听栏同时显示对象类型。</p>`],
 more:['更多推荐',`<p>此录制快照仅包含 Top3，未编造额外候选。正式版在同一区域追加可滚动列表，保留已有 retained / defaultPresented / More 策略；不重新评分。</p>`]
 };
 const [title,content]=panels[name];$('#panel').innerHTML=`<div class="panelHeader"><h3>${title}</h3><button id="closePanel">收起 ×</button></div>${content}`;$('#panel').hidden=false;$('#panel').scrollIntoView({block:'end',behavior:'instant'});
}
async function loadPreset(){const g=++state.generation;stop();state.busy=true;state.detail=0;state.selected=1;$('#analysisStatus').textContent='◌ 读取录制样例';render();await new Promise(r=>setTimeout(r,600));if(g!==state.generation)return;state.busy=false;$('#analysisStatus').textContent='✓ 录制结果已加载';render();}
document.addEventListener('click',e=>{const b=e.target.closest('button');if(!b||b.disabled)return;
 if(b.dataset.select){stop();state.selected=+b.dataset.select;render();}
 if(b.dataset.audition){const rank=+b.dataset.audition;if(state.playing&&state.selected===rank)stop();else play(rank);}
 if(b.dataset.detail){state.detail=state.detail===+b.dataset.detail?0:+b.dataset.detail;render();}
 if(b.dataset.intent){stop();state.intent=b.dataset.intent;render();}
 if(b.dataset.scope){const was=state.playing;stop();state.scope=b.dataset.scope;render();if(was)toast('试听范围已更改，请从起点重新播放。');}
 if(b.dataset.panel)panel(b.dataset.panel);
 if(b.dataset.note)toast(b.dataset.note);
 if(b.dataset.save)save(+b.dataset.save);
 if(b.dataset.drag){$('#dropDemo').hidden=false;$('#dropDemo').scrollIntoView({block:'end'});toast('按住 MIDI 手柄拖动；也可在详情中保存 MIDI 文件。');}
 if(b.id==='closePanel')$('#panel').hidden=true;
});
$('#scheme').onclick=()=>{state.scheme=state.scheme==='A'?'B':'A';scheme();};
$('#cardDensity').onclick=()=>{stop();state.dense=!state.dense;state.detail=0;render();};
$('#algorithm').onchange=()=>{state.preset=$('#algorithm').value;loadPreset();};
let fullTimeline=$('.timeline').innerHTML;
$('#matchMode').onchange=()=>{const skeleton=$('#matchMode').value==='SKELETON';$('.timeline').innerHTML=skeleton?'<div style="grid-column:1/-1">骨架索引未包含在此录制快照中；正式版复用现有 skeletonIndices。推荐结果保持不变。</div>':fullTimeline;};
$('#play').onclick=()=>state.playing?stop():play();$('#stop').onclick=stop;
$('#bpm').min='20';$('#bpm').max='400';
$('#bpm').onchange=()=>{const n=Number($('#bpm').value);if(!Number.isFinite(n)||n<20||n>400){$('#bpm').value=state.bpm;toast('试听速度范围为 20–400 BPM。');return;}const was=state.playing;stop();state.bpm=n;if(was)toast('速度已更改，请从起点重新播放。');};
$('#tempoMode').onchange=()=>{stop();const host=$('#tempoMode').value==='host';$('#tempoStatus').textContent=host?'宿主速度不可用 · 回退 120 BPM':'仅影响试听 · MIDI 时值保持不变';$('#bpm').disabled=host;$('#bpm').value=host?120:state.bpm;};
$('#import').onclick=()=>panel('input');$('#more').onclick=()=>panel('more');
$('#stateGuide').onclick=()=>$('#guide').showModal();$('#closeGuide').onclick=()=>$('#guide').close();
$('#showDrag').onclick=()=>{$('#guide').close();$('#dropDemo').hidden=false;$('#dropDemo').scrollIntoView({block:'end'});};
$('#loadingDemo').onclick=()=>{$('#guide').close();stop();state.busy=true;render();toast('加载状态演示；切换算法可恢复录制结果。');};
document.addEventListener('change',e=>{if(e.target.id==='exportScope'){state.exportScope=e.target.value;toast('MIDI 导出范围已更改；试听范围保持独立。');}if(e.target.id==='zoomDemo'){$('#app').style.zoom=e.target.value;$('#app').style.height=`calc((100vh - 48px) / ${e.target.value})`;}if(e.target.id==='localeDemo')toast('英文入口已标示；完整双语文案将在正式 VSTGUI 中复用 Localization。');});
document.addEventListener('dragstart',e=>{const el=e.target.closest('[data-drag]');if(!el)return;const r=rows().find(x=>x.rank===+el.dataset.drag);e.dataTransfer.setData('text/plain',String(r.rank));e.dataTransfer.effectAllowed='copy';$('#dropDemo').hidden=false;});
$('#dropDemo').ondragover=e=>{e.preventDefault();e.dataTransfer.dropEffect='copy';};$('#dropDemo').ondrop=e=>{e.preventDefault();const r=rows().find(x=>x.rank===Number(e.dataTransfer.getData('text/plain')));if(r){$('#dropResult').textContent=`✓ 已接收 ${r.candidateID} · ${state.exportScope==='full'?'完整进行':'仅续写'}。点击详情中的保存 MIDI 获取文件。`;}};
function updateExample(){
 const r=rows()[0],input=inputEvents(r),inputQN=sum(input);
 $('#example').value=state.sample;$('#algorithm').disabled=state.sample==='R02';
 $('.inputTitle h1').innerHTML=pathHTML(r.fullPath.split(' -> ')[0]);
 $('.inputMeta').textContent=`C 大调 · 4/4 · ${inputQN/4} 小节`;
 fullTimeline=input.map((e,i)=>`<div><small>${i+1}</small><b>${e.label}</b><small>${e.qn} QN</small></div>`).join('');$('.timeline').style.gridTemplateColumns=`repeat(${input.length},1fr)`;$('.timeline').innerHTML=fullTimeline;$('.timeline').setAttribute('aria-label',`已有进行时间轴，共${inputQN} QN`);
 $('.inputFoot').innerHTML=`<span>${state.sample==='B08'?'B08 · 半音下行低音':'R02 · 中段匹配样例（Legacy 录制）'}</span><span>${inputQN} QN / 完整输入已保留</span>`;
}
$('#example').onchange=()=>{stop();state.generation++;state.busy=false;state.sample=$('#example').value;state.preset='Legacy';$('#algorithm').value='Legacy';state.intent='RESOLVE';state.detail=0;state.selected=1;$('#matchMode').value='FULL';$('#panel').hidden=true;updateExample();render();$('#workspace').scrollTop=0;};
scheme();updateExample();render();
// URL states are reproducible review views, not host state.
if(params.get('state')==='detail') {state.detail=1;render();}
if(params.get('state')==='settings') panel('settings');
if(params.get('state')==='drag') {$('#dropDemo').hidden=false;$('#dropDemo').scrollIntoView({block:'end'});}
if(params.get('state')==='loading') {state.busy=true;render();}
if(params.get('state')==='controls') $('#guide').showModal();
