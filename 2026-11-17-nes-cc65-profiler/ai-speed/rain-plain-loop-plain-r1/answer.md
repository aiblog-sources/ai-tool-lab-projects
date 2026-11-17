```c
/* main.c - Rain: 40 falling drops (8x8) and a d-pad-controlled umbrella (32x16).
 *
 * Same behaviour as before, but the per-frame work is much cheaper:
 *  - band counters are only touched for drops that are actually in the band
 *  - the umbrella collision test only runs for drops in the band rows
 *  - constant OAM fields (tile, attr, umbrella Y) are written once
 *  - OAM drop slots are filled with a pointer in two linear runs (no modulo wrap)
 *  - loop variables are static, so cc65 avoids the software stack
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

/* a drop at row y overlaps the band when y in [193, 215] */
#define IN_BAND(y)  ((y) >= UMB_Y - 7 && (y) < UMB_Y + UMB_H)

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

/* loop state (static = fast absolute addressing) */
static unsigned char i, k, pad, y, t, old, inb;
static unsigned char *p;

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

static void respawn(unsigned char n)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[n] = x;
    dy[n] = 0;
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    /* constant OAM fields */
    for (i = 0; i < 8; ++i) {
        OAM_Y(i)    = (i < 4) ? UMB_Y : UMB_Y + 8;
        OAM_TILE(i) = (i < 4) ? 0x01 : 0x02;
        OAM_ATTR(i) = 1;
    }
    for (i = 8; i < 8 + NUM_DROPS; ++i) {
        OAM_TILE(i) = DROP_TILE;
        OAM_ATTR(i) = 0;
    }

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
            old = dy[i];
            y = old + dsp[i];
            if (IN_BAND(old)) band_add(old, -1);

            inb = 0;
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (IN_BAND(y)) {
                if ((unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits(y)) {
                    dy[i] = y;
                    inb = 1;
                } else {
                    inb = IN_BAND(old);  /* waits: stays at old row */
                }
            } else {
                dy[i] = y;
            }
            if (inb) band_add(dy[i], 1);
        }

        /* --- OAM --- */
        OAM_X(0) = OAM_X(4) = ux;
        OAM_X(1) = OAM_X(5) = ux + 8;
        OAM_X(2) = OAM_X(6) = ux + 16;
        OAM_X(3) = OAM_X(7) = ux + 24;

        p = &OAM_Y(8);
        for (k = rot; k < NUM_DROPS; ++k) {
            p[0] = dy[k];
            p[3] = dx[k];
            p += 4;
        }
        for (k = 0; k < rot; ++k) {
            p[0] = dy[k];
            p[3] = dx[k];
            p += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```

I haven't compiled or profiled this, so I can't confirm it now fits in one frame.

One behaviour to check: when a drop is held back by `band_fits`, it keeps its old row. If that old row was in the band, its counter is re-added, as in the original.