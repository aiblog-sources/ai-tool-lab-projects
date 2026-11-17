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
#define FAST_MAX    (UMB_Y - 8)   /* y <= this: drop cannot touch the band */
#define BAND_LAST   (UMB_Y + UMB_H - 1)

#define DROP_OAM_Y ((unsigned char *)0x0220)
#define DROP_OAM_X ((unsigned char *)0x0223)

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
    unsigned char i, pad, y, t, x, o;
    unsigned char old, lo, hi, l, ok, elo, ehi;
    unsigned char *q;

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

    /* constant OAM bytes, written once */
    for (i = 0; i < 8; ++i) {
        OAM_Y(i)    = UMB_Y + ((i & 4) << 1);
        OAM_TILE(i) = (i & 4) ? 0x02 : 0x01;
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
            y = dy[i] + dsp[i];
            if (y <= FAST_MAX) {
                dy[i] = y;           /* nowhere near the band */
                continue;
            }
            old = dy[i];

            if (y >= DROP_BOTTOM ||
                (y < UMB_Y + UMB_H &&
                 (unsigned int)dx[i] + 8 > ux &&
                 (unsigned int)dx[i] < (unsigned int)ux + UMB_W)) {
                band_add(old, -1);
                respawn(i);          /* bottom reached or caught */
                continue;
            }

            /* lines newly covered by the drop: old+8 .. y+7, within band.
             * Lines still covered from before can never block it. */
            elo = old + 8;
            if (elo < UMB_Y) elo = UMB_Y;
            ehi = y + 7;
            if (ehi > BAND_LAST) ehi = BAND_LAST;

            ok = 1;
            if (elo <= ehi) {
                q = band_cnt + (elo - UMB_Y);
                for (l = elo; l <= ehi; ++l) {
                    if (*q >= BAND_MAX) { ok = 0; break; }
                    ++q;
                }
            }
            if (!ok) continue;       /* wait a frame */

            /* lines left behind: old .. y-1, within band */
            lo = (old < UMB_Y) ? UMB_Y : old;
            hi = y - 1;
            if (hi > BAND_LAST) hi = BAND_LAST;
            if (lo <= hi) {
                q = band_cnt + (lo - UMB_Y);
                for (l = lo; l <= hi; ++l) {
                    --*q;
                    ++q;
                }
            }
            if (elo <= ehi) {
                q = band_cnt + (elo - UMB_Y);
                for (l = elo; l <= ehi; ++l) {
                    ++*q;
                    ++q;
                }
            }
            dy[i] = y;
        }

        /* --- OAM --- */
        x = ux;
        OAM_X(0) = x;      OAM_X(4) = x;
        x += 8;
        OAM_X(1) = x;      OAM_X(5) = x;
        x += 8;
        OAM_X(2) = x;      OAM_X(6) = x;
        x += 8;
        OAM_X(3) = x;      OAM_X(7) = x;

        o = 0;
        for (i = rot; i < NUM_DROPS; ++i) {
            DROP_OAM_Y[o] = dy[i];
            DROP_OAM_X[o] = dx[i];
            o += 4;
        }
        for (i = 0; i < rot; ++i) {
            DROP_OAM_Y[o] = dy[i];
            DROP_OAM_X[o] = dx[i];
            o += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
