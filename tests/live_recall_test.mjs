// SPDX-License-Identifier: GPL-3.0-only
// Run the actual Max JS against an asynchronous FM-1 protocol model, without hardware or Max.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source = fs.readFileSync('build/SLOOP Live Integration/sloop-live.js','utf8');
function harness() {
    const events=[], wire=[], statuses=[];
    let playing=0, hardwarePlaying=0, notifications=0, observer, drop=false, wrong=false, incompatible=false;
    const schema=JSON.parse(fs.readFileSync('build/SLOOP Live Integration/schema.json'));
    const tracks=Array.from({length:schema.nt},(_,k)=>({engine:k===schema.nt-1?schema.ne:k===4?schema.fm6:k,preset:k+3,
        p:Array.from({length:schema.np},(_,j)=>j===schema.e0?3:(j+k)%12),
        fm6:Array.from({length:128},(_,j)=>(j+k)%128)}));
    let selected=4;
    const globals=Array.from({length:schema.ng},(_,i)=>i-3);
    const drums=Array.from({length:schema.lanes},(_,i)=>(i%16)+(i%8)*16);
    const v=n=>[(n+8192)&127,(n+8192)>>7];
    const rv=(a,k)=>a[k]+128*a[k+1]-8192;
    const c={console,JSON,Math,Number,String,Array,
        notifyclients(){notifications++;},
        Task:function(fn,ctx){
            let generation=0;
            this.schedule=()=>{const g=generation;events.push(()=>{if(g===generation)fn.call(ctx);});};
            this.cancel=()=>generation++;
            this.repeat=()=>{};
        },
        LiveAPI:function(cb){this.get=()=>[playing];if(cb)observer=cb;},
        outlet(n,...args){
            if(n===1){statuses.push(args);return;}
            const f=Array.from(args[0]); wire.push(f);
            if(f[0]!==240||drop)return;
            const cmd=f[4],a=f.slice(5,-1);
            let reply=[];
            switch(cmd){
            case 1:reply=Array.from('FELUCCA SLOOP 2.8 PEEEO',x=>x.charCodeAt(0)).concat(0,
                schema.ne,schema.np+(incompatible?1:0),schema.ng,64,schema.e0);
                for(let i=0;i<schema.ne;i++)reply.push(69,0);
                reply.push(schema.nt,12);break;
            case 77:
                if(a[0]===12)drums[a[1]]=a[2]+a[3]*16;
                reply=[a[0],hardwarePlaying?1:0,...Array(20+schema.nt*3).fill(0),...drums];break;
            case 27:if(a.length)selected=a[0];reply=[selected,schema.nt];
                for(const t of tracks)reply.push(t.engine,t.preset,...v(t.p[0]),0,0);
                reply.push(0);break;
            case 29:{const t=tracks[a[0]];reply=[a[0],t.engine,t.preset,...t.p.flatMap(v)];break;}
            case 4:{const t=tracks[selected];reply=[t.engine,t.preset,...t.p.flatMap(v),...globals.flatMap(v)];break;}
            case 8:tracks[selected].engine=a[0];tracks[selected].preset=a[1];tracks[selected].p.fill(0);reply=a;break;
            case 31:tracks[a[0]].p[a[1]]=rv(a,2);reply=a.slice();if(wrong)reply[2]^=1;break;
            case 3:assert.equal(a[0],1);globals[a[1]]=rv(a,2);reply=a;break;
            case 68:assert.equal(a[0],0);reply=[...a,0,...tracks[a[1]].fm6];break;
            case 69:assert.equal(a[0],0);tracks[a[1]].fm6=a.slice(2);reply=[...a.slice(0,2),0];break;
            default:throw new Error('Unexpected command '+cmd);
            }
            events.push(()=>{for(const b of [240,125,70,76,cmd,...reply,247])c.msg_int(b);});
        }};
    vm.createContext(c);vm.runInContext(source,c);
    c.ready();
    return {c,tracks,globals,drums,wire,statuses,schema,
        pump(max=5000){let i=0;while(events.length){assert.ok(i++<max,'queue must terminate');events.shift()();}},
        setPlaying(v){playing=v;observer(['is_playing',v]);},
        setHardwarePlaying(v){hardwarePlaying=v;},
        setDrop(v){drop=v;},setWrong(v){wrong=v;},setIncompatible(v){incompatible=v;},
        selected:()=>selected, notifications:()=>notifications,
        state:()=>JSON.parse(c.getvalueof()),
        status:()=>statuses.filter(s=>s[0]==='set').at(-1)?.[1]};
}
const h=harness();h.c.capture();h.pump();
assert.match(h.status(),/CAPTURE OK/);
assert.equal(h.notifications(),1);
const captured=h.c.getvalueof(),s=h.state().sound;
assert.equal(s.tracks.length,6);assert.equal(s.tracks[5].engine,12);assert.equal(s.selected,4);
const before=structuredClone(h.tracks),gBefore=h.globals.slice();
const dBefore=h.drums.slice();h.drums.fill(0);
h.tracks.forEach(t=>{t.engine=0;t.preset=0;t.p.fill(-1);t.fm6.fill(0);});h.tracks[5].engine=12;
h.globals.fill(0);h.c.restore();h.pump();
assert.match(h.status(),/RESTORE OK/);assert.equal(h.selected(),4);
for(let k=0;k<5;k++){
 assert.equal(h.tracks[k].engine,before[k].engine);assert.equal(h.tracks[k].preset,before[k].preset);
 for(let j=0;j<h.schema.np;j++)if(!h.schema.skip.includes(j))assert.equal(h.tracks[k].p[j],before[k].p[j]);
}
assert.equal(h.tracks[5].p[h.schema.e0],before[5].p[h.schema.e0]);
assert.deepEqual(h.tracks[4].fm6,before[4].fm6,'edited FM6 working patch restored independently of the bank');
assert.deepEqual(h.drums,dBefore,'each drum sound restores tune and decay');
for(const id of h.schema.globals)assert.equal(h.globals[id],gBefore[id]);
assert.ok(h.wire.every(f=>f[0]!==240||![9,11,12,13,14,18,19,20,21,34,35,36].includes(f[4])), 'no projects, flash, assets or backup writes');
assert.ok(h.wire.filter(f=>f[4]===3).every(f=>h.schema.globals.includes(f[6])),'routing, tempo and MIDI settings preserved');

// Snapshot remains byte-for-byte intact on corrupt data, missing packets and incompatible firmware.
h.c.setvalueof('{broken');assert.equal(h.c.getvalueof(),captured);
const malformed=JSON.parse(captured);malformed.sound.tracks[4].p[3]=null;
h.c.setvalueof(JSON.stringify(malformed));assert.equal(h.c.getvalueof(),captured);
h.setDrop(true);h.c.capture();for(let i=0;i<31;i++)h.c.tick();h.pump();
assert.equal(h.c.getvalueof(),captured);assert.match(h.status(),/Sem resposta/);h.setDrop(false);
h.setIncompatible(true);h.c.restore();h.pump();assert.match(h.status(),/incompativel/);h.setIncompatible(false);
h.setHardwarePlaying(1);h.c.restore();h.pump();assert.match(h.status(),/Pare o Live/);h.setHardwarePlaying(0);
h.setPlaying(1);const n=h.wire.length;h.c.restore();h.pump();assert.equal(h.wire.length,n);h.setPlaying(0);
h.c.restore();h.setPlaying(1);h.pump();assert.match(h.status(),/interrompida/);h.setPlaying(0);
h.setWrong(true);h.c.restore();h.pump();assert.match(h.status(),/Valor recusado/);h.setWrong(false);

// No musical MIDI echo; only section clip commands survive, also with running status/realtime.
let count=h.wire.length;
for(const b of [144,60,100,128,60,0,176,7,127,192,3])h.c.msg_int(b);
assert.equal(h.wire.length,count);
for(const b of [176,119,0,248,119,3,119,4])h.c.msg_int(b);
assert.deepEqual(h.wire.slice(count),[[191,119,0],[191,119,3]]);

// Opening a Live Set restores its saved Blob once; saving the device does not restore again.
const opened=harness();opened.c.setvalueof(captured);
for(let i=0;i<20;i++)opened.c.tick();opened.pump();assert.match(opened.status(),/RESTORE OK/);
count=opened.wire.length;opened.c.ready();for(let i=0;i<30;i++)opened.c.tick();opened.pump();assert.equal(opened.wire.length,count);
opened.c.auto(0);assert.equal(opened.state().auto,0);
const off=harness();off.c.setvalueof(opened.c.getvalueof());for(let i=0;i<40;i++)off.c.tick();off.pump();assert.equal(off.wire.length,0);
console.log('Live Recall: six sounds, globals, persistence, auto recall, ACKs, timeouts, transport guards, malformed input and no note echo passed');
