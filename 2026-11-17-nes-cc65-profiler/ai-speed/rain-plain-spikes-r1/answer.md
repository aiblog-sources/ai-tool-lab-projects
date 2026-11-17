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

/* a drop at row y touches the band iff y in [BAND_LO, BAND_LO + BAND_N) */
#define BAND_LO     (UMB_Y - 7)
#define BAND_N      (UMB_H + 7)
#define IN_BAND(y)  ((unsigned char)((y) - BAND_LO) < BAND_N)

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
    l -= UMB_Y;
    hi -= UMB_Y;
    for (; l <= hi; ++l) band_cnt[l] += d;
}

/* can a drop at row y be added without exceeding BAND_MAX on any line? */
static unsigned char band_fits(unsigned char y)
{
    unsigned char l, hi;
    if (y + 8 <= UMB_Y || y >= UMB_Y + UMB_H) return 1;
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

void main(void)
{
    unsigned char i, k, pad, y, t, old, n;
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
    p = OAM;
    for (n = 0; n < 8; ++n) {
        p[0] = UMB_Y + ((n >> 2) << 3);
        p[1] = (n >> 2) ? 0x02 : 0x01;
        p[2] = 1;
        p += 4;
    }
    for (n = 0; n < NUM_DROPS; ++n) {
        p[1] = DROP_TILE;
        p[2] = 0;
        p += 4;
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
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (IN_BAND(y) &&
                       (unsigned int)dx[i] + 8 > ux &&
                       (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                respawn(i);          /* caught by the umbrella */
            } else if (!IN_BAND(y) || band_fits(y)) {
                dy[i] = y;
            }
            /* else: wait a frame so no line gets a 5th drop in the band */
            t = dy[i];
            if (IN_BAND(t)) band_add(t, 1);
        }

        /* --- OAM --- */
        p = OAM;
        t = ux;
        p[3]  = t;      p[7]  = t + 8;  p[11] = t + 16; p[15] = t + 24;
        p[19] = t;      p[23] = t + 8;  p[27] = t + 16; p[31] = t + 24;

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

I haven't compiled or profiled this. The changes:

- **Band work skipped outside the band:** a drop only touches the band when its Y is 193–215, so `band_add` and `band_fits` are now called only then. Most of the 40 drops are outside that range on any frame.
- **Umbrella test reordered:** the cheap band check now runs before the horizontal overlap test. The overlap test has no side effects, so the result is the same.
- **Band loops simplified:** the counter index is computed once, so the loop no longer subtracts `UMB_Y` on every iteration.
- **OAM writes cut down:** the tile, attribute and umbrella Y bytes never change, so they are written once before the loop. Each frame now writes only the drop Y and X bytes and the umbrella X bytes.
- **Rotation wrap removed:** the rotating drop order is now two plain loops, with no per-iteration wrap test. Slot order is the same as before.

The order of `rnd8` calls is unchanged, so spawn positions stay identical.