# Liveloop visual identity

Liveloop is Pedro Nogueira Nunes's independent fork of [SLOOP](https://github.com/isod89/sloop-fm1), built on [Felucca](https://github.com/hugelton/Felucca). This document is a design specification, not an indication that the firmware UI has already been changed.

## Naming and attribution

- Product name: **Liveloop** (one word).
- Creator and musician: Pedro Nogueira Nunes (peeeo).
- Credit SLOOP by isod89 and Felucca by Leo Kuroshita / Hügelton Instruments wherever project origins are described.
- Do not imply endorsement or official affiliation with Ableton. The name references a live music workflow and SLOOP's heritage, not ownership of Ableton Live or its trademarks.
- Preserve existing copyright notices, GPL-3.0 licensing, and third-party licenses.

## Logos

Original supplied artwork, kept as vector paths:

- [White logo](../assets/branding/liveloop-white.svg): for dark backgrounds.
- [Black logo](../assets/branding/liveloop-black.svg): for light backgrounds.

Do not recreate, stretch, recolor or replace the supplied artwork casually. Provide appropriate contrast and padding.

## Type

- Preferred interface family: **Rubik**.
- Rubik Regular 400 for secondary text, Medium 500 for labels and controls, Bold 700 for major values and headings.
- Firmware usage is **proposed, not implemented**. Benchmark the existing Terminus bitmap font against a Rubik conversion first. Compare glyph data in flash, RAM and pool headroom, UI frame/render cost, glyph coverage, and small-screen legibility.
- Keep font licensing and attribution with any future bundled font files; do not add font files until integration is approved.
- Screen constraints: 240 × 240 pixels; preserve readable sizes, parameter alignment, existing controls, and accessibility.

## Colors: Vibrant Rainbow v1

From top to bottom, track 1 to track 6 (drums). These are the canonical hex colors for brand and editor mockups; firmware RGB565 conversion may round them.

| Track | Role | Name | HEX |
|---|---|---|---|
| 1 | Synth | Violet | `#AD85F7` |
| 2 | Synth | Blue | `#598FF5` |
| 3 | Synth | Cyan | `#43D2E8` |
| 4 | Synth | Mint | `#54D9A0` |
| 5 | Synth | Yellow | `#F3D35C` |
| 6 | Drums | Orange | `#FF862E` |

The order creates a cool-to-warm spectrum moving **down** toward the orange drums. Use each track color consistently across meters, steps, mixer labels, and indicators. Never use color as the only signifier of track identity; include track numbers and state labels.

### Proposed neutral foundation (not final)

| Token | HEX | Use |
|---|---|---|
| Background | `#14171C` | Main UI |
| Surface | `#252A32` | Elevated controls |
| Foreground | `#F6F6F3` | Primary labels and values |
| Muted | `#949DA9` | Secondary text |

Reserve white or a high-contrast neutral for focus and selection. Check contrast on RGB565 hardware before committing the palette to firmware.

## Integration plan (not implemented in this branch)

1. Build accurate 240 × 240 UI previews using existing navigation and four-column control mapping.
2. Benchmark Rubik versus Terminus with `tools/gen_font.py` and `tests/font_test.c`; verify flash/RAM placement and measured cost.
3. On a future firmware integration branch, update all six track colors in `firmware/src/ui.c` / `firmware/src/gfx.c` without changing track numbering or behavior.
4. Extend custom themes to six tracks only with a backwards-compatible format and synchronized web-editor changes. Current themes store only four track colors.
5. Test on-device, maintaining newly numbered builds and preserving SLOOP attribution.

This branding branch intentionally avoids firmware source, web editor implementation, project formats, and MIDI features.
