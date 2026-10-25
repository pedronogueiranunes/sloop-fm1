/* SPDX-License-Identifier: GPL-3.0-only */
/* Recall ordering against the real engine/preset descriptors, including mode-dependent ranges. */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
#include <assert.h>
int main(void)
{
    uint32_t e, pi, mode, j, k, values = 0;
    int16_t savedp[P_COUNT];
    host_tracks_init(); song.sel = 0;
    for (e = 0; e < NENGINES; e++)
        for (pi = 0; pi < ENGINES[e]->npresets; pi++)
            for (mode = 0; mode < 3; mode++) {
                set_engine_of(TSEL, e); apply_preset(pi);
                if (mode) {
                    TSEL->p[P_ESYNC] = (int16_t)(mode - 1u);
                    TSEL->p[P_ITYPE] = (int16_t)mode;
                    for (j = 0; j < P_COUNT; j++) {
                        const param_desc_t *d = track_desc(TSEL, j);
                        if (j != P_ESYNC && j != P_ITYPE && d->max > d->min)
                            TSEL->p[j] = (int16_t)(d->min + (j * 11u + e * 7u + mode) % (uint32_t)(d->max - d->min + 1));
                    }
                }
                memcpy(savedp, TSEL->p, sizeof savedp);
                set_engine_of(TSEL, (e + 1u) % NENGINES);
                set_engine_of(TSEL, e); apply_preset(pi);
                /* This is the generated Max schema's restore order: modes first, then the remaining ids. */
                for (k = 0; k < P_COUNT + 2u; k++) {
                    const param_desc_t *d;
                    j = k == 0 ? P_ESYNC : k == 1 ? P_ITYPE : k - 2u;
                    if ((k >= 2u && (j == P_ESYNC || j == P_ITYPE)) || (j >= P_SLEN && j <= P_SGATE)) continue;
                    d = track_desc(TSEL, j);
                    if (d->max > d->min) TSEL->p[j] = (int16_t)clamp(savedp[j], d->min, d->max);
                    assert(TSEL->p[j] == savedp[j]);
                    values++;
                }
            }
    printf("Live Recall: %u values restored with real preset/engine ranges, edited modes, ENV SYNC and INSERT ordering\n", values);
    return 0;
}
