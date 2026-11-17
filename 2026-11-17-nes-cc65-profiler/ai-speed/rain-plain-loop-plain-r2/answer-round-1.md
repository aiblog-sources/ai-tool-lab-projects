Most of the cost was in the slow path for drops inside the umbrella band. Each one ran about 24 per-line counter updates and a full band check per frame, and a drop that's blocked keeps doing that every frame.

This version only touches the lines that actually change when a drop moves from row `o` to row `y`:

- **Counter updates:** a drop moves down at most 5 rows, so only the newly covered bottom lines need checking and incrementing, and the vacated top lines need decrementing. Lines covered both before and after keep the same count, and their post-removal count is always below 4, so they never need re-checking.
- **Equivalence:** this gives the same pass/wait decisions as remove, check, then re-add. Drops are still processed in the same order, so the `rnd8()` sequence and all positions are unchanged.
- **Respawn bound:** in the slow path `y` is at most 220, so it can never hit the bottom respawn there. That check now only runs on the fast paths.

I haven't compiled or run this one either, so the frame time is unmeasured.

```c
/* main.c - Rain: 40 falling drops (8x8) and a d-pad-controlled umbrella (32x16).
 *
 * Sprite budget: the umbrella takes OAM slots 0-7 (the highest priority) so
 * it never loses sprites to the 8-per-scanline limit. The 40 drops take
 * slots 8-47. Their priority order rotates every frame.
 *
 * The umbrella uses 4 sprites on each of lines UMB_Y..UMB_Y+15, so at most
 * 4 drops may overlap any one of those lines. A per-line counter (band_cnt)
 * tracks this; a drop whose next step would put a 5th drop on a line in the
 * band simply waits a frame, so the hardware limit is never exceeded.
 */
#include "nes.h"

#define NUM_DROPS   40
#define DROP_TILE   0x08   /* small dot */
#define UMB_Y       200    /* OAM Y of the canopy's top row */
#define UMB_H       16
#define UMB_W       32
#define UMB_SPEED   3
#define DROP_BOTTOM 232
#define BAND_MAX    4      /* drops allowed per line inside the umbrella band */

static const unsigned char palette[32] = {
    /* background */
    0x0F, 0x0F, 0x0F, 0x0F,  0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x0F, 0x0F,  0x0F, 0x0F, 0x0F, 0x0F,
    /* sprites: pal 0 = rain, pal 1 = umbrella */
    0x0F, 0x21, 0x11, 0x30,
    0x0F, 0x16, 0x06, 0x38,
    0x0F, 0x21, 0x11, 0x30,
    0x0F, 0x21, 0x11, 0x30
};

static unsigned char dx[NUM_DROPS];
static unsigned char dy[NUM_DROPS];
static unsigned char dsp[NUM_DROPS];
static unsigned char band_cnt[UMB_H];

static unsigned char ux;
static unsigned char rot;

/* init-time helpers (original logic) */
static void band_add(unsigned char y, signed char d)
{
    unsigned char l, hi;
    if (y + 8 <= UMB_Y || y >= UMB_Y + UMB_H) return;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;
}

static unsigned char band_fits(unsigned char y)
{
    unsigned char l, hi;
    if (y + 8 <= UMB_Y || y >= UMB_Y + UMB_H) return 1;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    for (; l <= hi; ++l) {
        if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;
    }
    return 1;
}

static void respawn(unsigned char i)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[i] = x;
    dy[i] = 0;
}

static void draw_umbrella_init(void)
{
    unsigned char c, row, n = 0;
    for (row = 0; row < 2; ++row) {
        for (c = 0; c < 4; ++c) {
            OAM_Y(n)    = UMB_Y + (row << 3);
            OAM_TILE(n) = row ? 0x02 : 0x01;
            OAM_ATTR(n) = 1;
            ++n;
        }
    }
}

/* Slow path: drop i moves from o to y, with o < 216 and y > 192 (so y <= 220). */
static void step_band(unsigned char i, unsigned char o, unsigned char y)
{
    unsigned char aLo = 0, aHi = 0, bLo = 0, bHi = 0, s, e, j;
    unsigned char hasA, inB;

    hasA = (o >= 193);
    if (hasA) {
        aLo = (o > UMB_Y) ? o : UMB_Y;
        aHi = o + 7;
        if (aHi > UMB_Y + UMB_H - 1) aHi = UMB_Y + UMB_H - 1;
    }
    inB = (y < UMB_Y + UMB_H);

    if (inB &&
        (unsigned int)dx[i] + 8 > ux &&
        (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
        /* caught by the umbrella */
        if (hasA) {
            for (j = aLo; j <= aHi; ++j) --band_cnt[j - UMB_Y];
        }
        respawn(i);
        return;
    }

    s = 0;
    if (inB) {
        bLo = (y > UMB_Y) ? y : UMB_Y;
        bHi = y + 7;
        if (bHi > UMB_Y + UMB_H - 1) bHi = UMB_Y + UMB_H - 1;
        s = hasA ? aHi + 1 : bLo;
        if (s < bLo) s = bLo;
        for (j = s; j <= bHi; ++j) {
            if (band_cnt[j - UMB_Y] >= BAND_MAX) return;   /* wait a frame */
        }
    }

    if (hasA) {
        e = aHi;
        if (inB && (unsigned char)(bLo - 1) < aHi) e = bLo - 1;
        for (j = aLo; j <= e; ++j) --band_cnt[j - UMB_Y];
    }
    if (inB) {
        for (j = s; j <= bHi; ++j) ++band_cnt[j - UMB_Y];
    }
    dy[i] = y;
}

void main(void)
{
    unsigned char i, pad, y, t, o, x;
    unsigned char *p;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (i = 0; i < UMB_H; ++i) band_cnt[i] = 0;

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (!band_fits(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        band_add(t, 1);
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM fields, written once */
    draw_umbrella_init();
    for (i = 0; i < NUM_DROPS; ++i) {
        OAM_TILE(8 + i) = DROP_TILE;
        OAM_ATTR(8 + i) = 0;
    }

    for (;;) {
        /* --- umbrella input --- */
        pad = pad_read();
        if (pad & PAD_LEFT) {
            ux = (ux > UMB_SPEED) ? ux - UMB_SPEED : 0;
        }
        if (pad & PAD_RIGHT) {
            ux += UMB_SPEED;
            if (ux > 256 - UMB_W) ux = 256 - UMB_W;
        }

        /* --- drops --- */
        for (i = 0; i < NUM_DROPS; ++i) {
            o = dy[i];
            y = o + dsp[i];
            if (y <= 192) {
                dy[i] = y;
            } else if (o >= 216) {
                if (y >= DROP_BOTTOM) respawn(i);
                else dy[i] = y;
            } else {
                step_band(i, o, y);
            }
        }

        /* --- OAM --- */
        x = ux;
        OAM_X(0) = x;       OAM_X(4) = x;
        x += 8;
        OAM_X(1) = x;       OAM_X(5) = x;
        x += 8;
        OAM_X(2) = x;       OAM_X(6) = x;
        x += 8;
        OAM_X(3) = x;       OAM_X(7) = x;

        p = OAM + 32;
        for (i = rot; i < NUM_DROPS; ++i) {
            p[0] = dy[i];
            p[3] = dx[i];
            p += 4;
        }
        for (i = 0; i < rot; ++i) {
            p[0] = dy[i];
            p[3] = dx[i];
            p += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```