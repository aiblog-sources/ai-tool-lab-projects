```c
/* main.c - Rain: 40 falling drops (8x8) and a d-pad-controlled umbrella (32x16).
 *
 * Same scene as before, restructured to fit in one frame:
 *  - drops that stay above the umbrella band (the vast majority of updates)
 *    skip all band bookkeeping and the collision test
 *  - collision test is 8-bit, with per-frame precomputed umbrella bounds
 *  - tile/attribute/Y bytes that never change are written to OAM once
 *  - the OAM copy loop has no modulo and uses a running byte offset
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

#define DROP_OAM ((unsigned char *)0x0220)   /* slot 8 */

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

/* add (+1) or remove (-1) a drop at row y from the band counters */
static void band_add(unsigned char y, signed char d)
{
    unsigned char l, hi;
    if (y + 8 <= UMB_Y || y >= UMB_Y + UMB_H) return;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;
}

/* can a drop at row y be added without exceeding BAND_MAX on any line? */
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

void main(void)
{
    unsigned char i, pad, y, t, o, n;
    unsigned char xl, xh;

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

    /* constant OAM bytes: umbrella (slots 0-7) */
    for (n = 0; n < 8; ++n) {
        OAM_Y(n)    = UMB_Y + ((n >> 2) << 3);
        OAM_TILE(n) = (n >> 2) ? 0x02 : 0x01;
        OAM_ATTR(n) = 1;
    }
    /* constant OAM bytes: drops (slots 8-47) */
    for (n = 0; n < NUM_DROPS; ++n) {
        o = n << 2;
        DROP_OAM[o + 1] = DROP_TILE;
        DROP_OAM[o + 2] = 0;
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

        /* umbrella overlap bounds on drop x: xl <= dx <= xh */
        xl = (ux > 7) ? ux - 7 : 0;
        xh = ux + (UMB_W - 1);

        /* --- drops --- */
        for (i = 0; i < NUM_DROPS; ++i) {
            y = dy[i] + dsp[i];
            if (y < UMB_Y - 7) {
                /* above the band both before and after: no bookkeeping */
                dy[i] = y;
                continue;
            }
            band_add(dy[i], -1);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y < UMB_Y + UMB_H && dx[i] >= xl && dx[i] <= xh) {
                respawn(i);          /* caught by the umbrella */
            } else if (band_fits(y)) {
                dy[i] = y;
            }
            /* else: wait a frame so no line gets a 5th drop in the band */
            band_add(dy[i], 1);
        }

        /* --- OAM --- */
        OAM_X(0) = ux;       OAM_X(1) = ux + 8;
        OAM_X(2) = ux + 16;  OAM_X(3) = ux + 24;
        OAM_X(4) = ux;       OAM_X(5) = ux + 8;
        OAM_X(6) = ux + 16;  OAM_X(7) = ux + 24;

        o = 0;
        for (i = rot; i < NUM_DROPS; ++i) {
            DROP_OAM[o]     = dy[i];
            DROP_OAM[o + 3] = dx[i];
            o += 4;
        }
        for (i = 0; i < rot; ++i) {
            DROP_OAM[o]     = dy[i];
            DROP_OAM[o + 3] = dx[i];
            o += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```