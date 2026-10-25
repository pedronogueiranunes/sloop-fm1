#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import gzip
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET

folder=Path('build/SLOOP Live Integration')
for mode in ['DAW','Capture']:
    r=ET.fromstring(gzip.decompress((folder/f'SLOOP {mode}.als').read_bytes()))
    tracks=r.findall('./LiveSet/Tracks/*')
    assert len(tracks)==9 and len(r.findall('.//MidiClip'))==4
    assert len({t.get('Id') for t in tracks})==9
    assert not r.findall('.//Devices/*'), 'no copied plug-ins, instruments or unrelated effects'
    for k,ch in enumerate([1,2,3,4,5,10]):
        t=tracks[k];assert t.tag=='MidiTrack'
        assert t.find('DeviceChain/MidiInputRouting/Target').get('Value').endswith('/'+str(ch-1))
        target=t.find('DeviceChain/MidiOutputRouting/Target').get('Value')
        assert target=='MidiOut/None' if mode=='Capture' else target.endswith('/'+str(ch-1))
    ctl=tracks[6]
    assert ctl.find('DeviceChain/MidiOutputRouting/Target').get('Value').endswith('/15')
    target=ctl.find('DeviceChain/MainSequencer/MidiControllers/ControllerTargets.121').get('Id')
    for s,c in enumerate(ctl.findall('.//MidiClip')):
        assert c.find('LaunchQuantisation').get('Value')=='1'
        assert c.find('Loop/LoopOn').get('Value')=='false'
        assert not c.findall('.//MidiNoteEvent')
        assert c.find('.//PointeeId').get('Value')==target
        assert {e.get('Value') for e in c.findall('.//FloatEvent')}=={str(s)}
        assert c.find('ProgramChange').get('Value')=='-1'
    assert tracks[8].tag=='AudioTrack'
    assert tracks[8].find('DeviceChain/AudioInputRouting/Target').get('Value')=='AudioIn/External/S0'
    assert tracks[7].find('DeviceChain/MainSequencer/MonitoringEnum').get('Value')=='2', 'empty recall track cannot echo'
    ids=[e.get('Id') for e in r.iter() if e.tag in ['Pointee','AutomationTarget','ModulationTarget'] or e.tag.startswith('ControllerTargets.')]
    assert len(ids)==len(set(ids)), 'globally unique automation and routing targets'
    for e in r.iter('PointeeId'):
        assert e.get('Value') in ids or e.get('Value')=='0', e.attrib
    assert not r.findall('.//FileRef/Path[@Value]') or all(not e.get('Value') for e in r.findall('.//FileRef/Path'))
raw=(folder/'SLOOP Live Recall.amxd').read_bytes()
assert raw[:4]==b'ampf' and raw[8:12]==b'mmmm' and raw[24:28]==b'ptch'
assert struct.unpack_from('<I',raw,28)[0]==len(raw)-32
p=json.loads(raw[32:].rstrip(b'\0\n'))['patcher']
b={x['box']['id']:x['box'] for x in p['boxes']}
assert b['state']['saved_attribute_attributes']['valueof']['parameter_type']==3
assert b['state']['saved_object_attributes']['parameter_enable']==1
assert b['state']['saved_attribute_attributes']['valueof']['parameter_invisible']==1
assert 'controller' in b['state']['text'] and b['js']['varname']=='controller'
assert (folder/'sloop-live.js').is_file()
print('Live bundle: native AMXD container, persisted Blob, six MIDI routes, safe recall track, four CC119 clips, stereo return and global ID references passed')
