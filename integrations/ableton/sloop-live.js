/* SPDX-License-Identifier: GPL-3.0-only */
/* Max's legacy js runtime (ES5). The bundle builder prepends the firmware parameter schema. */
autowatch = 0;
inlets = 1;
outlets = 2;                     // raw MIDI; status text
var saved = {format: 1, auto: 1, sound: null};
var frame = [], pending = null, job = null, info = null, initialized = false;
var midiStatus = 0, midiData = [];
var api = null, observer = null, autoPending = false, autoTries = 0;
var timer = new Task(tick, this);
timer.interval = 100;
var delayed = new Task(function () { if (job) next(); }, this);
function status(s) { outlet(1, 'set', s); }
function persist() { notifyclients(); }
function getvalueof() { return JSON.stringify(saved); }
function validSound(s) {
    if (!s || s.np !== SLOOP.np || s.ng !== SLOOP.ng || s.ne !== SLOOP.ne ||
        !s.tracks || s.tracks.length !== SLOOP.nt || !s.globals ||
        s.globals.length !== SLOOP.globals.length || !s.drums || s.drums.length !== SLOOP.lanes ||
        s.selected < 0 || s.selected >= SLOOP.nt) return false;
    var i, j, t;
    for (i = 0; i < s.tracks.length; i++) {
        t = s.tracks[i];
        if (!t || t.engine !== Math.floor(t.engine) || t.engine < 0 ||
            (i === SLOOP.nt - 1 ? t.engine !== SLOOP.ne : t.engine >= SLOOP.ne) ||
            t.preset !== Math.floor(t.preset) || t.preset < 0 || t.preset > 127 || !t.p || t.p.length !== SLOOP.np) return false;
        for (j = 0; j < t.p.length; j++) if (!validValue(t.p[j])) return false;
        if (t.engine === SLOOP.fm6) {
            if (!t.fm6 || t.fm6.length !== 128) return false;
            for (j = 0; j < t.fm6.length; j++) if (!validValue(t.fm6[j]) || t.fm6[j] < 0 || t.fm6[j] > 127) return false;
        }
    }
    for (i = 0; i < s.globals.length; i++) if (!validValue(s.globals[i])) return false;
    for (i = 0; i < s.drums.length; i++) if (!validValue(s.drums[i]) || s.drums[i] < 0 || s.drums[i] > 127) return false;
    return s.selected === Math.floor(s.selected);
}
function validValue(v) { return typeof v === 'number' && v === Math.floor(v) && v >= -8192 && v <= 8191; }
function setvalueof(v) {
    try {
        var s = JSON.parse(String(v));
        if (s.format !== 1 || (s.auto !== 0 && s.auto !== 1) || (s.sound !== null && !validSound(s.sound))) throw 'schema';
        saved = s;
        status(saved.sound ? 'Snapshot carregado no Live Set' : 'Sem snapshot: use CAPTURE');
        outlet(1, 'auto', saved.auto);
        if (initialized) armAuto();
    } catch (e) { status('Snapshot incompativel; dados atuais preservados'); }
}
function auto(v) { saved.auto = v ? 1 : 0; if (!saved.auto) autoPending = false; persist(); }
function armAuto() { autoPending = !!(saved.auto && saved.sound); autoTries = 0; }
function ready() {
    if (initialized) return;          // live.thisdevice also bangs on device saves
    initialized = true;
    api = new LiveAPI(null, 'live_set');
    observer = new LiveAPI(function (a) {
        if (a && a[0] === 'is_playing' && Number(a[1])) {
            if (job) cancel('Transporte iniciado: operacao interrompida; repita com STOP');
        }
    }, 'live_set');
    observer.property = 'is_playing';
    armAuto();
    timer.repeat();
    outlet(1, 'auto', saved.auto);
    status(saved.sound ? 'Snapshot salvo; aguardando FM-1' : 'Pronto: configure Felucca e use CAPTURE');
}
function playing() { return !api || Number(api.get('is_playing')) !== 0; }
function cancel(s) {
    job = pending = null;
    delayed.cancel();
    autoPending = false;
    status(s || 'Cancelado; snapshot salvo preservado');
}
function raw(cmd, args) { outlet(0, [240,125,70,76,cmd].concat(args || [], [247])); }
function request(cmd, args, callback, prefix, length) {
    pending = {cmd:cmd, args:args, callback:callback, prefix:prefix || [], length:length || 0, ticks:0, retries:0};
    raw(cmd, args);
}
function tick() {
    if (pending) {
        if (++pending.ticks < 10) return;
        if (pending.retries++ < 2) { pending.ticks = 0; raw(pending.cmd, pending.args); }
        else cancel('Sem resposta do FM-1; confira Track, portas e Monitor Auto/armado');
    } else if (autoPending && !job && ++autoTries >= 20) {
        autoPending = false;
        restore();
    }
}
function receive(cmd, a) {
    if (!pending || pending.cmd !== cmd || a.length < pending.length) return;
    var i;
    for (i = 0; i < pending.prefix.length; i++) if (a[i] !== pending.prefix[i]) return;
    var callback = pending.callback;
    pending = null;
    callback(a);
}
function msg_int(b) {
    if (b >= 248) {                 // realtime can be interleaved in a SysEx frame
        if (b === 250 || b === 251) { if (job) cancel('FM-1 iniciou: use STOP e RESTORE novamente'); }
        return;
    }
    if (b === 240) { frame = [b]; midiStatus = 0; midiData = []; return; }
    if (!frame.length) {
        if (b >= 128) { midiStatus = b < 240 ? b : 0; midiData = []; return; }
        if (!midiStatus) return;
        midiData.push(b);
        if (midiData.length === ((midiStatus & 224) === 192 ? 1 : 2)) {
            // Section clips only. Never forward musical notes or other CCs from this track.
            if ((midiStatus & 240) === 176 && midiData[0] === 119 && midiData[1] < 4)
                outlet(0, [191,119,midiData[1]]);
            midiData = [];
        }
        return;
    }
    if (b === 247) {
        if (frame.length >= 5 && frame[1] === 125 && frame[2] === 70 && frame[3] === 76)
            receive(frame[4], frame.slice(5));
        frame = []; return;
    }
    if (b >= 128 || frame.length >= 1024) { frame = []; return; }
    frame.push(b);
}
function v14(n) { n += 8192; return [n & 127, (n >> 7) & 127]; }
function rv(a, k) { return a[k] + a[k+1] * 128 - 8192; }
function handshake(callback) {
    request(1, [], function (a) {
        var z = a.indexOf(0), p = z + 1, version = '', i, q;
        for (i = 0; i < z; i++) version += String.fromCharCode(a[i]);
        if (z < 0 || version.indexOf('FELUCCA SLOOP') !== 0 || a[p] !== SLOOP.ne ||
            a[p+1] !== SLOOP.np || a[p+2] !== SLOOP.ng || a[p+4] !== SLOOP.e0) {
            cancel('Firmware incompativel com este dispositivo'); return;
        }
        q = p + 5;
        for (i = 0; i < SLOOP.ne; i++) { q = a.indexOf(0, q); if (q < 0) break; q++; }
        if (q < 0 || a[q] !== SLOOP.nt || a[q+1] < 12) { cancel('Firmware sem protocolo de seis pistas'); return; }
        info = {version:version};
        request(77, [0], function (a) {
            if ((a[1] & 11) || playing()) { cancel('Pare o Live e o FM-1 antes de capturar/restaurar'); return; }
            callback(a);
        }, [0], 22 + SLOOP.nt * 3 + SLOOP.lanes);
    }, [], 20);
}
function capture() {
    if (job || pending) { status('Operacao em andamento'); return; }
    if (playing()) { status('Use STOP antes de CAPTURE'); return; }
    autoPending = false;
    job = {kind:'capture', sound:{np:SLOOP.np, ng:SLOOP.ng, ne:SLOOP.ne, selected:0, tracks:[], globals:[]}};
    status('Capturando sons das seis pistas...');
    handshake(function (state) {
        job.sound.drums = state.slice(22 + SLOOP.nt * 3, 22 + SLOOP.nt * 3 + SLOOP.lanes);
        request(27, [], function (a) {
            if (a[1] !== SLOOP.nt || a[0] >= SLOOP.nt) { cancel('Resposta TRACK invalida'); return; }
            job.sound.selected = a[0];
            readTrack(0);
        }, [], 2 + SLOOP.nt * 6);
    });
}
function readTrack(k) {
    request(29, [k], function (a) {
        var p = [], j;
        for (j = 0; j < SLOOP.np; j++) p.push(rv(a, 3+j*2));
        job.sound.tracks.push({engine:a[1], preset:a[2], p:p});
        function afterTrack() {
            if (k + 1 < SLOOP.nt) readTrack(k + 1);
            else request(4, [], function (d) {
            for (j = 0; j < SLOOP.globals.length; j++) job.sound.globals.push(rv(d, 2+SLOOP.np*2+SLOOP.globals[j]*2));
            if (!validSound(job.sound)) { cancel('Snapshot invalido; anterior preservado'); return; }
            saved.sound = job.sound; job = null; persist();
            status('CAPTURE OK: salve o Live Set (Cmd+S)');
            }, [], 2+2*(SLOOP.np+SLOOP.ng));
        }
        if (a[1] === SLOOP.fm6) request(68, [0,k], function (d) {
            if (d[2] !== 0 || d.length !== 131) { cancel('Falha na captura do patch FM6; anterior preservado'); return; }
            job.sound.tracks[k].fm6 = d.slice(3);
            afterTrack();
        }, [0,k], 3);
        else afterTrack();
    }, [k], 3+2*SLOOP.np);
}
function restore() {
    if (job || pending) { status('Operacao em andamento'); return; }
    autoPending = false;
    if (!validSound(saved.sound)) { status('Sem snapshot: use CAPTURE primeiro'); return; }
    if (playing()) { status('Use STOP antes de RESTORE'); return; }
    job = {kind:'restore', queue:[], index:0};
    status('Restaurando; aguarde a confirmacao...');
    handshake(function () {
        var k, j, id, order = SLOOP.modes.slice(), t, q = job.queue, s = saved.sound;
        for (j = 0; j < SLOOP.np; j++) if (order.indexOf(j) < 0) order.push(j);
        for (k = 0; k < SLOOP.nt; k++) {
            t = s.tracks[k];
            if (k < SLOOP.nt - 1) {
                q.push({cmd:27, args:[k], prefix:[k,SLOOP.nt], length:2+SLOOP.nt*6});
                q.push({cmd:8, args:[t.engine,t.preset], prefix:[t.engine,t.preset], length:2, pause:150});
                for (j = 0; j < order.length; j++) {
                    id = order[j];
                    if (SLOOP.skip.indexOf(id) >= 0) continue;
                    q.push({cmd:31, args:[k,id].concat(v14(t.p[id])), prefix:[k,id], length:4});
                }
                // The working FM6 patch can differ from its bank. Restore it in RAM, without a flash write.
                if (t.engine === SLOOP.fm6) {
                    q.push({cmd:69, args:[0,k].concat(t.fm6), prefix:[0,k,0], length:3, pause:150});
                    q.push({cmd:68, args:[0,k], prefix:[0,k,0], length:131, patch:t.fm6});
                }
            } else {
                q.push({cmd:31, args:[k,SLOOP.e0].concat(v14(t.p[SLOOP.e0])), prefix:[k,SLOOP.e0], length:4});
                q.push({cmd:31, args:[k,SLOOP.mute].concat(v14(t.p[SLOOP.mute])), prefix:[k,SLOOP.mute], length:4});
            }
        }
        for (j = 0; j < SLOOP.globals.length; j++)
            q.push({cmd:3, args:[1,SLOOP.globals[j]].concat(v14(s.globals[j])), prefix:[1,SLOOP.globals[j]], length:4});
        for (j = 0; j < SLOOP.lanes; j++)
            q.push({cmd:77, args:[12,j,s.drums[j] & 15,s.drums[j] >> 4], prefix:[12],
                length:22 + SLOOP.nt * 3 + SLOOP.lanes, lane:j, attr:s.drums[j]});
        q.push({cmd:27, args:[s.selected], prefix:[s.selected,SLOOP.nt], length:2+SLOOP.nt*6});
        next();
    });
}
function next() {
    if (!job || playing()) { if (job) cancel('Live iniciou: restauracao interrompida'); return; }
    if (job.index >= job.queue.length) { job = null; status('RESTORE OK: sons e mix recuperados'); return; }
    var r = job.queue[job.index++];
    request(r.cmd, r.args, function (a) {
        if ((r.cmd === 31 || r.cmd === 3) && rv(a,2) !== rv(r.args,2)) {
            cancel('Valor recusado pelo FM-1; restauracao parcial, repita RESTORE'); return;
        }
        if (r.patch && JSON.stringify(a.slice(3)) !== JSON.stringify(r.patch)) {
            cancel('Patch FM6 divergente; restauracao parcial'); return;
        }
        if (r.cmd === 77 && a[22 + SLOOP.nt * 3 + r.lane] !== r.attr) {
            cancel('Bateria divergente; restauracao parcial'); return;
        }
        delayed.schedule(r.pause || 12);       // one acknowledged write at a time, no burst on USB
    }, r.prefix, r.length);
}
