Here is a Mega Drive game written with SGDK 2.09. Make it run faster on the real hardware (68000 at 7.67 MHz) without changing what the game does or how it looks. Reply with the full main.c in one ```c code block.

Below is a line profile of this code, measured in an emulator with a scripted pad: cycles per video frame by source line (one NTSC frame is 128,005 cycles), what each line called, and the findings of a performance lint on the same lines.

== Lines by measured cost (sgdkhot: mdlines + sgdklint) ==
budget 128,005 cycles per video frame (NTSC); 1,200 frames counted; busy 24,333 cycles per frame on average (19.0%)
each line = its own instructions + every library/libgcc call made from it (game helpers count on their own lines)
    7,377 /frame  30.3% of busy  src/main.c:57  buf[digits - 1 - i] = '0' + (char)(value % 10);
                     6,673 in __umodsi3: 8.00 calls/frame, 834 cycles each
                 lint [div] hot: '%' on 32-bit / constant: 1,041 cycles (s32 % s32); u16 % u16 is 126
    3,662 /frame  15.0% of busy  src/main.c:62  VDP_drawTextBG(BG_A, buf, x, y);
                     2,418 in VDP_setTileMapDataRowEx.constprop.0: 2.00 calls/frame, 1,209 cycles each
                 lint [text] frame: VDP_drawTextBG on the frame path (not measured; VDP_drawText of 19 characters is 3,673 cycles); redraw only wh
    3,421 /frame  14.1% of busy  src/main.c:178  SYS_doVBlankProcess();
                     3,393 in SYS_doVBlankProcessEx.constprop.0.isra.0: 1.00 calls/frame, 3,393 cycles each
    2,811 /frame  11.6% of busy  src/main.c:58  value /= 10;
                     2,477 in __udivsi3: 6.00 calls/frame, 413 cycles each
                 lint [div] hot: '/' on 32-bit / constant: about 623 cycles (measured: s32 / 10 is 623, (u16 - 240) / 30 is 640; u16 / 10 is 77
    2,748 /frame  11.3% of busy  src/main.c:96  VDP_setTileMapXY(BG_A,
                 lint [tile-loop] hot: VDP_setTileMapXY in a loop: 324 cycles per tile; VDP_fillTileMapRect is 18 per tile, VDP_setTileMapDataRect 37
    2,006 /frame   8.2% of busy  src/main.c:79  VDP_drawTextBG(BG_A, buf, 7, 2);
                     1,252 in VDP_setTileMapDataRowEx.constprop.0: 1.00 calls/frame, 1,252 cycles each
                 lint [text] frame: VDP_drawTextBG on the frame path (not measured; VDP_drawText of 19 characters is 3,673 cycles); redraw only wh
      351 /frame   1.4% of busy  src/main.c:93  else if (remaining >= 8) fill = 8;
      344 /frame   1.4% of busy  src/main.c:72  buf[0] = '0' + (char)((minutes / 10) % 10);
      318 /frame   1.3% of busy  [no game line]:0  
                       124 in no_bmp_task
                       124 in no_user_task
                        30 in no_xgm_task
      294 /frame   1.2% of busy  src/main.c:89  s16 remaining = (s16)health - (s16)(i * 8);
      259 /frame   1.1% of busy  src/main.c:92  if (remaining <= 0)      fill = 0;
      182 /frame   0.7% of busy  src/main.c:75  buf[3] = '0' + (char)(secs / 10);
      180 /frame   0.7% of busy  src/main.c:73  buf[1] = '0' + (char)(minutes % 10);
       88 /frame   0.4% of busy  src/main.c:69  u16 minutes = seconds / 60;
       68 /frame   0.3% of busy  src/main.c:70  u16 secs    = seconds % 60;

== Lint findings, by measured cost ==
   matters     7,377 /frame (5.76% of a frame)  src/main.c:57 [div] hot
   matters     3,662 /frame (2.86% of a frame)  src/main.c:62 [text] frame
   matters     2,811 /frame (2.20% of a frame)  src/main.c:58 [div] hot
   matters     2,748 /frame (2.15% of a frame)  src/main.c:96 [tile-loop] hot
   matters     2,006 /frame (1.57% of a frame)  src/main.c:79 [text] frame
verdicts: matters >= 1% of a frame, small >= 0.2%, negligible below, not run = no cycles on that line in this run

== Game lines over 2% of a frame with no lint finding ==
    3,421 /frame  src/main.c:178 (main), mostly SYS_doVBlankProcessEx.constprop.0.isra.0

library code with no game line on the stack (interrupt handlers, SGDK's VBlank work): 318 cycles per frame

main.c:
#include <genesis.h>

/* ---------- Layout / tuning ---------- */
#define HUD_TILE_BASE    TILE_USER_INDEX  /* first VRAM tile used by the HUD bar */
#define HUD_TILE_COUNT   9                /* tile 0 = empty, tiles 1..8 = 1..8 px filled */
#define BAR_SEGMENTS     10               /* number of 8x8 tiles in the health bar */
#define BAR_MAX          (BAR_SEGMENTS * 8)   /* health in pixels: 80 */
#define START_LIVES      3
#define START_TIME_SEC   180              /* 3:00 countdown */
#define SCORE_MAX        999999UL

/* ---------- HUD tile graphics (4bpp, 8x8, 8 rows of u32 each) ----------
 * Each tile is filled from the left. Colour index 1 = green (PAL0),
 * colour index 0 = transparent/background.
 * Row value: pixel i (left to right) occupies nibble (28 - 4*i). */
static const u32 hud_tiles[HUD_TILE_COUNT * 8] =
{
    /* tile 0: empty */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* tile 1: 1 px filled */
    0x10000000, 0x10000000, 0x10000000, 0x10000000,
    0x10000000, 0x10000000, 0x10000000, 0x10000000,
    /* tile 2: 2 px filled */
    0x11000000, 0x11000000, 0x11000000, 0x11000000,
    0x11000000, 0x11000000, 0x11000000, 0x11000000,
    /* tile 3: 3 px filled */
    0x11100000, 0x11100000, 0x11100000, 0x11100000,
    0x11100000, 0x11100000, 0x11100000, 0x11100000,
    /* tile 4: 4 px filled */
    0x11110000, 0x11110000, 0x11110000, 0x11110000,
    0x11110000, 0x11110000, 0x11110000, 0x11110000,
    /* tile 5: 5 px filled */
    0x11111000, 0x11111000, 0x11111000, 0x11111000,
    0x11111000, 0x11111000, 0x11111000, 0x11111000,
    /* tile 6: 6 px filled */
    0x11111100, 0x11111100, 0x11111100, 0x11111100,
    0x11111100, 0x11111100, 0x11111100, 0x11111100,
    /* tile 7: 7 px filled */
    0x11111110, 0x11111110, 0x11111110, 0x11111110,
    0x11111110, 0x11111110, 0x11111110, 0x11111110,
    /* tile 8: full */
    0x11111111, 0x11111111, 0x11111111, 0x11111111,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

/* ---------- Helpers ---------- */

/* Draw an unsigned value as a fixed-width, zero-padded decimal number. */
static void drawNumber(u16 x, u16 y, u32 value, u16 digits)
{
    char buf[12];
    u16 i;

    for (i = 0; i < digits; i++)
    {
        buf[digits - 1 - i] = '0' + (char)(value % 10);
        value /= 10;
    }
    buf[digits] = '\0';

    VDP_drawTextBG(BG_A, buf, x, y);
}

/* Draw the countdown as MM:SS. */
static void drawTimer(u16 seconds)
{
    char buf[6];
    u16 minutes = seconds / 60;
    u16 secs    = seconds % 60;

    buf[0] = '0' + (char)((minutes / 10) % 10);
    buf[1] = '0' + (char)(minutes % 10);
    buf[2] = ':';
    buf[3] = '0' + (char)(secs / 10);
    buf[4] = '0' + (char)(secs % 10);
    buf[5] = '\0';

    VDP_drawTextBG(BG_A, buf, 7, 2);
}

/* Draw the health bar: each segment shows 0..8 filled pixels. */
static void drawHealth(u16 health)
{
    u16 i;

    for (i = 0; i < BAR_SEGMENTS; i++)
    {
        s16 remaining = (s16)health - (s16)(i * 8);
        u16 fill;

        if (remaining <= 0)      fill = 0;
        else if (remaining >= 8) fill = 8;
        else                     fill = (u16)remaining;

        VDP_setTileMapXY(BG_A,
                         TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, HUD_TILE_BASE + fill),
                         10 + i, 8);
    }
}

/* ---------- Entry point ---------- */
int main(bool hardReset)
{
    /* Palette 0: 0 = background (black), 1 = green (bar), 2 = red, 3 = yellow,
     * 15 = white (text font colour). Genesis colour format: 0000BBB0GGG0RRR0. */
    u16 palette[16] =
    {
        0x0000, 0x00E0, 0x000E, 0x00EE,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0EEE
    };

    u16 joy;
    u16 fpsTarget;
    u16 frameTick = 0;
    u16 timeLeft  = START_TIME_SEC;
    u32 score     = 0;
    u16 lives     = START_LIVES;
    u16 health    = BAR_MAX;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    PAL_setPalette(PAL0, palette, DMA);
    VDP_loadTileData(hud_tiles, HUD_TILE_BASE, HUD_TILE_COUNT, FALSE);

    /* Static labels */
    VDP_drawTextBG(BG_A, "TIME",   2, 2);
    VDP_drawTextBG(BG_A, "SCORE",  2, 4);
    VDP_drawTextBG(BG_A, "LIVES",  2, 6);
    VDP_drawTextBG(BG_A, "HEALTH", 2, 8);

    fpsTarget = IS_PAL_SYSTEM ? 50 : 60;

    while (TRUE)
    {
        joy = JOY_readJoypad(JOY_1);

        /* A held: +10 score every frame */
        if (joy & BUTTON_A)
        {
            score += 10;
            if (score > SCORE_MAX) score = SCORE_MAX;
        }

        /* B held: health bar shrinks by 1 px per frame */
        if ((joy & BUTTON_B) && health > 0)
        {
            health--;
        }

        /* Bar emptied: lose a life and refill (only if lives remain) */
        if (health == 0 && lives > 0)
        {
            lives--;
            health = BAR_MAX;
        }

        /* Countdown: one second per fpsTarget frames, stops at 00:00 */
        frameTick++;
        if (frameTick >= fpsTarget)
        {
            frameTick = 0;
            if (timeLeft > 0) timeLeft--;
        }

        /* Draw HUD */
        drawTimer(timeLeft);
        drawNumber(8, 4, score, 6);
        drawNumber(8, 6, lives, 2);
        drawHealth(health);

        SYS_doVBlankProcess();
    }

    return 0;
}
