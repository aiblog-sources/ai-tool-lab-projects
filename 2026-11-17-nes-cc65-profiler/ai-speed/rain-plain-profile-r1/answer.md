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
 * Speed: band work is skipped for drops outside the band, constant OAM bytes
 * (tiles, attributes, umbrella Y) are written once, and hot-path variables
 * are static so cc65 avoids software-stack traffic.
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

/* a drop at row y touches the band when y in [193, 215], i.e. y-193 < 23 */
#define BAND_LO     (UMB_Y - 7)
#define BAND_SPAN   (UMB_H + 7)
#define IN_BAND(y)  ((unsigned char)((y) - BAND_LO) < BAND_SPAN)

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

static unsigned char s_l, s_n;
static unsigned char *s_p;

/* compute first band line index pointer and line count for a drop at y */
static void band_range(unsigned char y)
{
    unsigned char l = (y > UMB_Y) ? y : UMB_Y;
    unsigned char hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    s_p = band_cnt + (l - UMB_Y);
    s_n = hi - l + 1;
}

/* add a drop at row y (must be in band) */
static void __fastcall__ band_inc(unsigned char y)
{
    band_range(y);
    do { ++*s_p; ++s_p; } while (--s_n);
}

/* remove a drop at row y (must be in band) */
static void __fastcall__ band_dec(unsigned char y)
{
    band_range(y);
    do { --*s_p; ++s_p; } while (--s_n);
}

/* can a drop at row y (in band) be added without exceeding BAND_MAX? */
static unsigned char __fastcall__ band_fits(unsigned char y)
{
    if (!IN_BAND(y)) return 1;
    band_range(y);
    do {
        if (*s_p >= BAND_MAX) return 0;
        ++s_p;
    } while (--s_n);
    return 1;
}

static void respawn(unsigned char i)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[i] = x;
    dy[i] = 0;
}

static unsigned char i, k, pad, slot, y, oy;
static unsigned int  top;
static unsigned char *op;

void main(void)
{
    unsigned char t;

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
        if (IN_BAND(t)) band_inc(t);
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM bytes, written once */
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
            oy = dy[i];
            y = oy + dsp[i];
            if (IN_BAND(oy)) band_dec(oy);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (IN_BAND(y)) {
                top = y;
                if (top + 8 > UMB_Y && top < UMB_Y + 16 &&
                    (unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits(y)) {
                    dy[i] = y;
                }
            } else {
                dy[i] = y;
            }
            oy = dy[i];
            if (IN_BAND(oy)) band_inc(oy);
        }

        /* --- OAM --- */
        op = OAM;
        op[3]  = ux;
        op[7]  = ux + 8;
        op[11] = ux + 16;
        op[15] = ux + 24;
        op[19] = ux;
        op[23] = ux + 8;
        op[27] = ux + 16;
        op[31] = ux + 24;

        op = OAM + 32;
        i = rot;
        for (k = 0; k < NUM_DROPS; ++k) {
            op[0] = dy[i];
            op[3] = dx[i];
            op += 4;
            ++i;
            if (i >= NUM_DROPS) i = 0;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```