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
 * Speed: the band bookkeeping is skipped for drops outside the band rows
 * (most of them), OAM tiles/attributes/umbrella Y are written once, and the
 * per-frame OAM fill uses a running pointer.
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

/* a drop at row y touches the band iff y > BAND_LO && y < BAND_HI */
#define BAND_LO (UMB_Y - 8)          /* 192 */
#define BAND_HI (UMB_Y + UMB_H)      /* 216 */

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

/* add (+1) or remove (-1) a drop at row y; caller guarantees y in band range */
static void band_add(unsigned char y, signed char d)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    hi -= UMB_Y;
    for (; l <= hi; ++l) band_cnt[l] += d;
}

/* can a drop at row y (in band range) be added without exceeding BAND_MAX? */
static unsigned char band_fits(unsigned char y)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    hi -= UMB_Y;
    for (; l <= hi; ++l) {
        if (band_cnt[l] >= BAND_MAX) return 0;
    }
    return 1;
}

void main(void)
{
    unsigned char i, k, pad, y, t, x, o;
    unsigned char *p;
    unsigned int  top;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (i = 0; i < UMB_H; ++i) band_cnt[i] = 0;

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (t > BAND_LO && t < BAND_HI && !band_fits(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (t > BAND_LO && t < BAND_HI) band_add(t, 1);
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM fields, written once */
    for (i = 0; i < 4; ++i) {
        OAM_Y(i)     = UMB_Y;
        OAM_TILE(i)  = 0x01;
        OAM_ATTR(i)  = 1;
        OAM_Y(i + 4)    = UMB_Y + 8;
        OAM_TILE(i + 4) = 0x02;
        OAM_ATTR(i + 4) = 1;
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
            o = dy[i];
            y = o + dsp[i];
            if (o > BAND_LO && o < BAND_HI) band_add(o, -1);
            if (y >= DROP_BOTTOM) {
                x = rnd8();
                if (x > 247) x -= 8;
                dx[i] = x;
                dy[i] = 0;
            } else if (y > BAND_LO && y < BAND_HI) {
                top = y;
                if ((unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    x = rnd8();          /* caught by the umbrella */
                    if (x > 247) x -= 8;
                    dx[i] = x;
                    dy[i] = 0;
                } else if (band_fits(y)) {
                    dy[i] = y;
                }
            } else {
                dy[i] = y;
            }
            o = dy[i];
            if (o > BAND_LO && o < BAND_HI) band_add(o, 1);
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

        p = OAM + 32;
        i = rot;
        for (k = 0; k < NUM_DROPS; ++k) {
            p[0] = dy[i];
            p[3] = dx[i];
            p += 4;
            ++i;
            if (i >= NUM_DROPS) i = 0;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
