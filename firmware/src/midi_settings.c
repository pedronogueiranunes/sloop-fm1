/* SPDX-License-Identifier: GPL-3.0-only */
/* MIDI settings and legacy backup migration. Included by project.c; also exercised on the host. */
#define MIDI_PART_V27 (2u * ML_N + 8u)          /* as 2.6 .. 2.8 wrote it */
#define MIDI_PART_V6 (2u * ML_N + 10u)             /* sloop-peeeo: then CH4, CH5 (tracks 4 and 5) */
#define MIDI_PART_6T6 (MIDI_PART_V6 + (ML_N + 7u) / 8u)   /* one extra track bit per learned CC; old map bytes unchanged */
#define MIDI_PART (MIDI_PART_6T6 + 1u)   /* MODE bits 0..1, CC OUT bit 2, SPP bit 3, LOST bits 4..5 */
static int midi_part_size(uint32_t n)
{
    return n == 2u * ML_N || n == 2u * ML_N + 4u || n == MIDI_PART_V27 || n == MIDI_PART_V6 || n == MIDI_PART_6T6 || n == MIDI_PART;
}
static uint8_t midi_saved[MIDI_PART];
UI_SMALL static void midi_part_fill(uint8_t *b)
{
    uint32_t k;
    memset(b + MIDI_PART_V6, 0, MIDI_PART - MIDI_PART_V6);
    for (k = 0; k < ML_N; k++) {
        b[2u * k] = (uint8_t)ml_tab[k], b[2u * k + 1u] = (uint8_t)(ml_tab[k] >> 8);
        b[MIDI_PART_V6 + k / 8u] |= (uint8_t)(((ml_tab[k] >> 16) & 1u) << (k % 8u));
    }
    b[2u * ML_N] = midi_set[0], b[2u * ML_N + 1u] = midi_set[1], b[2u * ML_N + 2u] = midi_set[2];
    b[2u * ML_N + 3u] = (uint8_t)(midi_set[3] | (drum_preview != 0u) << 5 | (step_adv_off != 0u) << 6 | midi_set[4] << 7);
                                                    /* (2.7.1: bit 5 DRUM PREVIEW, bit 6 STEP ADVANCE OFF: bits 2.7 does not read) */
    b[2u * ML_N + 4u] = (uint8_t)(midi_set[5] | (fxfade > 15u ? 15u : fxfade) << 4);   /* BEND (bits 0-3); 2.7.2: FX FADE bits 4-7 (0 = none: the byte reads as 2.6's BEND) */
    b[2u * ML_N + 5u] = midi_set[6];                /* (2.7) GLO > MIDI 3: FX CH (2.6 read this spare byte as nothing) */
    b[2u * ML_N + 6u] = (uint8_t)(midi_set[7] | midi_set[9] << 1 |
                                  (flt_reso > 10u ? 10u : flt_reso) << 2);   /* (2.7) GLO > MIDI 3: DR MAP, bit 1 CLK OUT
                                                                      * (a spare byte to 2.6); 2.7.2: FILTER RESO in bits
                                                                      * 2..5 (0 = none: the byte then reads as 2.7's) */
    b[2u * ML_N + 7u] = midi_set[8];                /* (2.7) GLO > MIDI 3: REMOTE (the last spare byte) */
    b[MIDI_PART_6T6] = (uint8_t)(midi_set[12] | midi_set[13] << 2 | midi_set[14] << 3 | midi_set[15] << 4);
    b[2u * ML_N + 8u] = midi_set[10], b[2u * ML_N + 9u] = midi_set[11];   /* sloop-peeeo: CH4, CH5 */
}
UI_SMALL static int midi_part_default(const uint8_t *b)
{
    static const uint8_t def[sizeof MIDI_SET_ID] = MIDI_SET_DEF;
    uint32_t k;
    for (k = 0; k < 2u * ML_N; k++)
        if (b[k])
            return 0;
    for (k = MIDI_PART_V6; k < MIDI_PART_6T6; k++)
        if (b[k])
            return 0;
    return b[MIDI_PART_6T6] == (uint8_t)(def[12] | def[13] << 2 | def[14] << 3 | def[15] << 4) &&
           b[2u * ML_N] == def[0] && b[2u * ML_N + 1u] == def[1] && b[2u * ML_N + 2u] == def[2] &&
           b[2u * ML_N + 3u] == (uint8_t)(def[3] | def[4] << 7) && b[2u * ML_N + 4u] == def[5] && b[2u * ML_N + 5u] == def[6] &&
           b[2u * ML_N + 6u] == (uint8_t)(def[7] | def[9] << 1) && b[2u * ML_N + 7u] == def[8] &&
           b[2u * ML_N + 8u] == def[10] && b[2u * ML_N + 9u] == def[11];
}
/* n: 32 (the map), 36 (and the channels) or MIDI_PART; every entry checked (a CC that learns, a parameter this knows, a
 * channel 0..16, a range 1..12) */
UI_SMALL static void midi_part_read(const uint8_t *b, uint32_t n)
{
    static const uint8_t def[sizeof MIDI_SET_ID] = MIDI_SET_DEF;
    static const uint8_t order[NTRK] = {0, 1, 2, TRK_DRUM, 3, 4};   /* preserve the old tracks before assigning the new ones */
    uint32_t k, e, used = 0;
    if (!midi_part_size(n))
        return;
    fm1_irq_off();                                  /* (the ISR reads the map and the channels: never half of them) */
    memcpy(midi_set, def, sizeof midi_set);
    for (k = 0; k < ML_N; k++) {
        e = (uint32_t)b[2u * k] | (uint32_t)b[2u * k + 1u] << 8;
        if (n >= MIDI_PART_6T6)
            e |= (uint32_t)((b[MIDI_PART_V6 + k / 8u] >> (k % 8u)) & 1u) << 16;
        ml_tab[k] = ML_CODE(e) && ml_free_cc(e & 127u) && ml_id(ML_CODE(e)) < P_COUNT &&
                    ml_track(e) < NTRK && ml_entry(e & 127u, ml_track(e), ML_CODE(e)) == e ? e : 0u;
    }
    if (n >= 2u * ML_N + 4u) {
        for (k = 0; k < 3u; k++)
            midi_set[k] = (uint8_t)(b[2u * ML_N + k] <= 16u ? b[2u * ML_N + k] : k + 1u);
        midi_set[3] = (uint8_t)((b[2u * ML_N + 3u] & 31u) <= 16u ? b[2u * ML_N + 3u] & 31u : 10u);
        midi_set[4] = (uint8_t)(b[2u * ML_N + 3u] >> 7);
        drum_preview = (uint8_t)((b[2u * ML_N + 3u] >> 5) & 1u);   /* (2.7.1) menu DRUM PREVIEW; older: OFF */
        step_adv_off = (uint8_t)((b[2u * ML_N + 3u] >> 6) & 1u);   /* (2.7.1) menu STEP ADVANCE; older: ON */
    }
    if (n >= MIDI_PART_V27) {
        midi_set[5] = (uint8_t)((b[2u * ML_N + 4u] & 15u) >= 1u && (b[2u * ML_N + 4u] & 15u) <= 12u ? b[2u * ML_N + 4u] & 15u : 2u);
        fxfade = (uint8_t)((b[2u * ML_N + 4u] >> 4) & 15u);         /* (2.7.2) FX FADE (bits 4-7); older: 0 */
        song.g[G_FXFADE] = fxfade;
        midi_set[6] = (uint8_t)(b[2u * ML_N + 5u] <= 16u ? b[2u * ML_N + 5u] : 0u);   /* (2.7) FX CH */
        midi_set[7] = (uint8_t)(b[2u * ML_N + 6u] & 1u);             /* (2.7) DR MAP */
        midi_set[9] = (uint8_t)((b[2u * ML_N + 6u] >> 1) & 1u);      /* (2.7) CLK OUT */
        flt_reso = (uint8_t)((b[2u * ML_N + 6u] >> 2) & 15u);        /* (2.7.2) FILTER RESO (bits 2..5); older: 0 */
        if (flt_reso > 10u)
            flt_reso = 10u;
        midi_set[8] = (uint8_t)(b[2u * ML_N + 7u] <= 16u ? b[2u * ML_N + 7u] : 0u);   /* (2.7) REMOTE */
    }
    if (n >= MIDI_PART_V6) {                        /* CH4, CH5; older backups use the defaults even during a restore */
        midi_set[10] = (uint8_t)(b[2u * ML_N + 8u] <= 16u ? b[2u * ML_N + 8u] : 4u);
        midi_set[11] = (uint8_t)(b[2u * ML_N + 9u] <= 16u ? b[2u * ML_N + 9u] : 5u);
    }
    if (n >= MIDI_PART) {
        uint32_t options = b[MIDI_PART_6T6];
        midi_set[12] = (uint8_t)((options & 3u) <= DAW_CAPTURE ? options & 3u : DAW_STAND);
        midi_set[13] = (uint8_t)((options >> 2) & 1u);
        midi_set[14] = (uint8_t)((options >> 3) & 1u);
        midi_set[15] = (uint8_t)(((options >> 4) & 3u) <= MLOSS_STOP ? (options >> 4) & 3u : MLOSS_RUN);
    }
    for (k = 0; k < sizeof MIDI_SET_ID; k++)
        song.g[MIDI_SET_ID[k]] = midi_set[k];
    for (k = 0; k < NTRK; k++) {
        uint32_t id = TRK_CH[order[k]], ch = (uint32_t)song.g[id];
        while (ch && (used & (1u << ch)))
            ch = ch < 16u ? ch + 1u : 1u;
        song.g[id] = (int16_t)ch;
        used |= ch ? 1u << ch : 0u;
    }
    for (k = 0; k < sizeof MIDI_SET_ID; k++)
        midi_set[k] = (uint8_t)song.g[MIDI_SET_ID[k]];
    fm1_irq_on();
}
