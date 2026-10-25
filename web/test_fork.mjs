// SPDX-License-Identifier: GPL-3.0-only
// Six-track fork coverage, alongside the upstream four-track regression suite.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const html = fs.readFileSync(new URL('./editor.html', import.meta.url), 'utf8');
const proto = html.slice(html.indexOf('/*PROTO-BEGIN*/'), html.indexOf('/*PROTO-END*/'));
const layout = html.slice(html.indexOf('const EXPECT ='), html.indexOf('const G_SKIP ='));
const helpers = html.slice(html.indexOf('const lvP ='), html.indexOf('/* ---- requests:'));
const chan = html.slice(html.indexOf('function lvChan('), html.indexOf('const lvPro ='));
const ctx = vm.createContext({setTimeout,clearTimeout,setInterval,clearInterval,TextEncoder,TextDecoder,Blob,Response,DecompressionStream,console});
const E = vm.runInContext(proto + layout + `
let dev; const gEng = () => 20;
${helpers}
${chan}
;({CMD, req, frame, unframe, parse, Link, makeMockDevice, mixKeys, midiTrackIds, LAYOUT, EXPECT,
 songCapture, songLoad, songObjects, songOrderOk, backupCapture, backupObjects,
 setDevice(d) { dev = d; }, lvChan})`, ctx);
function attach(ntrk) {
 const m=E.makeMockDevice({ntrk,auto:false}), input=[...m.access.inputs.values()][0], output=[...m.access.outputs.values()][0];
 const sent=[],link=new E.Link(d=>{sent.push(E.unframe(d));output.send(d);},{timeout:300});
 input.onmidimessage=e=>link.receive(e.data);
 return {m,sent,rq:(...a)=>link.request(...a),done(){link.close();m.stop();}};
}
const d=attach(6),C=E.CMD;
const info=E.parse[C.INFO](await d.rq(E.req.info()));
assert.equal(info.ntrk,6);assert.equal(info.gcount,56);assert(E.EXPECT.gcount.includes(56));
assert.deepEqual(Array.from(E.midiTrackIds(6)),[33,34,35,50,51,24]);
for (const [id,label] of [[24,'DRUM'],[50,'CH4'],[51,'CH5'],[52,'MODE'],[53,'CCOUT'],[54,'SPP'],[55,'LOST']])
 assert.equal(E.parse[C.DESC](await d.rq(E.req.desc(1,id))).label,label);
await d.rq(E.req.set(1,50,5));
assert.equal(d.m.state.g[50],6,'changing CH4 skips the occupied CH5 channel');
assert.equal(d.m.state.g[51],5);await d.rq(E.req.set(1,50,4));
const pages=E.LAYOUT(info.pe0,info.ntrk,info.gcount);
assert.equal(pages.find(g=>g.t==='MIDI').pages[0][2].length,6);
assert.deepEqual(Array.from(pages.find(g=>g.t==='DAW').pages[0][2]),[52,53,54,55]);
const live=async(...a)=>E.parse[C.LIVE](await d.rq(E.req.live(...a)),info);
let state=await live(0);assert.equal(state.tracks.length,6);assert(state.pro);assert.equal(state.dtw.length,16);
const keys=E.mixKeys(info.ntrk);
for(const k of [4,5]){
 await live(2,6,k,1);state=await live(2,6,k,0);assert(state.mutes&(1<<k));
 await live(2,6,keys.solo+k,1);state=await live(2,6,keys.solo+k,0);assert(state.solo&(1<<k));
}
state=await live(2,6,keys.fill,1);assert(state.fillNow);state=await live(2,6,keys.fill,0);assert(!state.fillNow);
state=await live(10,3);assert.equal(state.throws,3);
state=await live(11,63);assert.equal(state.master,63);
state=await live(12,15,11,2);assert.deepEqual(JSON.parse(JSON.stringify(state.dtw[15])),{tune:3,dec:-2});
d.m.state.rec=(1<<4)|(1<<5);state=await live(0);assert.equal(state.rec,48);
const raw=await d.rq(E.req.live(0));assert.equal(raw.length,56);
assert.throws(()=>E.parse[C.LIVE](raw,{ntrk:4}));assert.throws(()=>E.parse[C.LIVE](raw.slice(0,-1),info));
const desc=[];for(let k=0;k<info.gcount;k++)desc.push(E.parse[C.DESC](await d.rq(E.req.desc(1,k))));
const gui={info,gdesc:desc,pdesc:[],dump:{g:d.m.state.g}};E.setDevice(gui);
for(let k=0;k<6;k++)assert.equal(E.lvChan(k).ch,[0,1,2,3,4,9][k]);
d.m.state.g[14]=1;assert.equal(E.lvChan(4).err,'lvClock');
d.m.state.g[52]=1;assert.equal(E.lvChan(4).ch,4,'DAW overrides stored CLOCK-only input');
d.m.state.g[52]=2;assert.equal(E.lvChan(4).err,'lvCapture');d.m.state.g[52]=0;d.m.state.g[14]=0;
// Distinct synth-5 and drum patterns must survive project export/import.
await d.rq(E.req.trackStep(4,7,{n:1,notes:[77,0,0,0],time:0,flags:0,vel:91,lvl:0,rat:0}));
await d.rq(E.req.drumStep(7,{on:1<<15,lvl:Array(16).fill(0),rat:Array(16).fill(0)}));
const f=await E.songCapture(d.rq,info,'Six tracks');assert.equal(f.tracks,6);assert.equal(f.projectFormat,7);
assert(E.songObjects(f).objects.get(0).length>0);
const fresh=attach(6);await E.songLoad(fresh.rq,f);
assert.equal(fresh.m.state.tracks[4].step[7].notes[0],77);assert(fresh.m.state.tracks[5].dstep[7].on&(1<<15));
const old=attach(4);await assert.rejects(()=>E.songLoad(old.rq,f));assert(!old.sent.some(x=>x.cmd===C.BK_PUT),'reject before any write');
const deviceInfo=await fresh.rq(E.req.info());
const actualInfo=[...new TextEncoder().encode('FELUCCA SLOOP 6T9'),0,...deviceInfo.slice(deviceInfo.indexOf(0)+1)];
const start=fresh.sent.length;
await assert.rejects(()=>E.songLoad((r,o)=>r[0]===C.INFO?Promise.resolve(actualInfo):fresh.rq(r,o),f));
assert(!fresh.sent.slice(start).some(x=>x.cmd===C.BK_PUT),'demo files are refused before touching hardware');
const bad=JSON.parse(JSON.stringify(f));bad.objects.push(bad.objects[0]);assert.throws(()=>E.songObjects(bad));
const oi=E.parse[C.INFO](await old.rq(E.req.info())),os=E.parse[C.LIVE](await old.rq(E.req.live(0)),oi);
assert.equal(os.tracks.length,4);assert(os.pro);assert(!E.LAYOUT(oi.pe0,oi.ntrk,oi.gcount).some(g=>g.t==='DAW'));
for(const x of [d,fresh,old])x.done();
console.log('Fork editor: six-track LIVE/masks, channel routing, DAW settings, legacy compatibility and project round-trip passed');
