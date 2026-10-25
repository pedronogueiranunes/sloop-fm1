/* SPDX-License-Identifier: GPL-3.0-only */
/* Position changes are applied between MIDI packets, before notes at the new position play. */
static void midi_transport_release(void)
{
    uint32_t i;
    for (i = 0; i < 27u; i++) if (kb_kind[i]) key_up(i);
    kb_prev = fm1_in.notes;                        /* held keys must be released before they play again */
    for (i = 0; i < NROLL; i++) roll_end(i);
    seq_out_all_off();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        seq_release(t); arp_end(t); kh_release(t); trk_all_off(t);
        t->nheld = t->arp_phys = 0; t->bend = 0;
        sus_on[i] = 0;
    }
    memset(sus_wait, 0, sizeof sus_wait);
    memset(ch_sus, 0, sizeof ch_sus);
    memset(ch_bend, 0, sizeof ch_bend);
    memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(mi_pen, 0, sizeof mi_pen);
}
static void mclk_locate(uint32_t spp, uint32_t source)
{
    uint32_t playing = song.playing, beat = spp / 4u, pos = (spp % 4u) * (BEAT_U / 4u);
    if (!song.g[G_SPP] || !midi_sync_source() || source != midi_sync_source() || spp > 16383u)
        return;
    midi_transport_release();
    song.rec = rec_wait = ci_on = 0;              /* seeking never writes an accidental partial take */
    if (ft_on) { ft_on = 0; ft_bars = 0xFF; }
    mclk.pos = mclk.done = 0;
    mclk.have = mclk.end = 0;
    mclk.seeked = mclk.wait = 1;
#if FELUCCA_ARRANGER
    if (arrangement_enabled && song.g[G_DAW] != DAW_LIVE) {
        int scene;
        if (!arrangement_clock.running) song_backup();
        scene = arr_locate(&arrangement_clock, &arrangement, arrangement_ready(), spp, FS);
        if (scene < 0) {
            arrangement_clock.running = 1;        /* STOP restores the loop kept before the song */
            transport_end();
            mclk.end = 1;
            return;
        }
        arrangement_apply((uint32_t)scene);
        beat = (uint32_t)arrangement_clock.bar * 4u + (spp % 16u) / 4u;
    }
    live_req = -1; remote_section = 0; chain_n = 0;
#endif
    seq_reset_tracks(pos);
    clk_beat = beat;
    song.playing = (uint8_t)playing;              /* SPP locates; only START/CONTINUE begin playback */
}

static void mclk_continue(void)
{
#if FELUCCA_ARRANGER
    if (arrangement_enabled && song.g[G_DAW] != DAW_LIVE && !arrangement_clock.running) {
        uint32_t phase = arrangement_clock.phase;
        if (!arr_valid(&arrangement, arrangement_ready()) || arrangement_clock.index >= arrangement.count) {
            mclk.end = 1;
            return;
        }
        song_backup();
        arrangement_apply(arrangement.entry[arrangement_clock.index].scene);
        seq_reset_tracks(phase % BEAT_U);
        clk_beat = (uint32_t)arrangement_clock.bar * 4u + phase / BEAT_U;
        arrangement_clock.running = 1;
    }
#endif
    song.playing = 1;
}
