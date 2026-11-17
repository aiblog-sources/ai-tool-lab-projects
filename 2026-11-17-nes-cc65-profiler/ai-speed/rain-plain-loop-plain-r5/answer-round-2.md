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
 * Speed notes (cc65): all working variables are file-level statics (absolute
 * addressing) instead of stack locals, everything in the per-frame loop is
 * 8-bit, and the band counters are updated incrementally: only the lines a
 * drop enters are tested, and only the lines it leaves / enters are changed.
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

/* working variables (statics are much cheaper than cc65 stack locals) */
static unsigned char i, j, pad, y, d, t, lo, hi, uxr;
static unsigned char *p;

/* add (+1) or remove (-1) a drop at row y from the band counters (init only) */
static void band_add(unsigned char yy, signed char dd)
{
    unsigned char l, h;
    if (yy + 8 <= UMB_Y || yy >= UMB_Y + UMB_H) return;
    l = (yy > UMB_Y) ? yy : UMB_Y;
    h = yy + 7;
    if (h > UMB_Y + UMB_H - 1) h = UMB_Y + UMB_H - 1;
    for (; l <= h; ++l) band_cnt[l - UMB_Y] += dd;
}

/* can a drop at row y be added without exceeding BAND_MAX on any line? (init only) */
static unsigned char band_fits(unsigned char yy)
{
    unsigned char l, h;
    if (yy + 8 <= UMB_Y || yy >= UMB_Y + UMB_H) return 1;
    l = (yy > UMB_Y) ? yy : UMB_Y;
    h = yy + 7;
    if (h > UMB_Y + UMB_H - 1) h = UMB_Y + UMB_H - 1;
    for (; l <= h; ++l) {
        if (band_cnt[l - UMB_Y] >= BAND_MAX) return 0;
    }
    return 1;
}

static void respawn(void)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[i] = x;
    dy[i] = 0;
}

void main(void)
{
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
        uxr = ux + (UMB_W - 1);          /* last column covered (<= 255) */

        /* --- drops --- */
        for (i = 0; i < NUM_DROPS; ++i) {
            d = dy[i];
            y = d + dsp[i];
            if (y >= DROP_BOTTOM) {
                respawn();                  /* d >= 227: never in the band */
            } else if (y <= UMB_Y - 8 || d >= UMB_Y + UMB_H) {
                dy[i] = y;                  /* not touching the band */
            } else {
                t = dx[i];
                if (y < UMB_Y + UMB_H && (unsigned char)(t + 8) > ux && t <= uxr) {
                    /* caught: remove old drop (lines d..d+7 in band), respawn */
                    if (d > UMB_Y - 8) {
                        lo = (d > UMB_Y) ? d - UMB_Y : 0;
                        hi = d - (UMB_Y - 7);
                        if (hi > UMB_H - 1) hi = UMB_H - 1;
                        for (j = lo; j <= hi; ++j) --band_cnt[j];
                    }
                    respawn();
                } else {
                    /* lines newly entered: d+8 .. y+7 (band-relative) */
                    lo = (d > UMB_Y - 8) ? d - (UMB_Y - 8) : 0;
                    hi = y - (UMB_Y - 7);
                    if (hi > UMB_H - 1) hi = UMB_H - 1;
                    for (j = lo; j <= hi; ++j) {
                        if (band_cnt[j] >= BAND_MAX) goto wait;
                    }
                    for (j = lo; j <= hi; ++j) ++band_cnt[j];
                    /* lines left: d .. y-1 (band-relative) */
                    if (y > UMB_Y) {
                        lo = (d > UMB_Y) ? d - UMB_Y : 0;
                        hi = y - (UMB_Y + 1);
                        if (hi > UMB_H - 1) hi = UMB_H - 1;
                        for (j = lo; j <= hi; ++j) --band_cnt[j];
                    }
                    dy[i] = y;
                wait: ;
                }
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

I haven't built or run this, so I don't know whether it holds 60 fps in your scripted run.

My best guess at the remaining cost is two things I hadn't addressed before:

- **Stack locals:** cc65 puts function locals on its software stack, and every access to them is slow. All the hot variables in `main` are now file-level statics with absolute addressing.
- **Int arithmetic:** the hot paths used `int` math, which cc65 handles with slow helper calls. The drop loop is now 8-bit throughout. `dx` is at most 247 and `ux+31` is at most 255, so the collision test fits in a byte.

The slow path for drops in the band is inlined with the same incremental counter updates as before. Behaviour should be identical to the original: same drops, same `rnd8` call order, same wait and catch rules, and the same OAM slot rotation.