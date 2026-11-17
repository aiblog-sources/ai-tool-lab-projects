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

#define BAND_LO     (UMB_Y - 7)          /* 193: first drop row touching band */
#define BAND_HI     (UMB_Y + UMB_H)      /* 216: first drop row past the band */
#define LINE_LAST   (UMB_Y + UMB_H - 1)  /* 215 */

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
/* per-line drop counters, indexed by absolute scanline (only 200..215 used) */
static unsigned char bl[UMB_Y + UMB_H];

static unsigned char ux;
static unsigned char rot;

#define DROP_Y(s)    ((unsigned char *)0x0220)[s]
#define DROP_T(s)    ((unsigned char *)0x0221)[s]
#define DROP_A(s)    ((unsigned char *)0x0222)[s]
#define DROP_X(s)    ((unsigned char *)0x0223)[s]

/* add (+1) or remove (-1) a whole drop at row y; y must be in band range */
static void band_drop(unsigned char y, signed char d)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > LINE_LAST) hi = LINE_LAST;
    for (; l <= hi; ++l) bl[l] += d;
}

/* init-time: can a drop at row y be added? */
static unsigned char band_fits(unsigned char y)
{
    unsigned char l, hi;
    if (y < BAND_LO || y >= BAND_HI) return 1;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > LINE_LAST) hi = LINE_LAST;
    for (; l <= hi; ++l) {
        if (bl[l] >= BAND_MAX) return 0;
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
    unsigned char i, s, pad, y, t, oy, lo, hi, l, x0;
    unsigned char hit;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    /* constant OAM fields, written once */
    for (i = 0; i < 8; ++i) {
        s = i << 2;
        OAM[s]     = UMB_Y + ((i >> 2) << 3);
        OAM[s + 1] = (i >> 2) ? 0x02 : 0x01;
        OAM[s + 2] = 1;
    }
    for (s = 0; s < NUM_DROPS * 4; s += 4) {
        DROP_T(s) = DROP_TILE;
        DROP_A(s) = 0;
    }

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (!band_fits(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (t >= BAND_LO && t < BAND_HI) band_drop(t, 1);
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
            oy = dy[i];
            y = oy + dsp[i];
            if (y >= DROP_BOTTOM) {
                if (oy >= BAND_LO && oy < BAND_HI) band_drop(oy, -1);
                respawn(i);
            } else if (y < BAND_LO) {
                dy[i] = y;
            } else if (y >= BAND_HI) {
                if (oy < BAND_HI) band_drop(oy, -1);
                dy[i] = y;
            } else {
                /* new row y is inside the band range */
                hit = ((unsigned int)dx[i] + 8 > ux &&
                       (unsigned int)dx[i] < (unsigned int)ux + UMB_W);
                if (hit) {
                    if (oy >= BAND_LO) band_drop(oy, -1);
                    respawn(i);          /* caught by the umbrella */
                } else {
                    /* only lines newly covered (oy+8 .. y+7) can overflow */
                    lo = oy + 8;
                    if (lo < UMB_Y) lo = UMB_Y;
                    hi = y + 7;
                    if (hi > LINE_LAST) hi = LINE_LAST;
                    for (l = lo; l <= hi; ++l) {
                        if (bl[l] >= BAND_MAX) goto wait;
                    }
                    for (l = lo; l <= hi; ++l) ++bl[l];
                    lo = (oy > UMB_Y) ? oy : UMB_Y;
                    hi = y - 1;
                    for (l = lo; l <= hi; ++l) --bl[l];
                    dy[i] = y;
                wait:
                    ;   /* else: wait a frame so no line gets a 5th drop */
                }
            }
        }

        /* --- OAM --- */
        x0 = ux;
        OAM_X(0) = x0;       OAM_X(1) = x0 + 8;
        OAM_X(2) = x0 + 16;  OAM_X(3) = x0 + 24;
        OAM_X(4) = x0;       OAM_X(5) = x0 + 8;
        OAM_X(6) = x0 + 16;  OAM_X(7) = x0 + 24;

        s = 0;
        for (i = rot; i < NUM_DROPS; ++i) {
            DROP_Y(s) = dy[i];
            DROP_X(s) = dx[i];
            s += 4;
        }
        for (i = 0; i < rot; ++i) {
            DROP_Y(s) = dy[i];
            DROP_X(s) = dx[i];
            s += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```

I haven't built or run this one, so the 60 fps result is unconfirmed.

- **Band updates:** a drop that falls a few rows only changes the lines at its top and bottom edges. The new code updates just those lines, plus the fit check on the newly covered lines, instead of removing and re-adding all 8 lines. The checks are equivalent to the original: the new lines are the only ones that can reach 5, and the drop's own old lines are already counted. Drops entirely outside the band take a short path. That path is the common case and covers most drops.
- **Counters:** `bl` is indexed by absolute scanline, so each access is a single indexed load or store instead of an offset calculation.
- **OAM:** the umbrella's Y, tile and attribute bytes, and every drop's tile and attribute bytes, never change, so they are written once at startup. Each frame writes only the umbrella X values and the Y and X of each drop. The rotation loop is split into two segments, so there is no wrap check.