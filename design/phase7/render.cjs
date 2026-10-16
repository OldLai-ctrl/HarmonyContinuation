// Render and inspect this isolated HTML prototype. Does not launch the plugin or access user.db.
// NODE_PATH must point to an installed Playwright package; use HC_BROWSER for a Chrome/Edge binary.
const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),{pathToFileURL}=require('url');
(async()=>{
 const browser=await chromium.launch({headless:true,executablePath:process.env.HC_BROWSER||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',args:['--autoplay-policy=no-user-gesture-required']});
 const page=await browser.newPage({viewport:{width:1200,height:1000},deviceScaleFactor:1});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));const root=pathToFileURL(path.join(__dirname,'index.html')).href;
 const evidence={renderer:'Playwright / installed Edge, offscreen HTML; not VSTGUI or Cubase',screens:[],layouts:[],interactions:[],errors};
 async function open(scheme,view='main',sample='B08',density='mini'){await page.goto(`${root}?scheme=${scheme}&state=${view}&sample=${sample}&density=${density}`);await page.evaluate(()=>document.fonts.ready);}
 async function shot(name){await page.screenshot({path:path.join(__dirname,'screens',name+'.png')});evidence.screens.push(name+'.png');}
 for(const scheme of ['A','B']){
  await page.setViewportSize({width:scheme==='A'?800:1360,height:scheme==='A'?840:1130});
  for(const view of ['main','detail','settings','drag','loading']){await open(scheme,view);await shot(`${scheme}-${view}`);}
  await open(scheme);await page.click('#play');await page.waitForFunction(()=>document.querySelector('#playingState').textContent.startsWith('正在试听'));await page.waitForTimeout(900);await shot(`${scheme}-playing`);await page.click('#stop');
 }
 await open('A','controls');await shot('controls');
 await page.setViewportSize({width:800,height:840});await open('A','main','B08','full');
 const fullHeight=await page.locator('.candidate').first().evaluate(x=>x.getBoundingClientRect().height);
 await page.locator('.candidate').first().screenshot({path:path.join(__dirname,'screens','A2-card-comparison.png')});evidence.screens.push('A2-card-comparison.png');
 await page.click('#cardDensity');const miniHeight=await page.locator('.candidate').first().evaluate(x=>x.getBoundingClientRect().height);
 const tileWidth=await page.locator('.candidate').first().evaluate(x=>x.getBoundingClientRect().width);
 const placements=await page.locator('.candidate').evaluateAll(xs=>xs.map(x=>{const r=x.getBoundingClientRect();return{x:r.x,y:r.y,bottom:r.bottom}}));
 const contentBottom=await page.locator('#workspace').evaluate(x=>x.getBoundingClientRect().bottom);if(placements.some(p=>p.bottom>contentBottom))throw Error('Top3 do not fit in default window');
 evidence.cardSize={window:[800,840],columns:1,cardWidth:tileWidth,cardHeight:miniHeight,A2Height:fullHeight,A3Baseline:{commit:'87f40a6',width:1125,height:94},areaReductionPercent:Math.round(100*(1-tileWidth*miniHeight/(1125*94)))};
 const content=await page.locator('.candidate').first().evaluate(x=>({name:x.querySelector('.progressionName').textContent,position:x.querySelector('.miniLocation').textContent,input:!!x.querySelector('.miniRibbon .inputChord'),boundary:!!x.querySelector('.miniRibbon .continuationBoundary'),chords:[...x.querySelectorAll('.miniRibbon .nextChord')].map(c=>({label:c.querySelector('b')?.textContent,qn:c.dataset.duration}))}));
 if(!content.input||!content.boundary||content.chords.some(c=>!c.label)||content.chords.map(c=>c.qn).join(',')!=='4,4,8,16')throw Error('A3 music information was lost');evidence.a3Information=content;
 await page.locator('.candidate[data-rank="1"]').screenshot({path:path.join(__dirname,'screens','A-card.png')});evidence.screens.push('A-card.png');
 const rhythm=await page.locator('.candidate[data-rank="1"] .clipCell.nextChord').evaluateAll(c=>c.map(x=>({qn:Number(x.dataset.duration),width:x.getBoundingClientRect().width})));
 if(Math.abs(rhythm[2].width/rhythm[0].width-2)>.03||Math.abs(rhythm[3].width/rhythm[0].width-4)>.03)throw Error('QN proportions are incorrect');
 if(!(await page.locator('.progressionName').first().textContent()).includes('半音下降低音'))throw Error('Missing actual progression name');
 evidence.interactions.push('B08 names from Factory; 4:8:16 QN chord widths measure 1:2:4; input and continuation boundary visible');
 await page.click('[data-scope="suffix"]');await page.click('#play');await page.waitForTimeout(500);const marker=await page.locator('.candidate[data-rank="1"] .clipPlayhead').evaluate(x=>parseFloat(x.style.left));if(marker<33||marker>37)throw Error('Suffix playhead does not start at input boundary');await shot('A-suffix-playing');await page.click('#stop');
 await open('A','main','R02');await shot('A-middle');
 const middle=page.locator('.candidate[data-rank="2"]');await middle.screenshot({path:path.join(__dirname,'screens','A-middle-card.png')});evidence.screens.push('A-middle-card.png');
 if(!(await middle.locator('.miniLocation').textContent()).includes('2–5/8'))throw Error('Compact source location missing');
 await page.click('[data-detail="2"]');
 if(await middle.locator('.sourceCell.prefix').count()!==1||await middle.locator('.sourceCell.gap').count()!==1||await middle.locator('.sourceCell.matched').count()!==3)throw Error('Middle match or skipped source chord is incorrect');
 if((await middle.locator('.clipGrid').allTextContents()).join(' ').includes('dim'))throw Error('Skipped source chord leaked into playback');
 evidence.interactions.push('R02 middle window 2–5 shows source prelude, three aligned input chords, one uncovered source chord; actual clip excludes source-only chords');
 await open('A');await page.evaluate(()=>{window.PHASE7_DATA[0].fullSuffix='Cmaj9/E@4.000000QN | F#m7b5@4.000000QN | B7(b9)@8.000000QN | Em9/G@4.000000QN | Am11@4.000000QN';render();});
 const five=await page.locator('.candidate').first().evaluate(x=>({cells:x.querySelectorAll('.miniRibbon .nextChord').length,labels:[...x.querySelectorAll('.miniRibbon .nextChord b')].map(c=>c.textContent)}));if(five.cells!==5||five.labels.length!==5)throw Error('Five-chord labels were lost');
 evidence.interactions.push('Layout-only five-long-chord-label fixture retains all five labels and the input block; not a generated music recommendation');
 await page.setViewportSize({width:1440,height:900});await open('A');const wide=await page.locator('.candidate').evaluateAll(xs=>xs.slice(0,2).map(x=>({x:x.getBoundingClientRect().x,y:x.getBoundingClientRect().y,input:!!x.querySelector('.inputChord'),boundary:!!x.querySelector('.continuationBoundary')})));if(Math.abs(wide[0].y-wide[1].y)>1||wide[0].x===wide[1].x||wide.some(x=>!x.input||!x.boundary))throw Error('Wide two-column layout lost A3 information');await shot('A-wide');
 for(const scheme of ['A','B'])for(const size of [[640,720],[720,800],[800,840],[900,640],[1440,1100]])for(const zoom of [1,1.25,1.5]){
  await page.setViewportSize({width:size[0],height:size[1]});await open(scheme);
  await page.evaluate(z=>{const app=document.querySelector('#app');app.style.zoom=z;app.style.height=`calc((100vh - 48px) / ${z})`;},zoom);
  const layout=await page.evaluate(()=>{const app=document.querySelector('#app'),main=document.querySelector('main'),footer=document.querySelector('footer');const f=footer.getBoundingClientRect(),m=main.getBoundingClientRect();return {bodyOverflow:document.documentElement.scrollWidth>innerWidth+1,appOverflow:app.scrollWidth>app.clientWidth+1,mainOverflow:main.scrollWidth>main.clientWidth+1,footerVisible:f.bottom<=innerHeight+2&&f.left>=0&&f.right<=innerWidth+2,mainHeight:Math.round(m.height),cards:document.querySelectorAll('.candidate').length};});
  evidence.layouts.push({scheme,size,zoom,...layout});
  if(layout.bodyOverflow||layout.appOverflow||layout.mainOverflow||!layout.footerVisible||layout.mainHeight<100)throw Error('Layout failed '+JSON.stringify(evidence.layouts.at(-1)));
  if(size[1]===640&&zoom===1.5)await shot(`${scheme}-small-150`);
 }
 await page.setViewportSize({width:1200,height:960});await open('A');
 await page.click('[data-detail="1"]');if(await page.locator('.detail').count()!==1)throw Error('Detail did not open');
 await page.click('[data-select="2"]');if(await page.locator('.selectedCard').getAttribute('data-rank')!=='2')throw Error('Selection failed');
 evidence.interactions.push('Details open; independent selection changes candidate');
 const midiBefore=await page.evaluate(()=>window.PHASE7_DATA.map(x=>x.fullBase64+'|'+x.suffixBase64).join(''));
 await page.fill('#bpm','90');await page.locator('#bpm').dispatchEvent('change');await page.click('[data-scope="suffix"]');await page.click('#play');await page.waitForFunction(()=>document.querySelector('#playingState').textContent.includes('仅续写'));await page.click('#stop');
 const midiAfter=await page.evaluate(()=>window.PHASE7_DATA.map(x=>x.fullBase64+'|'+x.suffixBase64).join(''));if(midiBefore!==midiAfter)throw Error('Tempo mutated MIDI');
 evidence.interactions.push('90 BPM and suffix-only WebAudio playback starts; stop resets; recorded MIDI bytes unchanged');
 await page.selectOption('#tempoMode','host');if(!(await page.locator('#tempoStatus').textContent()).includes('不可用'))throw Error('Missing host fallback');
 evidence.interactions.push('Unavailable host reports fallback; no host connection');
 await page.selectOption('#algorithm','HarmonyFirst');if(await page.locator('.candidate').count())throw Error('Stale cards visible');await page.waitForSelector('.candidate');
 await page.selectOption('#algorithm','Abstract');await page.selectOption('#algorithm','Legacy');await page.waitForSelector('.candidate');if(await page.locator('#algorithm').inputValue()!=='Legacy')throw Error('Wrong final preset');
 evidence.interactions.push('Preset loading clears cards and disables play; rapid switches retain final captured preset');
 await page.selectOption('#matchMode','SKELETON');if(await page.locator('.candidate').count()!==3)throw Error('Display mode changed recommendations');
 await page.selectOption('#matchMode','FULL');await page.click('[data-detail="1"]');const downloadPromise=page.waitForEvent('download');await page.click('[data-save="1"]');const dl=await downloadPromise;if(!dl.suggestedFilename().endsWith('-full.mid'))throw Error('Playback scope changed export scope');
 const expected=await page.evaluate(()=>window.PHASE7_DATA[0].fullBase64);if(fs.readFileSync(await dl.path()).toString('base64')!==expected)throw Error('Downloaded full MIDI differs from recording');
 await page.selectOption('#exportScope','suffix');const suffixDownload=page.waitForEvent('download');await page.click('[data-save="1"]');const sd=await suffixDownload;const suffixExpected=await page.evaluate(()=>window.PHASE7_DATA[0].suffixBase64);if(fs.readFileSync(await sd.path()).toString('base64')!==suffixExpected)throw Error('Downloaded suffix MIDI differs from recording');
 evidence.interactions.push('FULL/SKELETON is display-only; playback scope does not change export default; full/suffix .mid downloads equal recorded bytes');
 await open('A','drag');await page.locator('[data-drag="1"]').dragTo(page.locator('#dropDemo'));if(!(await page.locator('#dropResult').textContent()).includes('已接收'))throw Error('Prototype drag failed');
 evidence.interactions.push('Separate MIDI handle reaches internal prototype receiver; OS/Cubase drag is not tested');
 if(errors.length)throw Error(errors.join('\n'));
 fs.writeFileSync(path.join(__dirname,'verification.json'),JSON.stringify(evidence,null,2));
 console.log(JSON.stringify({screens:evidence.screens.length,layouts:evidence.layouts.length,interactions:evidence.interactions,errors}));await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
