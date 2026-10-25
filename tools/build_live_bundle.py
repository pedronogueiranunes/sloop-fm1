#!/usr/bin/env python3
"""Build native Live 12 templates and an unencrypted Max MIDI device from installed factory scaffolds.
No user projects, plug-ins, clips or audio are copied. Keep the resulting folder together.
"""
from pathlib import Path
import copy
import csv
import gzip
import json
import re
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
RES = Path('/Applications/Ableton Live 12 Suite.app/Contents/App-Resources')
OUT = ROOT / 'build/SLOOP Live Integration'
OUT.mkdir(parents=True, exist_ok=True)
drums = (ROOT/'firmware/src/drums.c').read_text()
notes = re.search(r'LANE_NOTE\[DRUM_LANES\]\s*=\s*\{([^}]+)', drums)[1]
names = re.search(r'LANE_NAME\[DRUM_LANES\]\s*=\s*\{([^}]+)', drums)[1]
with (OUT/'Drum Map.csv').open('w', newline='') as f:
    writer = csv.writer(f);writer.writerow(['Lane','Sound','MIDI note','Channel'])
    for i,(note,name) in enumerate(zip(re.findall(r'\d+',notes), re.findall(r'"([^"]+)"',names))):
        writer.writerow([i+1,name,note,10])
(OUT/'LEIA-ME.md').write_text((ROOT/'integrations/ableton/README.md').read_text())

core = re.sub(r'/\*.*?\*/', '', (ROOT/'firmware/src/core.h').read_text(), flags=re.S)
def ids(prefix):
    names = re.findall(r'enum\s*\{([^}]+)\}', core)
    body = next(b for b in names if re.search(r'\b'+prefix+r'COUNT\b', b))
    tokens = [s.strip() for s in body.split(',') if s.strip()]
    assert all(re.fullmatch(prefix+r'\w+', s) for s in tokens)
    return {n: i for i, n in enumerate(tokens)}
p, g = ids('P_'), ids('G_')
engine_source = (ROOT/'firmware/src/engines.c').read_text()
if re.search(r'#define FELUCCA_SLICE\s+0\b', core):
    engine_source = re.sub(r'#if FELUCCA_SLICE\b.*?#endif', '', engine_source, flags=re.S)
ne = len(re.findall(r'&ENGINE_\w+', engine_source))
if not ne:  # the production engine table uses the engine structs directly
    table = re.search(r'ENGINES\[.*?\]\s*=\s*\{(.*?)\}', engine_source, re.S)
    assert table
    ne = len(re.findall(r'&\w+', table[1]))
npart = int(re.search(r'#define NPART\s+(\d+)', core)[1])
globals_ = ['G_TUNE','G_DTIME','G_DFDBK','G_DCOLOR','G_DMIX','G_RSIZE','G_RDAMP','G_CRATE','G_CDEPTH',
            'G_DRLVL','G_DRREV','G_DRDLY','G_DUST','G_DUCK','G_FILT','G_TXSRC','G_TXLVL','G_TXTONE',
            'G_TXMOVE','G_TXRATE','G_TXWHEN','G_TXDUCK','G_FXFADE']
schema = dict(np=p['P_COUNT'], ng=g['G_COUNT'], ne=ne, nt=npart+1, e0=p['P_E0'], mute=p['P_MUTE'],
              lanes=int(re.search(r'#define DRUM_LANES\s+(\d+)', core)[1]),
              fm6=int(re.search(r'#define ENGI_FM6\s+(\d+)', core)[1]),
              modes=[p['P_ESYNC'],p['P_ITYPE']],
              skip=[p[n] for n in ['P_SLEN','P_SDIV','P_SSWING','P_SGATE']], globals=[g[n] for n in globals_])
js = 'var SLOOP = '+json.dumps(schema, separators=(',',':'))+';\n'+(ROOT/'integrations/ableton/sloop-live.js').read_text()
(OUT/'sloop-live.js').write_text(js)
(OUT/'schema.json').write_text(json.dumps(schema, indent=2)+'\n')

# AMPF is Max's native, unencrypted MIDI effect container. Preserve the factory header.
factory = (RES/'Misc/Max Devices/Max MIDI Effect.amxd').read_bytes()
assert factory[:4] == b'ampf' and factory[8:12] == b'mmmm' and factory[24:28] == b'ptch'
patcher = json.loads(factory[32:].rstrip(b'\0\n'))['patcher']
patcher.update(title='SLOOP Live Recall', openinpresentation=1, devicewidth=620.0, boxes=[], lines=[], parameters={})
boxes, lines = patcher['boxes'], patcher['lines']
def box(id_, cls, rect, text=None, **extra):
    b = dict(id=id_, maxclass=cls, patching_rect=rect, **extra)
    if text is not None: b['text'] = text
    boxes.append(dict(box=b))
def wire(a, b, out=0, inlet=0):
    lines.append(dict(patchline=dict(source=[a,out], destination=[b,inlet])))
box('in','newobj',[20,220,45,22],'midiin')
box('js','newobj',[20,265,170,22],'js sloop-live.js',varname='controller',numinlets=1,numoutlets=2)
box('out','newobj',[20,315,55,22],'midiout')
wire('in','js');wire('js','out')
box('ready','newobj',[220,220,95,22],'live.thisdevice')
box('readymsg','message',[220,250,45,22],'ready');wire('ready','readymsg');wire('readymsg','js')
box('state','newobj',[350,220,245,22],'pattr snapshot @bindto controller',varname='snapshot',
    saved_object_attributes={'parameter_enable':1},
    saved_attribute_attributes={'valueof':{'parameter_type':3,'parameter_invisible':1,
        'parameter_longname':'SLOOP Snapshot','parameter_shortname':'Snapshot','parameter_initial_enable':0}})
patcher['parameters']['state']=['SLOOP Snapshot','Snapshot',0]
box('route','newobj',[220,300,80,22],'route auto');wire('js','route',1)
box('autoset','newobj',[350,300,80,22],'prepend set');wire('route','autoset')
box('auto','toggle',[20,80,24,24],presentation=1,presentation_rect=[20,80,24,24])
box('automsg','newobj',[450,300,90,22],'prepend auto');wire('auto','automsg');wire('automsg','js');wire('autoset','auto')
box('autolabel','comment',[55,80,240,22],'Restaurar ao abrir este Live Set',presentation=1,presentation_rect=[55,80,240,22])
box('title','comment',[20,10,580,26],'SLOOP  •  Sons salvos no Live Set',fontsize=18.0,presentation=1,presentation_rect=[20,10,580,26])
for i, (name,label) in enumerate([('capture','CAPTURE'),('restore','RESTORE'),('cancel','CANCEL')]):
    # Message boxes remain usable in presentation mode; commands never depend on parameter automation.
    box(name,'textbutton',[20+i*150,44,130,27],presentation=1,presentation_rect=[20+i*150,44,130,27],text=label,mode=0)
    box(name+'msg','message',[20+i*150,360,90,22],name);wire(name,name+'msg');wire(name+'msg','js')
box('status','comment',[20,112,590,22],'Pronto: configure Felucca e use CAPTURE',presentation=1,presentation_rect=[20,112,590,22])
wire('route','status',1)
box('help','comment',[20,140,590,22],'STOP nos dois transportes • CAPTURE → Cmd+S • Mantenha este arquivo junto ao .js',
    presentation=1,presentation_rect=[20,140,590,22])
patcher['dependency_cache']=[{'name':'sloop-live.js','type':'TEXT','patcherrelativepath':'.','implicit':1}]
patcher['project']['contents']={'patchers':{}, 'code':{'sloop-live.js':{'kind':'javascript','local':1}}}
raw=json.dumps({'patcher':patcher}, ensure_ascii=False, indent=2).encode()+b'\n\0'
(OUT/'SLOOP Live Recall.amxd').write_bytes(factory[:28]+struct.pack('<I',len(raw))+raw)
(OUT/'SLOOP Live Recall.maxpat').write_bytes(raw[:-1])

# Use only factory empty-track scaffolds; remove factory return effects and groove clips.
source = ET.fromstring(gzip.decompress((RES/'Builtin/Templates/DefaultLiveSet.als').read_bytes()))
midibase = source.find('./LiveSet/Tracks/MidiTrack')
audiobase = source.find('./LiveSet/Tracks/AudioTrack')
clipbase = source.find('.//MidiClip')
assert all(e is not None for e in [midibase,audiobase,clipbase])
def value(e, path, v):
    n = e.find(path)
    assert n is not None, path
    n.set('Value',str(v))
def clear(e, path):
    n = e.find(path)
    if n is not None: n.clear()
def route(t, tag, target, upper, lower=''):
    for field, val in [('Target',target),('UpperDisplayString',upper),('LowerDisplayString',lower)]:
        value(t,'DeviceChain/'+tag+'/'+field,val)
def cleantrack(base, id_, name):
    t=copy.deepcopy(base); t.set('Id',str(id_))
    for n in ['EffectiveName','UserName']: value(t,'Name/'+n,name)
    value(t,'TrackGroupId',-1)
    for path in ['DeviceChain/DeviceChain/Devices','AutomationEnvelopes/Envelopes','TakeLanes/TakeLanes',
                 'DeviceChain/MainSequencer/ClipTimeable/ArrangerAutomation/Events',
                 'DeviceChain/FreezeSequencer/ClipTimeable/ArrangerAutomation/Events','DeviceChain/Mixer/Sends']:
        clear(t,path)
    for slot in t.findall('.//ClipSlot/ClipSlot/Value'): slot.clear()
    for env in t.findall('.//AutomationEnvelope/Automation/Events'): env.clear()
    for n in t.findall('.//Arm'): n.set('Value','false')
    value(t,'DeviceChain/Mixer/SoloSink','false')
    route(t,'AudioOutputRouting','AudioOut/Main','Main')
    return t
def section_clip(s, target):
    c=copy.deepcopy(clipbase); c.set('Id',str(s));c.set('Time','0')
    value(c,'Name','SLOOP '+chr(65+s));value(c,'Loop/LoopOn','false')
    value(c,'LaunchQuantisation',1)       # None: the FM-1 queues the next bar, avoiding double quantization
    value(c,'Annotation','CC119 = '+str(s)+'; REMOTE CH16; SLOOP changes on its next bar')
    for n in c.findall('./Notes/*'): n.clear()
    c.find('Notes').clear()
    ET.SubElement(c.find('Notes'),'KeyTracks');store=ET.SubElement(c.find('Notes'),'PerNoteEventStore');ET.SubElement(store,'EventLists')
    ET.SubElement(c.find('Notes'),'NoteProbabilityGroups')
    for tag in ['ProbabilityGroupIdGenerator','NoteIdGenerator']:ET.SubElement(ET.SubElement(c.find('Notes'),tag),'NextId',Value='1')
    envelopes=c.find('Envelopes/Envelopes');envelopes.clear()
    env=ET.SubElement(envelopes,'ClipEnvelope',Id='0')
    ref=ET.SubElement(env,'EnvelopeTarget');ET.SubElement(ref,'PointeeId',Value=target)
    automation=ET.SubElement(env,'Automation');events=ET.SubElement(automation,'Events')
    ET.SubElement(events,'FloatEvent',Id='0',Time='-63072000',Value=str(s))
    ET.SubElement(events,'FloatEvent',Id='1',Time='0',Value=str(s))
    view=ET.SubElement(automation,'AutomationTransformViewState')
    ET.SubElement(view,'IsTransformPending',Value='false');ET.SubElement(view,'TimeAndValueTransforms')
    ET.SubElement(ET.SubElement(env,'LoopSlot'),'Value')
    scroller=ET.SubElement(env,'ScrollerTimePreserver')
    ET.SubElement(scroller,'LeftTime',Value='0');ET.SubElement(scroller,'RightTime',Value='4')
    return c
for capture_mode in [False,True]:
    root=copy.deepcopy(source); live=root.find('LiveSet');tracks=live.find('Tracks');tracks.clear()
    for k,ch in enumerate(list(range(1,npart+1))+[10]):
        name='SLOOP '+('Drums' if k==npart else 'T'+str(k+1))+' • CH'+str(ch)
        t=cleantrack(midibase,k+1,name)
        route(t,'MidiInputRouting',f'MidiIn/External.Dev:Felucca{{1209-0001-0-in-0}}/{ch-1}','Felucca',f'Ch. {ch}')
        route(t,'MidiOutputRouting','MidiOut/None' if capture_mode else f'MidiOut/External.Dev:Felucca{{1209-0001-0-out-0}}/{ch-1}',
              'None' if capture_mode else 'Felucca','' if capture_mode else f'Ch. {ch}')
        value(t,'DeviceChain/MainSequencer/MonitoringEnum',2 if capture_mode else 1)
        tracks.append(t)
    ctl=cleantrack(midibase,npart+2,'SLOOP Sections • CH16')
    route(ctl,'MidiInputRouting','MidiIn/External.All/-1','All Ins')
    route(ctl,'MidiOutputRouting','MidiOut/External.Dev:Felucca{1209-0001-0-out-0}/15','Felucca','Ch. 16')
    value(ctl,'DeviceChain/MainSequencer/MonitoringEnum',2)
    # Live's controller targets: pitch bend=0, pressure=1, CC0=2; CC119=121.
    target=ctl.find('DeviceChain/MainSequencer/MidiControllers/ControllerTargets.121').get('Id')
    slots=ctl.findall('DeviceChain/MainSequencer/ClipSlotList/ClipSlot')
    for s in range(4):slots[s].find('ClipSlot/Value').append(section_clip(s,target))
    tracks.append(ctl)
    # Separate return for SysEx replies: clips on Sections can play without muting the recall input.
    recall=cleantrack(midibase,npart+3,'SLOOP Recall • drop .amxd here')
    route(recall,'MidiInputRouting','MidiIn/External.Dev:Felucca{1209-0001-0-in-0}/-1','Felucca','All Channels')
    route(recall,'MidiOutputRouting','MidiOut/External.Dev:Felucca{1209-0001-0-out-0}/-1','Felucca','All Channels')
    value(recall,'DeviceChain/MainSequencer/MonitoringEnum',1)
    # Auto + armed for capture/recall while stopped; the JS drops musical input to prevent echo.
    for e in recall.findall('.//Arm'): e.set('Value','true')
    tracks.append(recall)
    audio=cleantrack(audiobase,npart+4,'SLOOP Audio • USB Stereo')
    route(audio,'AudioInputRouting','AudioIn/External/S0','Ext. In','1/2')
    value(audio,'DeviceChain/MainSequencer/MonitoringEnum',0)
    tracks.append(audio)
    # An empty recall track must not echo notes if the device has not yet been dropped on it.
    value(recall,'DeviceChain/MainSequencer/MonitoringEnum',2)
    for e in recall.findall('.//Arm'): e.set('Value','false')
    for path in ['GroovePool/Grooves','SendsPre','SignalModulations','LinkedTrackGroups','Locators/Locators']:
        clear(live,path)
    for path in ['MainTrack/DeviceChain/DeviceChain/Devices','PreHearTrack/DeviceChain/DeviceChain/Devices']:
        clear(live,path)
    for e in live.findall('.//AutomationEnvelopes/Envelopes'):e.clear()
    # Explicitly assign fresh IDs per track before the final set-wide reference check.
    for i,t in enumerate(tracks):
        local={}
        for e in t.iter():
            if e.tag in ['AutomationTarget','ModulationTarget','Pointee'] or e.tag.startswith('ControllerTargets.'):
                old=e.get('Id');local[old]=str(200000+i*1000+len(local));e.set('Id',local[old])
        for e in t.iter('PointeeId'):
            if e.get('Value') in local:e.set('Value',local[e.get('Value')])
    value(root,'LiveSet/NextPointeeId',300000)
    value(live,'HighlightedTrackIndex',0)
    name='SLOOP Capture.als' if capture_mode else 'SLOOP DAW.als'
    (OUT/name).write_bytes(gzip.compress(ET.tostring(root,encoding='utf-8',xml_declaration=True),mtime=0))
print(OUT)
print('Schema:',schema)
