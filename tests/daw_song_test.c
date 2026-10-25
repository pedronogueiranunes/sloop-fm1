/* SPDX-License-Identifier: GPL-3.0-only */
/* SPP with real packed scenes: seek/continue must not replace the user's kept working loop. */
#define FELUCCA_ARRANGER 1
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#define PROJ_HOST 1
static uint32_t trk_def_engine(uint32_t i) { return i < NPART ? i : 0; }
#include "../firmware/src/project.c"
static struct { uint8_t force; } ui;
static uint8_t sync_reload;
#include "../firmware/src/arranger_scene.c"
static void audio(void) { int32_t o[CTL * 2]; mix_block(o, CTL); }
static void note(uint8_t n) { put_step(&trk[NPART - 1], 0, 1, &n, ST_NOTE, 0); }
int main(void)
{
    uint32_t i, phase;
    host_tracks_init();
    for (i = 0; i < NPART; i++) { host_preset(&trk[i], 0, 0); trk[i].p[P_AMODE] = 0; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    note(60); assert(proj_capture(&proj_slot[0]));
    note(72); assert(proj_capture(&proj_slot[1]));
    note(55);
    arr_defaults(&arrangement); arrangement.count = 2; arrangement.loop = 0;
    arrangement.entry[0] = (arr_entry_t){0, 2}; arrangement.entry[1] = (arr_entry_t){1, 3};
    arrangement_enabled = 1; song.g[G_SYNC] = 1;
    audio();
    mclk_locate(37, 1);
    assert(!song.playing && song_kept && arrangement_clock.index == 1);
    assert(trk[NPART - 1].step[0].note[0] == 72 && clk_beat == 1 && clk_pos == BEAT_U / 4);
    mclk_event(0xFB, 1); mclk_event(0xF8, 1); audio();
    phase = arrangement_clock.phase;
    mclk_event(0xFC, 1); audio();
    assert(!song.playing && !song_kept && !arrangement_clock.running);
    assert(trk[NPART - 1].step[0].note[0] == 55);
    mclk_event(0xFB, 1);
    assert(song.playing && song_kept && arrangement_clock.running && arrangement_clock.index == 1);
    assert(trk[NPART - 1].step[0].note[0] == 72 && arrangement_clock.phase == phase);
    mclk_locate(16, 1);
    assert(arrangement_clock.index == 0 && arrangement_clock.bar == 1 && trk[NPART - 1].step[0].note[0] == 60);
    mclk_event(0xFC, 1);
    assert(trk[NPART - 1].step[0].note[0] == 55);
    mclk_locate(37, 1); mclk_event(0xFA, 1);
    assert(song_kept && arrangement_clock.index == 0 && !clk_beat && !clk_pos && mclk.wait);
    mclk_event(0xFC, 1);
    assert(!song_kept && trk[NPART - 1].step[0].note[0] == 55);
    mclk_locate(80, 1);
    assert(!song.playing && mclk.end && !song_kept && trk[NPART - 1].step[0].note[0] == 55);
    mclk_event(0xFB, 1); assert(!song.playing);
    arrangement.loop = 1; mclk_locate(85, 1);
    assert(!mclk.end && arrangement_clock.index == 0 && clk_beat == 1 && clk_pos == BEAT_U / 4);
    song.g[G_DAW] = DAW_LIVE; audio();
    assert(!song_kept && !arrangement_clock.running && trk[NPART - 1].step[0].note[0] == 55);
    mclk_event(0xFA, 1); mclk_event(0xF8, 1); audio();
    assert(!song_kept && !arrangement_clock.running && trk[NPART - 1].step[0].note[0] == 55);
    mclk_event(0xFC, 1);
    arrangement_enabled = 0;
    song.g[G_DAW] = DAW_CAPTURE;
    song.g[G_MIDI] = 1;
    usb.config = 1;
    audio();
    live_req = 1; live_bar = 0; song.playing = 1; clk_beat = 1; clk_pos = BEAT_U / 2;
    seq_out_on(&trk[NPART - 1], 55, 100);
    trk[NPART - 1].seq_n = 1; trk[NPART - 1].seq_notes[0] = 55;
    seq_out_on(TDRUM, 36, 100);
    mo_r = mo_w;
    audio();
    assert(live_req == 1 && trk[NPART - 1].step[0].note[0] == 55);
    clk_beat = 4; clk_pos = 0; audio();
    assert(live_req == -1 && live_sec == 1 && trk[NPART - 1].step[0].note[0] == 72);
    { uint32_t off5 = 0, offdrum = 0;
      while (mo_r != mo_w) {
          uint32_t p = midi_out_q[mo_r++ % MQ];
          off5 += ((p >> 8) & 255u) == 0x84u && ((p >> 16) & 127u) == 55;
          offdrum += ((p >> 8) & 255u) == 0x89u && ((p >> 16) & 127u) == 36;
      }
      assert(off5 && offdrum);
    }
    remote_section = 2; live_req = 0; mclk_locate(16, 1);
    assert(!remote_section && live_req == -1);
    puts("DAW song: real scene seek, STOP/CONTINUE, START, loop/end and working-loop restoration passed");
    return 0;
}
