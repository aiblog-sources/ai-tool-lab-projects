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
 *
 * Speed: drops away from the band take a very short path. For drops that
 * touch the band, the counters are updated incrementally (only the lines a
 * drop leaves and enters) instead of removing and re-adding all 8 lines.
 * A drop moves down by at most 5 lines, so the lines it keeps already hold
 * a count of at most BAND_MAX including itself, and only newly entered
 * lines need to be checked. Behaviour is identical to the original.
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

/* slow path: drop i at row d (d < 216) wants to move to row y (y > 192, y < 232) */
static void drop_band(unsigned char i, unsigned char d, unsigned char y)
{
    int l, lo, hi;
    unsigned char u = ux;

    if (y < UMB_Y + UMB_H &&
        (unsigned int)dx[i] + 8 > u &&
        (unsigned int)dx[i] < (unsigned int)u + UMB_W) {
        /* caught: remove the old drop completely, then respawn */
        band_add(d, -1);
        respawn(i);
        return;
    }

    /* lines newly entered: d+8 .. y+7, clipped to the band */
    lo = d + 8;
    if (lo < UMB_Y) lo = UMB_Y;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    for (l = lo; l <= hi; ++l) {
        if (band_cnt[l - UMB_Y] >= BAND_MAX) return;   /* wait a frame */
    }
    for (l = lo; l <= hi; ++l) ++band_cnt[l - UMB_Y];

    /* lines left: d .. y-1, clipped to the band */
    lo = d;
    if (lo < UMB_Y) lo = UMB_Y;
    hi = y - 1;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    for (l = lo; l <= hi; ++l) --band_cnt[l - UMB_Y];

    dy[i] = y;
}

void main(void)
{
    unsigned char i, pad, y, d, t;
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

    /* constant OAM bytes: umbrella Y/tile/attr, drop tile/attr */
    for (i = 0; i < 8; ++i) {
        OAM_Y(i)    = UMB_Y + ((i >> 2) << 3);
        OAM_TILE(i) = (i >> 2) ? 0x02 : 0x01;
        OAM_ATTR(i) = 1;
    }
    for (i = 8; i < 8 + NUM_DROPS; ++i) {
        OAM_TILE(i) = DROP_TILE;
        OAM_ATTR(i) = 0;
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
            d = dy[i];
            y = d + dsp[i];
            if (y >= DROP_BOTTOM) {
                respawn(i);                 /* d >= 227: never in the band */
            } else if (y <= UMB_Y - 8 || d >= UMB_Y + UMB_H) {
                dy[i] = y;                  /* not touching the band */
            } else {
                drop_band(i, d, y);
            }
        }

        /* --- OAM --- */
        OAM_X(0) = ux;      OAM_X(4) = ux;
        OAM_X(1) = ux + 8;  OAM_X(5) = ux + 8;
        OAM_X(2) = ux + 16; OAM_X(6) = ux + 16;
        OAM_X(3) = ux + 24; OAM_X(7) = ux + 24;

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

I haven't built or run this, so I don't know yet whether it holds 60 fps in your scripted run.

The remaining cost was the band bookkeeping, which removed and re-added all 8 lines of every drop in the band each loop. Now only the lines a drop leaves and enters are updated, and only the entered lines are checked against `BAND_MAX`. That is equivalent to the old test because a drop moves at most 5 lines per step. The lines it keeps already count at most 4 including itself, so they can never be the reason it waits.

Drops outside the band skip all of that and just add their speed to `dy`. The drops that do touch the band go through a separate slow-path function that handles the umbrella-catch test and the counter updates.