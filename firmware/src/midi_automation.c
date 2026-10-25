/* SPDX-License-Identifier: GPL-3.0-only */
/* Local panel edits only. The UI coalesces four controls; the audio side owns MIDI OUT.
 * No scan of live parameters: MIDI input, preset loads and parameter locks never become CC echoes. */
#define CC_PENDING 4u
static volatile uint32_t cc_pending[CC_PENDING];
static uint32_t cc_tx_ms;

static uint32_t midi_cc_value(const param_desc_t *d, int32_t v)
{
    return (uint32_t)((clamp(v, d->min, d->max) - d->min) * 127 + (d->max - d->min) / 2) / (uint32_t)(d->max - d->min);
}
static void midi_cc_local(track_t *t, uint32_t id, int32_t old, int32_t value)
{
    const param_desc_t *d, *rd;
    int16_t *slot;
    uint32_t i, j, cc = 128u, ti = trk_index(t), empty = CC_PENDING, packet;
    if (!song.g[G_CCTX] || !usb.config || old == value || trk_midi_ch(ti) > 15u)
        return;
    slot = midi_param_slot(t, id, &d);
    if (!slot || !d || d->max <= d->min || midi_cc_value(d, old) == midi_cc_value(d, value))
        return;
    for (i = 0; i < ML_N; i++) {
        uint32_t e = ml_tab[i];
        if (ML_CODE(e) && ml_track(e) == ti && ml_id(ML_CODE(e)) == id) {
            cc = e & 127u;
            break;
        }
    }
    if (cc == 128u) {
        for (i = 0; i < NELEM(MIDI_CC_MAP) + 8u; i++) {
            uint32_t candidate = i < NELEM(MIDI_CC_MAP) ? MIDI_CC_MAP[i][0] : 20u + i - NELEM(MIDI_CC_MAP);
            if (midi_cc_target(t, candidate, &rd) != slot)
                continue;
            for (j = 0; j < ML_N; j++)
                if (ML_CODE(ml_tab[j]) && (ml_tab[j] & 127u) == candidate)
                    break;                      /* a learned CC belongs to its learned target, on every channel */
            if (j == ML_N) { cc = candidate; break; }
        }
    }
    if (cc > 127u)
        return;
    packet = 0x0Bu | ti << 4 | cc << 16 | midi_cc_value(d, value) << 24;
    fm1_irq_off();
    for (i = 0; i < CC_PENDING; i++) {
        uint32_t was = cc_pending[i];
        if ((was & 0x007F007Fu) == (packet & 0x007F007Fu)) { empty = i; break; }
        if (!was) empty = i;
    }
    if (empty < CC_PENDING)
        cc_pending[empty] = packet;
    fm1_irq_on();
}
static void midi_cc_global(uint32_t id, int32_t old, int32_t value)
{
    if (id == G_DRLVL || id == G_DRREV || id == G_DRDLY)
        midi_cc_local(TDRUM, id == G_DRLVL ? P_LEVEL : id == G_DRREV ? P_REV : P_DLY, old, value);
}
static void midi_cc_flush(void)
{
    uint32_t i;
    if (!song.g[G_CCTX] || !usb.config) {
        for (i = 0; i < CC_PENDING; i++) cc_pending[i] = 0;
        return;
    }
    if (fm1_ms - cc_tx_ms < 10u)
        return;                                  /* at most 100 updates/s per control, four controls at once */
    for (i = 0; i < CC_PENDING && mo_w - mo_r < MQ - 8u; i++) {
        uint32_t p = cc_pending[i], ch;
        if (!p) continue;
        cc_pending[i] = 0;
        ch = trk_midi_ch((p >> 4) & 7u);
        if (ch <= 15u)
            midi_out_event((p & 0xFFFF0000u) | (0xB0u | ch) << 8 | 0x0Bu);
    }
    cc_tx_ms = fm1_ms;                            /* leave eight queue slots for notes and transport */
}
