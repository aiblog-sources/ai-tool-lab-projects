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
 * Speed: drops that stay above the band skip all band and collision work.
 * Band coverage ranges come from lookup tables, the umbrella overlap test is
 * a single 8-bit compare, constant OAM bytes are written once at startup, and
 * the per-frame OAM update uses byte-indexed stores.
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
#define BAND_TOP    (UMB_Y - 7)          /* 193: first y touching the band */
#define BAND_END    (UMB_Y + UMB_H)      /* 216: first y past the band */

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

/* band line range (index into band_cnt) covered by a drop at y = 193 + t */
static const unsigned char band_lo[23] = {
    0,0,0,0,0,0,0,0, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
};
static const unsigned char band_hi[23] = {
    0,1,2,3,4,5,6,7, 8,9,10,11,12,13,14,15,15,15,15,15,15,15,15
};
/* note: t = 0..7 -> lo 0; t = 8.. -> lo = t-7 */

static unsigned char dx[NUM_DROPS];
static unsigned char dy[NUM_DROPS];
static unsigned char dsp[NUM_DROPS];
static unsigned char band_cnt[UMB_H];

static unsigned char ux;
static unsigned char rot;

#define OAMYP ((unsigned char *)0x0220)
#define OAMXP ((unsigned char *)0x0223)

static void band_inc(unsigned char t)
{
    unsigned char l = band_lo[t];
    unsigned char h = band_hi[t];
    for (; l <= h; ++l) ++band_cnt[l];
}

static void band_dec(unsigned char t)
{
    unsigned char l = band_lo[t];
    unsigned char h = band_hi[t];
    for (; l <= h; ++l) --band_cnt[l];
}

static unsigned char band_ok(unsigned char t)
{
    unsigned char l = band_lo[t];
    unsigned char h = band_hi[t];
    for (; l <= h; ++l) {
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

static void init_oam_const(void)
{
    unsigned char c, row, n = 0, i;
    for (row = 0; row < 2; ++row) {
        for (c = 0; c < 4; ++c) {
            OAM_Y(n)    = UMB_Y + (row << 3);
            OAM_TILE(n) = row ? 0x02 : 0x01;
            OAM_ATTR(n) = 1;
            ++n;
        }
    }
    for (i = 0; i < NUM_DROPS; ++i) {
        OAM_TILE(8 + i) = DROP_TILE;
        OAM_ATTR(8 + i) = 0;
    }
}

void main(void)
{
    unsigned char i, pad, y, o, j, t;
    unsigned char uxl;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (i = 0; i < UMB_H; ++i) band_cnt[i] = 0;

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (t >= BAND_TOP && t < BAND_END && !band_ok(t - BAND_TOP))
            t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (t >= BAND_TOP && t < BAND_END) band_inc(t - BAND_TOP);
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    init_oam_const();

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
        uxl = ux;

        /* --- drops --- */
        for (i = 0; i < NUM_DROPS; ++i) {
            o = dy[i];
            y = o + dsp[i];
            if (y < BAND_TOP) {          /* nowhere near the band */
                dy[i] = y;
                continue;
            }
            if (o >= BAND_END) {         /* already below the band */
                if (y >= DROP_BOTTOM) respawn(i);
                else dy[i] = y;
                continue;
            }
            if (o >= BAND_TOP) band_dec(o - BAND_TOP);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y >= BAND_END) {
                dy[i] = y;
            } else if ((unsigned char)(dx[i] - uxl + 7) < 39) {
                respawn(i);              /* caught by the umbrella */
            } else {
                t = y - BAND_TOP;
                if (band_ok(t)) {
                    dy[i] = y;
                    band_inc(t);
                } else {
                    /* wait a frame so no line gets a 5th drop */
                    if (o >= BAND_TOP) band_inc(o - BAND_TOP);
                }
            }
        }

        /* --- OAM --- */
        OAM_X(0) = uxl;
        OAM_X(1) = uxl + 8;
        OAM_X(2) = uxl + 16;
        OAM_X(3) = uxl + 24;
        OAM_X(4) = uxl;
        OAM_X(5) = uxl + 8;
        OAM_X(6) = uxl + 16;
        OAM_X(7) = uxl + 24;

        j = 0;
        for (i = rot; i < NUM_DROPS; ++i) {
            OAMYP[j] = dy[i];
            OAMXP[j] = dx[i];
            j += 4;
        }
        for (i = 0; i < rot; ++i) {
            OAMYP[j] = dy[i];
            OAMXP[j] = dx[i];
            j += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
