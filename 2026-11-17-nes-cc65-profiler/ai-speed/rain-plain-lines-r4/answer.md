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
 * Speed notes: band bookkeeping is only done for drops that are actually in
 * the band (rows 193..215), and the OAM bytes that never change (umbrella
 * Y/tile/attr, drop tile/attr) are written once at startup.
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

/* a drop at row y touches the band iff 193 <= y <= 215 */
#define IN_BAND(y)  ((unsigned char)((y) - 193) < 23)

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

/* add (+1) or remove (-1) a drop at row y from the band counters
   (caller guarantees IN_BAND(y)) */
static void band_add(unsigned char y, signed char d)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    l -= UMB_Y;
    hi -= UMB_Y;
    for (; l <= hi; ++l) band_cnt[l] += d;
}

/* can a drop at row y be added without exceeding BAND_MAX on any line?
   (caller guarantees IN_BAND(y)) */
static unsigned char band_fits(unsigned char y)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    l -= UMB_Y;
    hi -= UMB_Y;
    for (; l <= hi; ++l) {
        if (band_cnt[l] >= BAND_MAX) return 0;
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

static void init_oam(void)
{
    unsigned char n, row, c;
    n = 0;
    for (row = 0; row < 2; ++row) {
        for (c = 0; c < 4; ++c) {
            OAM_Y(n)    = UMB_Y + (row << 3);
            OAM_TILE(n) = row ? 0x02 : 0x01;
            OAM_ATTR(n) = 1;
            ++n;
        }
    }
    for (n = 8; n < 8 + NUM_DROPS; ++n) {
        OAM_TILE(n) = DROP_TILE;
        OAM_ATTR(n) = 0;
    }
}

void main(void)
{
    unsigned char i, pad, y, t, old, nw, xx;
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
        while (IN_BAND(t) && !band_fits(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (IN_BAND(t)) band_add(t, 1);
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    init_oam();

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
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (IN_BAND(y)) {
                xx = dx[i];
                if ((unsigned int)xx + 8 > ux &&
                    (unsigned int)xx < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits(y)) {
                    dy[i] = y;
                }
                /* else: wait a frame */
            } else {
                dy[i] = y;
            }
            nw = dy[i];
            if (IN_BAND(nw)) band_add(nw, 1);
        }

        /* --- OAM --- */
        xx = ux;
        OAM_X(0) = xx;       OAM_X(4) = xx;
        xx += 8;
        OAM_X(1) = xx;       OAM_X(5) = xx;
        xx += 8;
        OAM_X(2) = xx;       OAM_X(6) = xx;
        xx += 8;
        OAM_X(3) = xx;       OAM_X(7) = xx;

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

I haven't compiled or profiled this, so the 60 fps target is untested.

The speedup comes from three changes:

- **Band bookkeeping:** `band_add` and `band_fits` now run only for drops in rows 193–215, which is a small fraction of the 40 drops. The loop used to call them for every drop, twice each. This was the biggest cost.
- **Constant OAM bytes:** The umbrella's Y, tile and attribute bytes and the drops' tile and attribute bytes never change. They are written once at startup, so each frame only stores the umbrella X values and the drops' Y and X.
- **Rotating copy:** The drop copy runs as two plain loops over a pointer, from `rot` to 39 and then from 0 to `rot - 1`. This replaces the per-drop wrap-around and slot arithmetic.

Drop movement, respawn order (so `rnd8` is called in the same sequence), the catch test, the band-limit waiting and the priority rotation are all unchanged.