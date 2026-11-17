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
 * Speed notes: static locals instead of stack locals, a fast path for drops
 * that are (and stay) above the band, and constant OAM bytes (tile, attr,
 * umbrella Y) written once at startup.
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

/* static scratch variables (much cheaper than cc65 stack locals) */
static unsigned char s_l, s_hi;
static unsigned char s_i, s_pad, s_y, s_old, s_j, s_t, s_u;

/* add (+1) or remove (-1) a drop at row y from the band counters */
static void band_add(unsigned char y, signed char d)
{
    if (y + 8 <= UMB_Y || y >= UMB_Y + UMB_H) return;
    s_l = (y > UMB_Y) ? y : UMB_Y;
    s_hi = y + 7;
    if (s_hi > UMB_Y + UMB_H - 1) s_hi = UMB_Y + UMB_H - 1;
    for (; s_l <= s_hi; ++s_l) band_cnt[s_l - UMB_Y] += d;
}

/* can a drop at row y be added without exceeding BAND_MAX on any line? */
static unsigned char band_fits(unsigned char y)
{
    if (y + 8 <= UMB_Y || y >= UMB_Y + UMB_H) return 1;
    s_l = (y > UMB_Y) ? y : UMB_Y;
    s_hi = y + 7;
    if (s_hi > UMB_Y + UMB_H - 1) s_hi = UMB_Y + UMB_H - 1;
    for (; s_l <= s_hi; ++s_l) {
        if (band_cnt[s_l - UMB_Y] >= BAND_MAX) return 0;
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
    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (s_i = 0; s_i < UMB_H; ++s_i) band_cnt[s_i] = 0;

    for (s_i = 0; s_i < NUM_DROPS; ++s_i) {
        dx[s_i] = rnd8();
        if (dx[s_i] > 247) dx[s_i] -= 8;
        s_t = rnd8() % DROP_BOTTOM;
        while (!band_fits(s_t)) s_t = rnd8() % DROP_BOTTOM;
        dy[s_i] = s_t;
        band_add(s_t, 1);
        dsp[s_i] = 1 + (s_i & 3) + ((s_i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM fields: umbrella Y/tile/attr */
    for (s_i = 0; s_i < 8; ++s_i) {
        s_u = (s_i < 4) ? 0 : 1;
        OAM_Y(s_i)    = UMB_Y + (s_u << 3);
        OAM_TILE(s_i) = s_u ? 0x02 : 0x01;
        OAM_ATTR(s_i) = 1;
    }
    /* constant OAM fields: drop tile/attr */
    for (s_i = 8; s_i < 8 + NUM_DROPS; ++s_i) {
        OAM_TILE(s_i) = DROP_TILE;
        OAM_ATTR(s_i) = 0;
    }

    for (;;) {
        /* --- umbrella input --- */
        s_pad = pad_read();
        if (s_pad & PAD_LEFT) {
            ux = (ux > UMB_SPEED) ? ux - UMB_SPEED : 0;
        }
        if (s_pad & PAD_RIGHT) {
            ux += UMB_SPEED;
            if (ux > 256 - UMB_W) ux = 256 - UMB_W;
        }

        /* --- drops --- */
        for (s_i = 0; s_i < NUM_DROPS; ++s_i) {
            s_old = dy[s_i];
            s_y = s_old + dsp[s_i];
            if (s_old < 193 && s_y < 193) {
                /* far above the band: no band bookkeeping, no collision */
                dy[s_i] = s_y;
                continue;
            }
            band_add(s_old, -1);
            if (s_y >= DROP_BOTTOM) {
                respawn(s_i);
            } else if (s_y >= 193 && s_y < UMB_Y + 16 &&
                       (unsigned int)dx[s_i] + 8 > ux &&
                       (unsigned int)dx[s_i] < (unsigned int)ux + UMB_W) {
                respawn(s_i);          /* caught by the umbrella */
            } else if (band_fits(s_y)) {
                dy[s_i] = s_y;
            }
            /* else: wait a frame so no line gets a 5th drop in the band */
            band_add(dy[s_i], 1);
        }

        /* --- OAM --- */
        s_u = ux;
        OAM_X(0) = s_u;
        OAM_X(4) = s_u;
        s_u += 8;
        OAM_X(1) = s_u;
        OAM_X(5) = s_u;
        s_u += 8;
        OAM_X(2) = s_u;
        OAM_X(6) = s_u;
        s_u += 8;
        OAM_X(3) = s_u;
        OAM_X(7) = s_u;

        s_j = 0;
        for (s_i = rot; s_i < NUM_DROPS; ++s_i) {
            ((unsigned char *)0x0220)[s_j]     = dy[s_i];
            ((unsigned char *)0x0220)[s_j + 3] = dx[s_i];
            s_j += 4;
        }
        for (s_i = 0; s_i < rot; ++s_i) {
            ((unsigned char *)0x0220)[s_j]     = dy[s_i];
            ((unsigned char *)0x0220)[s_j + 3] = dx[s_i];
            s_j += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```