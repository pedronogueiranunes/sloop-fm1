# Web editor for SLOOP PEEEO 6T9

The editor now follows the six-track fork: five synth tracks and drums on track 6. Four-track upstream devices remain supported.

## Use it

Unzip the installer package. On Mac, open **Start-Editor.command**. On Windows, open **Start-Editor.bat** (Python 3 must be installed). Keep the terminal window open and use Chrome or Edge. **Start-Installer.command** remains available for installing firmware on the Mac. The package also includes `webapp/editor/sloop-editor.html` for offline parameter editing; browser audio requires the accompanying served preview files.

Connect the FM-1 using its Felucca MIDI ports. Settings now exposes the six channel assignments, input/output routing, sync, and the DAW page: MODE, CCOUT, SPP and LOST. The editor uses the assigned channels when playing notes. In DAW mode it follows the effective routing; in CAPT it explains that incoming notes are ignored.

LIVE shows six mixer strips and six keyboard targets. The drum pads, kit picker, drum sequencer and tuning/decay controls address the final track. Solo, record indicators, fills and the master/throw state use the six-track wire layout. Alt+1–6 mutes tracks and Alt+Shift+1–6 toggles solo; arrow keys change the playable track. Existing section shortcuts remain unchanged.

The local demo (`?mock=1`) now has six tracks. Its audio preview uses the real firmware engines, the shared voice pool, and six-track pattern positions. Four-track preview models map their drum track to the compiled drum track.

## Project files

Open **Projects**, enter a name beside **Export project** and keep the `.sloopsong` file. It contains the working loop, all six tracks in each stored section A–D, and the arrangement. **Import project** loads it back after confirmation and requires playback to be stopped. Existing `.sloopsong` files remain supported.

The new export records the track count and project format. Import rejects an incompatible six-track project before writing to an older four-track target, checks object lengths/CRCs and native project headers/checksums, and rejects duplicate objects. Demo exports are simulator-only and are rejected before writing to real hardware. User samples and sound banks remain shared resources; use **Save a backup** when those resources must travel with the project.

Files can be kept on the Mac/PC and moved into Files or iCloud Drive for storage on the iPhone. Direct iPhone-to-FM-1 MIDI transfer is not implemented in this release. This update does not reallocate sample slots or add an internal project library.

## Validation

- Full host UI suite passes, including the six-track LIVE state and high solo/record bits.
- Four-track web regressions and six-track protocol tests cover routing, DAW settings, LIVE controls and project export/import.
- Browser audio matches native C output exactly for all 12 engines. Tests cover six independent patterns, mute, STOP, edits, samples and four-track compatibility.
- Browser checks with the simulated device confirm six strips/targets, synth-5 solo and the MIDI/DAW settings. A named project was downloaded through the browser; all object lengths/CRCs and six-track metadata were verified. Export uses an inline name field instead of a browser prompt. Repeat export/import with the physical FM-1 before relying on an archive.
- Physical FM-1 validation remains pending.

## Device checklist

- [ ] Connect 6T9; confirm six tracks and the Settings DAW card.
- [ ] Play tracks 1–5 on their channels and the drum pads on CH10.
- [ ] Toggle solo/record for track 5 and drums; compare browser and hardware indicators.
- [ ] Check fill, master level, throws, and the final drum lane's tuning/decay.
- [ ] Export a project with distinct patterns on synth 5 and drums, then import and compare all sections.
- [ ] Restore an old project/backup and confirm the new tracks' defaults.
- [ ] Test STOP after long notes, arp and rolls; no notes should remain held.
