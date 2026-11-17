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
 * Speed: band helpers run only for drops inside the band, the constant OAM
 * bytes (umbrella Y/tile/attr, drop tile/attr) are written once, and the
 * per-frame OAM copy uses plain absolute-indexed stores.
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

/* a drop at y touches the band iff 193 <= y <= 215 */
#define IN_BAND(y)  ((y) > 192 && (y) < 216)

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

static unsigned char bl, bh;   /* scratch for the band helpers */

/* y must be inside the band */
static void __fastcall__ band_inc(unsigned char y)
{
    bl = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    bh = y - (UMB_Y - 7);
    if (bh > UMB_H - 1) bh = UMB_H - 1;
    do {
        ++band_cnt[bl];
    } while (++bl <= bh);
}

static void __fastcall__ band_dec(unsigned char y)
{
    bl = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    bh = y - (UMB_Y - 7);
    if (bh > UMB_H - 1) bh = UMB_H - 1;
    do {
        --band_cnt[bl];
    } while (++bl <= bh);
}

/* can a drop at row y (inside the band) be added without exceeding
   BAND_MAX on any line? */
static unsigned char __fastcall__ band_fits_in(unsigned char y)
{
    bl = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    bh = y - (UMB_Y - 7);
    if (bh > UMB_H - 1) bh = UMB_H - 1;
    do {
        if (band_cnt[bl] >= BAND_MAX) return 0;
    } while (++bl <= bh);
    return 1;
}

static void __fastcall__ respawn(unsigned char i)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[i] = x;
    dy[i] = 0;
}

static unsigned char i, k, pad, y, t, oy, x, u;

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
        while (IN_BAND(t) && !band_fits_in(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (IN_BAND(t)) band_inc(t);
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM bytes, written once */
    for (i = 0; i < 16; i += 4) {
        OAM[i]      = UMB_Y;
        OAM[i + 1]  = 0x01;
        OAM[i + 2]  = 1;
        OAM[i + 16] = UMB_Y + 8;
        OAM[i + 17] = 0x02;
        OAM[i + 18] = 1;
    }
    for (x = 0; x < NUM_DROPS * 4; x += 4) {
        ((unsigned char *)0x0221)[x] = DROP_TILE;
        ((unsigned char *)0x0222)[x] = 0;
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
                if ((unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits_in(y)) {
                    dy[i] = y;
                }
                /* else: wait a frame */
            } else {
                dy[i] = y;
            }
            t = dy[i];
            if (IN_BAND(t)) band_inc(t);
        }

        /* --- OAM --- */
        u = ux;
        OAM[3]  = u;
        OAM[19] = u;
        u += 8;
        OAM[7]  = u;
        OAM[23] = u;
        u += 8;
        OAM[11] = u;
        OAM[27] = u;
        u += 8;
        OAM[15] = u;
        OAM[31] = u;

        x = 0;
        for (k = rot; k < NUM_DROPS; ++k) {
            ((unsigned char *)0x0220)[x] = dy[k];
            ((unsigned char *)0x0223)[x] = dx[k];
            x += 4;
        }
        for (k = 0; k < rot; ++k) {
            ((unsigned char *)0x0220)[x] = dy[k];
            ((unsigned char *)0x0223)[x] = dx[k];
            x += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
