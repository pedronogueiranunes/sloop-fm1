/* SPDX-License-Identifier: GPL-3.0-only */
/* Panel CC output, DAW/Capture profiles, SPP packet ordering and clock-loss policy. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#include "../firmware/src/midi_settings.c"

static void audio(void) { int32_t o[CTL * 2]; mix_block(o, CTL); }
static void usb_message(uint32_t status, uint32_t a, uint32_t b)
{
    uint32_t cin = status == 0xF2u ? 3u : status >= 0xF8u ? 15u : status >> 4;
    midi_in_event(cin | status << 8 | a << 16 | b << 24);
}
static uint32_t output(uint32_t status, uint32_t a, uint32_t *last)
{
    uint32_t n = 0;
    while (mo_r != mo_w) {
        uint32_t p = midi_out_q[mo_r++ % MQ];
        if (((p >> 8) & 255u) == status && ((p >> 16) & 127u) == a) {
            if (last) *last = (p >> 24) & 127u;
            n++;
        }
    }
    return n;
}
static uint32_t gates(track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate;
    return n;
}
static void clean(void)
{
    uint32_t i;
    transport_end(); midi_transport_release();
    memset((void *)ml_tab, 0, sizeof ml_tab);
    memset((void *)cc_pending, 0, sizeof cc_pending);
    memset(&mclk, 0, sizeof mclk);
    host_tracks_init();
    panel = PANEL_DEFAULT; fm1_in.notes = fm1_in.buttons = 0;
    kb_prev = 0; song.sel = 0; song.rec = 0;
    arrangement_enabled = 0; arrangement_clock.running = 0;
    for (i = 0; i < NPART; i++) { host_preset(&trk[i], 0, 0); trk[i].p[P_AMODE] = 0; }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    usb.config = 1; fm1_ms += 100u;
    audio(); mo_r = mo_w; mi_r = mi_w;
}
int main(int argc, char **argv)
{
    uint32_t i, v, before;
    uint8_t settings[MIDI_PART];
    outdir = argc > 1 ? argv[1] : "build/host";
    clean(); palette_set(4); layers_init();
    for (i = 0; i < NTRK; i++) {
        const param_desc_t *d;
        int16_t *level = midi_param_slot(&trk[i], P_LEVEL, &d);
        *level = 50;
        midi_cc_local(&trk[i], P_LEVEL, 50, 99);
        fm1_ms += 11; audio();
        check(mo_r == mo_w, "CC OUT defaults off");
        song.g[G_CCTX] = 1;
        midi_cc_local(&trk[i], P_LEVEL, 50, 60);
        midi_cc_local(&trk[i], P_LEVEL, 60, 99);
        fm1_ms += 11; audio();
        check(output(0xB0u | trk_midi_ch(i), 7, &v) == 1 && v == 99, "six channels: coalesced local volume CC, final value retained");
        usb_message(0xB0u | trk_midi_ch(i), 7, 40); audio();
        fm1_ms += 11; audio();
        check(*level == 40 && mo_r == mo_w, "received automation changes its track without echo");
        song.g[G_CCTX] = 0;
    }
    song.g[G_CCTX] = 1;
    midi_cc_local(&trk[4], P_ATK, 0, 127); audio();
    check(mo_r == mo_w, "CC transmission is rate limited");
    fm1_ms += 11; audio();
    check(output(0xB4, 73, &v) == 1 && v == 127, "envelope automation uses standard attack CC");
    trk[4].p[P_E4] = 20;
    midi_cc_local(&trk[4], P_E4, 20, 100); fm1_ms += 11; audio();
    check(output(0xB4, 74, &v) == 1 && v == 100, "cutoff knob uses the same CC74 target as MIDI input");
    usb_message(0xB4, 74, 51); audio();
    check(trk[4].p[P_E4] == 51, "CC74 automation returns to engine cutoff");
    midi_cc_local(&trk[4], P_E0, 0, 127); fm1_ms += 11; audio();
    check(output(0xB4, 20, &v) == 1, "engine slots have stable CC20..27 assignments");
    midi_cc_local(&trk[4], P_PAN, 0, -64); fm1_ms += 11; audio();
    check(output(0xB4, 10, &v) == 1 && v == 0, "bipolar parameter maps its minimum to CC0");
    ml_learn(30, 4, P_LEVEL);
    midi_cc_local(&trk[4], P_LEVEL, 40, 87); fm1_ms += 11; audio();
    check(output(0xB4, 30, &v) == 1 && v == 87, "a learned controller assignment is used for local output");
    usb_message(0xB4, 30, 12); audio();
    check(trk[4].p[P_LEVEL] == 12 && mo_r == mo_w, "learned automation replays without echo");
    memset((void *)ml_tab, 0, sizeof ml_tab);
    ml_learn(31, TRK_DRUM, P_LEVEL);
    midi_cc_local(TDRUM, P_LEVEL, 40, 70); fm1_ms += 11; audio();
    check(output(0xB9, 31, &v) == 1 && v == 70, "learned drum mix controller transmits on channel 10");
    usb_message(0xB9, 31, 33); audio();
    check(song.g[G_DRLVL] == 33 && mo_r == mo_w, "learned drum volume replays to the actual drum mixer");
    memset((void *)ml_tab, 0, sizeof ml_tab);
    song.sel = 4;
    for (i = 0; i < NPAGES; i++) if (PAGES[i].fam == FAM_ENV && PAGES[i].id[0] == P_ATK) break;
    ui.page = (uint8_t)i; ui.home = 0;
    trk[4].p[P_ATK] = 0; edit_param(0, 1); fm1_ms += 11; audio();
    check(output(0xB4, 73, &v) == 1, "actual page knob queues automation");
    for (i = 0; i < CC_PENDING; i++) cc_pending[i] = 0;
    mo_r = mo_w;
    while (mo_w - mo_r < MQ - 8u) midi_out_event(0x00009009u);
    before = mo_w;
    midi_cc_local(&trk[4], P_LEVEL, 0, 50); fm1_ms += 11; audio();
    check(mo_w == before, "CCs leave eight output queue slots reserved for notes");
    mo_r = mo_w; fm1_ms += 11; audio();
    check(output(0xB4, 7, &v) == 1 && v == 50, "backpressured final control value is sent when capacity returns");

    clean();
    { uint8_t note = 72; put_step(&trk[4], 0, 1, &note, ST_NOTE, 0); }
    song.g[G_SYNC] = 2; song.g[G_MIDI] = 1; song.g[G_ROUTE] = 1;
    song.g[G_DAW] = DAW_LIVE; audio();
    check(midi_sync_source() == 1 && !midi_seq_out() && song.g[G_SYNC] == 2 && song.g[G_MIDI] == 1 && song.g[G_ROUTE] == 1,
          "DAW profile overrides routing without changing standalone preferences");
    song.sel = 4;
    fm1_in.notes = 1u << 7; keyboard_block();
    check(!gates(&trk[4]) && output(0x94, kb_nt[7][0], &v) == 1, "DAW local-off: keys send notes without sounding twice");
    usb_message(0x94, 60, 100); audio();
    check(gates(&trk[4]) && mo_r == mo_w, "DAW return plays synth 5 even when standalone IN was CLOCK");
    {
        step_t saved = trk[4].step[0];
        for (i = 0; i < NPAGES; i++) if (PAGES[i].scope == SC_STEP) break;
        ui.page = (uint8_t)i; ui.cursor = 0; ui.home = 0; song.seq_mode = 1; mi_ok = 1;
        step_in_r = step_in_w;
        usb_message(0x94, 65, 100); audio(); midi_entry();
        check(!memcmp(&saved, &trk[4].step[0], sizeof saved), "DAW clips do not overwrite a pattern while the NOTES page is open");
        rec_toggle();
        check(!rec_wait && !song.rec && !ft_on, "DAW profile keeps panel REC from arming an internal take");
        song.seq_mode = 0;
    }
    fm1_in.notes = 0; keyboard_block(); mo_r = mo_w;
    usb_message(0x84, 60, 0); audio();
    usb_message(0xFA, 0, 0); usb_message(0xF8, 0, 0); audio();
    check(trk[4].seq_abs == SEQ_NONE && mo_r == mo_w, "DAW profile disables internal pattern playback");
    usb_message(0xE4, 127, 127); audio();
    check(trk[4].bend > 0, "DAW routes pitch bend despite standalone IN filter");
    usb_message(0xFC, 0, 0); audio();
    check(!song.playing && !gates(&trk[4]) && !trk[4].bend, "DAW STOP releases input notes and controllers");
    song.g[G_DAW] = DAW_CAPTURE; audio();
    check(midi_seq_out() && midi_local(), "Capture enables sequence output and local sound");
    usb_message(0x94, 60, 100); audio();
    check(!gates(&trk[4]), "Capture ignores returned MIDI note-ons");
    input_on(&trk[4], 64, 100);
    usb_message(0x84, 64, 0); audio();
    check(gates(&trk[4]), "Capture also ignores returned note-offs, preserving a local held note");
    input_off(&trk[4], 64);
    song.g[G_DAW] = DAW_STAND; audio();
    check(midi_sync_source() == 2 && midi_seq_out() && song.g[G_ROUTE] == 1, "standalone preferences are restored on profile exit");

    clean(); song.g[G_SYNC] = 1; audio();
    trk[4].p[P_SLEN] = 7; trk[4].p[P_SDIV] = 2;
    { uint8_t note = 63; put_step(&trk[4], 3, 1, &note, ST_NOTE, 0); }
    usb_message(0xF2, 17, 0); audio();
    check(!song.playing && clk_beat == 4 && clk_pos == BEAT_U / 4u && mo_r == mo_w, "SPP17 locates a stopped pattern to beat 4 + one sixteenth");
    song.g[G_MIDI] = 1;
    usb_message(0xFB, 0, 0); audio();
    for (i = 0; i < 20u; i++) audio();
    check(song.playing && trk[4].seq_abs == SEQ_NONE && mo_r == mo_w && clk_beat == 4 && clk_pos == BEAT_U / 4u,
          "CONTINUE freezes at its location until the first clock, even with RUN fallback");
    usb_message(0xF8, 0, 0); audio();
    check(trk[4].seq_idx == 3 && output(0x94, 63, &v) == 1, "first clock after SPP/CONTINUE plays the right step of a seven-step pattern");
    usb_message(0xFC, 0, 0); usb_message(0xF2, 0, 1); usb_message(0xFB, 0, 0); usb_message(0xF8, 0, 0); audio();
    check(song.playing && clk_beat == 32 && trk[4].seq_idx == 2, "STOP/SPP128/CONTINUE/clock in one USB batch preserve packet order");
    output(0, 0, 0);
    usb_message(0xFA, 0, 0); audio();
    check(clk_beat == 0 && clk_pos == 0 && mclk.wait, "START overrides the previous location and waits at zero");
    usb_message(0xF8, 0, 0); audio();
    check(!mclk.wait && trk[4].seq_idx == 0, "first START clock begins at pattern step zero");
    usb_message(0xF2, 127, 127); audio();
    check(clk_beat == 4095 && clk_pos == 3u * (BEAT_U / 4u), "maximum 14-bit SPP does not overflow clock units");
    song.g[G_SPP] = 0; usb_message(0xF2, 0, 0); audio();
    check(clk_beat == 4095, "SPP OFF ignores locate messages");
    song.g[G_SPP] = 1; mclk_locate(0, 2);
    check(clk_beat == 4095, "SPP from the unselected clock source is ignored");
    song.g[G_SYNC] = 2; audio();
    um_byte(0xF2); um_byte(9); um_byte(0xF8); um_byte(1); um_byte(2); um_byte(3);
    audio();
    check(clk_beat == 34 && clk_pos == BEAT_U / 4u, "TRS SPP137 accepts realtime interleaving and cancels running status");

    clean(); song.g[G_SYNC] = 1; audio();
    arr_defaults(&arrangement); arrangement.count = 2; arrangement.loop = 0;
    arrangement.entry[0] = (arr_entry_t){0, 2}; arrangement.entry[1] = (arr_entry_t){1, 3};
    arrangement_enabled = 1;
    usb_message(0xF2, 37, 0); audio();              /* bar 2, second beat + one sixteenth of scene 1 */
    check(!song.playing && arrangement_clock.running && arrangement_clock.index == 1 && arrangement_clock.bar == 0 &&
          arrangement_clock.phase == 5u * (BEAT_U / 4u) && clk_beat == 1 && clk_pos == BEAT_U / 4u,
          "SPP locates the internal song section and its relative pattern phase");
    usb_message(0xFB, 0, 0); usb_message(0xF8, 0, 0); audio();
    usb_message(0xFC, 0, 0); audio();
    check(!song.playing && !arrangement_clock.running && arrangement_clock.index == 1, "song STOP keeps the continuation position");
    usb_message(0xFB, 0, 0); audio();
    check(song.playing && arrangement_clock.running && arrangement_clock.index == 1 && clk_beat == 1,
          "song CONTINUE restores the stopped section rather than playing the standalone loop");
    usb_message(0xF2, 80, 0); audio();
    check(!song.playing && mclk.end, "locating past a non-looping song stops cleanly");
    usb_message(0xFB, 0, 0); audio();
    check(!song.playing, "CONTINUE cannot restart past the end of a non-looping song");
    arrangement.loop = 1;
    usb_message(0xF2, 85, 0); audio();
    check(!mclk.end && arrangement_clock.index == 0 && clk_beat == 1 && clk_pos == BEAT_U / 4u,
          "looping song locates wrap into the correct scene and phase");

    for (i = MLOSS_RUN; i <= MLOSS_STOP; i++) {
        clean(); song.g[G_SYNC] = 1; song.g[G_MLOSS] = (int16_t)i; audio();
        usb_message(0xFA, 0, 0); usb_message(0xF8, 0, 0); audio();
        before = clk_pos; fm1_ms += 501; audio();
        check(i == MLOSS_RUN ? song.playing && clk_pos > before : i == MLOSS_HOLD ? song.playing && clk_pos == before : !song.playing,
              "clock loss respects RUN / HOLD / STOP");
    }
    clean(); song.g[G_SYNC] = 1; song.g[G_MLOSS] = MLOSS_HOLD; audio();
    { uint8_t note = 72; put_step(&trk[4], 0, 1, &note, ST_NOTE, 0); }
    song.g[G_MIDI] = 1; transport_req = 1; audio();
    check(song.playing && mclk.wait == 2 && trk[4].seq_abs == SEQ_NONE && mo_r == mo_w,
          "manual PLAY with HOLD waits for a clock before playing step zero");
    song.g[G_MLOSS] = MLOSS_RUN; audio();
    check(!mclk.wait && trk[4].seq_idx == 0 && output(0x94, 72, &v) == 1,
          "switching clock-loss policy to RUN releases a loss/manual wait");
    clean();
    midi_set[12] = DAW_CAPTURE; midi_set[13] = 1; midi_set[14] = 0; midi_set[15] = MLOSS_STOP;
    midi_part_fill(settings); midi_part_read(settings, sizeof settings);
    check(song.g[G_DAW] == DAW_CAPTURE && song.g[G_CCTX] && !song.g[G_SPP] && song.g[G_MLOSS] == MLOSS_STOP,
          "DAW settings survive backup round trip");
    midi_part_read(settings, MIDI_PART_6T6);
    check(song.g[G_DAW] == DAW_STAND && !song.g[G_CCTX] && song.g[G_SPP] && song.g[G_MLOSS] == MLOSS_RUN,
          "6T6 backup initializes the new DAW settings safely");
    check(midi_part_size(45) && midi_part_size(44) && !midi_part_size(43), "DAW backups append one byte without breaking legacy sizes");
    for (i = 0; i < NPAGES; i++) if (PAGES[i].id[0] == G_DAW && PAGES[i].fam == FAM_GLO) break;
    ui.home = ui.menu = 0; ui.page = (uint8_t)i; vis_on = 0; ui.msg_t = 0;
    song.g[G_DAW] = DAW_CAPTURE; page_entered(); ui.force = 1; frame(); ppm("page-daw-capture-6t7");
    edit_param(1, 1); edit_param(3, 2); ui.force = 1; frame();
    check(song.g[G_CCTX] == 1 && song.g[G_MLOSS] == MLOSS_STOP, "GLO DAW knobs edit CC OUT and clock-loss policy");
    clean(); sec_loads = 0; live_req = -1; song.g[G_RMCH] = 16; remote_poll();
    usb_message(0xBE, 119, 1); audio(); remote_poll();
    check(!remote_section && live_req == -1 && !sec_loads, "section CC ignores other channels");
    usb_message(0xBF, 119, 1); audio(); remote_poll();
    check(live_sec == 1 && sec_loads == 1 && !song.playing, "CC119 loads a valid section while stopped without PLAY");
    usb_message(0xBF, 119, 2); audio(); remote_poll();
    usb_message(0xBF, 119, 127); audio(); remote_poll();
    check(sec_loads == 1, "empty sections and invalid CC119 values leave sounds unchanged");
    song.g[G_DAW] = DAW_LIVE; audio(); song.playing = 1; clk_beat = 1; clk_pos = BEAT_U / 2;
    usb_message(0xBF, 119, 0); audio(); remote_poll();
    check(live_req == 0 && live_sec == 1, "DAW section request queues without changing the current bar");
    clk_beat = 4; clk_pos = 0; audio();
    check(live_req == -1 && live_sec == 0, "DAW profile applies a queued section on the next bar");
    usb_message(0xBF, 119, 1); audio(); remote_poll();
    usb_message(0xFC, 0, 0); audio();
    check(!remote_section && live_req == -1 && !song.playing, "STOP cancels queued and unvalidated section requests");
    arrangement_clock.running = 1;
    usb_message(0xBF, 119, 1); audio(); remote_poll();
    check(live_req == -1 && sec_loads == 1, "section CC does not interrupt a running SONG arrangement");
    arrangement_clock.running = 0;
    usb_message(0xBF, 119, 1); audio(); song.g[G_RMCH] = 0; remote_poll();
    check(!remote_section && sec_loads == 1, "turning REMOTE off drops pending section requests");
    song.g[G_RMCH] = 16; remote_poll();
    song.playing = 1;
    usb_message(0xBF, 119, 1); audio(); section_cancel_on_check = 1; remote_poll();
    check(!song.playing && live_req == -1 && !remote_section && sec_loads == 1,
          "STOP during section validation cannot resurrect a cancelled request");
    printf("DAW integration: %d failures\n", fails);
    return fails != 0;
}
