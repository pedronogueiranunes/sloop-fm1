/* SPDX-License-Identifier: GPL-3.0-only */
/* Six-track MIDI routing, UI and the actual settings serializer / migration. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#include "../firmware/src/midi_settings.c"

static void block(void) { int32_t out[CTL * 2]; mix_block(out, CTL); }
static void packet(uint32_t status, uint32_t a, uint32_t b)
{
    midi_in_q[mi_w++ % MQ] = (status >> 4) | status << 8 | a << 16 | b << 24;
    block();
}
static uint32_t sent(uint32_t status, uint32_t note)
{
    uint32_t n = 0;
    while (mo_r != mo_w) {
        uint32_t e = midi_out_q[mo_r++ % MQ];
        n += ((e >> 8) & 255u) == status && ((e >> 16) & 127u) == note;
    }
    return n;
}
static int gated(track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return 1;
    return 0;
}
int main(int argc, char **argv)
{
    uint32_t i, k;
    uint8_t backup[MIDI_PART], old[MIDI_PART_V27];
    static const uint8_t channels[NTRK] = {1, 2, 3, 4, 5, 10};
    panel = PANEL_DEFAULT;
    layers_init(); palette_set(4); host_tracks_init();
    outdir = argc > 1 ? argv[1] : "build/host";
    for (i = 0; i < NPART; i++) {
        host_preset(&trk[i], 0, 0);
        trk[i].p[P_AMODE] = 0;
    }
    TDRUM->p[P_E0] = DRUM_DEFAULT_KIT;
    usb.config = 1; song.g[G_MIDI] = 1; song.g[G_CHOTH] = 1;
    block(); mo_r = mo_w;
    for (i = 0; i < NTRK; i++) {
        check(song.g[TRK_CH[i]] == channels[i] && midi_own(channels[i] - 1) == &trk[i], "default channel owns the correct track");
        seq_out_on(&trk[i], 60 + i, 100);
        check(sent(0x90u | (channels[i] - 1), 60 + i) == 1, "SEQ note-on on its track channel");
        seq_out_off(&trk[i], 60 + i);
        check(sent(0x80u | (channels[i] - 1), 60 + i) == 1, "SEQ note-off on its track channel");
        seq_out_on(&trk[i], 72 + i, 100); mo_r = mo_w;
        transport_req = 2; block();
        check(sent(0x80u | (channels[i] - 1), 72 + i) == 1, "STOP ends outstanding MIDI notes");
    }
    {   /* The actual transport and sequencer, all six parts in one block. */
        uint32_t mask = 0;
        for (i = 0; i < NPART; i++) {
            uint8_t note = (uint8_t)(60 + i);
            put_step(&trk[i], 0, 1, &note, ST_NOTE, 0);
        }
        dstep_set(&TDRUM->dstep[0], 0, LV_NORM, 0);
        transport_req = 1;
        for (i = 0; i < 8u; i++) block();
        while (mo_r != mo_w) {
            uint32_t e = midi_out_q[mo_r++ % MQ], st = (e >> 8) & 255u;
            for (i = 0; i < NTRK; i++) mask |= st == (0x90u | (channels[i] - 1)) ? 1u << i : 0;
        }
        check(mask == (1u << NTRK) - 1u, "actual sequencer emits all six tracks on 1/2/3/4/5/10");
        transport_req = 2; block(); mo_r = mo_w;
        for (i = 0; i < NTRK; i++) steps_clear(&trk[i]);
    }
    for (i = 3; i < NPART; i++) {
        trk[i].p[P_AMODE] = AM_UP; trk[i].p[P_AOCT] = 1;
        arp_add(&trk[i], 60);
        arp_step(&trk[i]);
        check(sent(0x90u | (channels[i] - 1), 60) == 1, "local arp emits on new synth track channel");
        arp_end(&trk[i]);
        check(sent(0x80u | (channels[i] - 1), 60) == 1, "local arp ends on new synth track channel");
        trk[i].nheld = trk[i].arp_phys = 0; trk[i].p[P_AMODE] = 0;
    }
    for (i = 4; i < NTRK; i++) {
        uint32_t note = i == TRK_DRUM ? 0u : 60u, out = i == TRK_DRUM ? LANE_NOTE[0] : note;
        roll_start(0, &trk[i], note, LV_NORM);
        check(sent(0x90u | (channels[i] - 1), out) == 1, "roll emits on track 5 or drums channel");
        roll_end(0);
        check(sent(0x80u | (channels[i] - 1), out) == 1, "roll ends on track 5 or drums channel");
    }
    for (i = 0; i < NPART; i++) {
        packet(0x90u | (channels[i] - 1), 60 + i, 100);
        check(gated(&trk[i], 60 + i) && mo_r == mo_w, "MIDI note reaches synth with no echo");
        packet(0xB0u | (channels[i] - 1), 64, 127);
        packet(0x80u | (channels[i] - 1), 60 + i, 0);
        check(gated(&trk[i], 60 + i) && sus_on[i], "sustain holds its track note");
        packet(0xB0u | (channels[i] - 1), 64, 0);
        check(!gated(&trk[i], 60 + i), "sustain release ends its track note");
        packet(0xE0u | (channels[i] - 1), 127, 127);
        check(trk[i].bend > 0, "pitch bend reaches its track");
        packet(0xC0u | (channels[i] - 1), 7, 0);
        check(pc_req[i] == 8, "Program Change queued on its track");
        pc_req[i] = 0;
    }
    for (i = 0; i < NPART; i++) {
        track_t *t = &trk[i];
        t->p[P_AMODE] = AM_UP; t->p[P_AOCT] = 2; t->p[P_AHOLD] = 1;
        packet(0x90u | (channels[i] - 1), 60, 100);
        for (k = 0; k < 4u; k++) { arp_end(t); arp_step(t); }
        check(t->nheld && mo_r == mo_w, "MIDI-fed arp sounds without echo, including octave notes");
        packet(0x80u | (channels[i] - 1), 60, 0);
        for (k = 0; k < 4u; k++) { arp_end(t); arp_step(t); }
        check(mo_r == mo_w, "latched MIDI arp stays silent on output after note-off");
        arp_end(t); t->nheld = t->arp_phys = 0;
        t->p[P_AMODE] = t->p[P_AHOLD] = 0;
    }
    {
        track_t *t = &trk[NPART - 1u];
        uint32_t local = 0, echo = 0;
        t->p[P_AMODE] = AM_UP; t->p[P_AOCT] = 1;
        packet(0x94, 60, 100);
        arp_add(t, 64);                        /* a local note in the same held chord */
        for (k = 0; k < 4u; k++) { arp_end(t); arp_step(t); }
        while (mo_r != mo_w) {
            uint32_t e = midi_out_q[mo_r++ % MQ];
            if (((e >> 8) & 255u) == 0x94) {
                echo += ((e >> 16) & 127u) == 60;
                local += ((e >> 16) & 127u) == 64;
            }
        }
        check(local && !echo, "mixed arp sends local notes and suppresses MIDI notes");
        arp_end(t); mo_r = mo_w;
        packet(0x84, 60, 0); arp_remove(t, 64);
        t->p[P_AMODE] = 0;
    }
    k = drums.age; packet(0x99, 36, 100);
    check(drums.age > k && mo_r == mo_w, "channel 10 reaches drums with no echo");
    packet(0xC9, 2, 0); check(pc_req[TRK_DRUM] == 3, "Program Change reaches relocated drums"); pc_req[TRK_DRUM] = 0;
    for (i = 3; i < NPART; i++) {
        uint32_t prev = song.g[TRK_CH[i]];
        seq_out_on(&trk[i], 80, 100); mo_r = mo_w;
        packet(0x90u | (prev - 1), 60, 100);
        song.g[TRK_CH[i]] = (int16_t)(12 + i);
        block();
        check(sent(0x80u | (prev - 1), 80) == 1 && !gated(&trk[i], 60) && !sus_on[i] && !trk[i].bend,
              "CH4/5 change ends old output notes and releases input, pedal and bend");
        song.g[TRK_CH[i]] = (int16_t)prev; block();
    }
    memset((void *)ml_tab, 0, sizeof ml_tab);
    for (i = 0; i < NTRK; i++) {
        check(ml_learn(20 + i, i, P_DIST) == 1, "MIDI LEARN accepts every track");
        packet(0xB0u | (channels[i] - 1), 20 + i, 127);
        check(trk[i].p[P_DIST] == TP[P_DIST].max, "learned CC changes its track");
    }
    midi_part_fill(backup);
    memset((void *)ml_tab, 0, sizeof ml_tab);
    song.g[G_CH4] = midi_set[10] = 13; song.g[G_CH5] = midi_set[11] = 14;
    midi_part_read(backup, sizeof backup);
    check(song.g[G_CH4] == 4 && song.g[G_CH5] == 5 && song.g[G_DRCH] == 10, "six channels survive backup round trip");
    for (i = 0; i < NTRK; i++)
        check(ml_track(ml_tab[i]) == i && ML_CODE(ml_tab[i]) == P_DIST + 1u, "learned track survives backup round trip");
    {
        uint8_t custom[MIDI_PART];
        midi_set[10] = 7; midi_set[11] = 8; midi_set[3] = 12;
        midi_part_fill(custom);
        midi_set[10] = midi_set[11] = midi_set[3] = 0;
        midi_part_read(custom, sizeof custom);
        check(song.g[G_CH4] == 7 && song.g[G_CH5] == 8 && song.g[G_DRCH] == 12 &&
              midi_set[10] == 7 && midi_set[11] == 8 && midi_set[3] == 12,
              "custom new synth and drum channels persist and restore");
        midi_part_read(backup, sizeof backup);
    }
    memcpy(old, backup, sizeof old);
    old[6] = 23u | 128u; old[7] = (uint8_t)((3u << 7 | (P_DIST + 1u) << 9) >> 8); /* legacy slot 3 = drums */
    for (k = 2u * ML_N; k <= MIDI_PART_V27; k += 4u) {
        song.g[G_CH4] = midi_set[10] = 13; song.g[G_CH5] = midi_set[11] = 14;
        midi_part_read(old, k);
        check(midi_part_size(k) && song.g[G_CH4] == 4 && song.g[G_CH5] == 5 && ml_track(ml_tab[3]) == TRK_DRUM,
              "32/36/40-byte legacy restore resets new channels and maps old drums");
    }
    memcpy(old, backup, sizeof old); old[2u * ML_N] = 4; old[2u * ML_N + 1] = 5;
    midi_part_read(old, sizeof old);
    check(song.g[G_CH1] == 4 && song.g[G_CH2] == 5 && song.g[G_CH4] == 6 && song.g[G_CH5] == 7 && song.g[G_DRCH] == 10,
          "legacy restore preserves old channels and gives new tracks free channels");
    midi_part_read(backup, MIDI_PART_V6);
    check(song.g[G_CH4] == 4 && song.g[G_CH5] == 5, "existing six-track 42-byte backup still restores");
    check(!midi_part_size(31) && !midi_part_size(MIDI_PART - 2), "truncated MIDI backups rejected");
    for (i = 0; i < NPAGES; i++) if (midi_page(&PAGES[i])) break;
    ui.page = (uint8_t)i; ui.home = 0; page_entered();
    for (i = 0; i < NTRK; i++) {
        song.sel = (uint8_t)i; ui.force = 1; frame();
        check(i >= trk_view_top() && i < trk_view_top() + 4u, "MIDI list follows selected track");
        if (i == TRK_DRUM) ppm("midi-six-tracks");
    }
    check(page_id(cur_page(), 2) == G_CH5 && page_id(cur_page(), 3) == G_DRCH, "last MIDI viewport edits track 5 and drums");
    edit_param(2, 1); midi_chan_fix();
    check(song.g[G_CH5] == 6, "visible knob 3 edits track 5");
    edit_param(3, 1); midi_chan_fix();
    check(song.g[G_DRCH] == 11, "visible knob 4 edits drums");
    printf("midi tracks: %d failures\n", fails);
    return fails != 0;
}
