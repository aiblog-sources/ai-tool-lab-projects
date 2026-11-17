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

/* static scratch variables (faster than cc65 stack locals) */
static unsigned char s_l, s_hi, s_n;
static unsigned char *s_p;

/* add (+1) or remove (-1) a drop at row y from the band counters */
static void band_add(unsigned char y, signed char d)
{
    if (y <= UMB_Y - 8 || y >= UMB_Y + UMB_H) return;
    s_l = (y > UMB_Y) ? y : UMB_Y;
    s_hi = y + 7;
    if (s_hi > UMB_Y + UMB_H - 1) s_hi = UMB_Y + UMB_H - 1;
    s_p = band_cnt + (s_l - UMB_Y);
    s_n = s_hi - s_l + 1;
    do {
        *s_p += d;
        ++s_p;
    } while (--s_n);
}

/* can a drop at row y be added without exceeding BAND_MAX on any line? */
static unsigned char band_fits(unsigned char y)
{
    if (y <= UMB_Y - 8 || y >= UMB_Y + UMB_H) return 1;
    s_l = (y > UMB_Y) ? y : UMB_Y;
    s_hi = y + 7;
    if (s_hi > UMB_Y + UMB_H - 1) s_hi = UMB_Y + UMB_H - 1;
    s_p = band_cnt + (s_l - UMB_Y);
    s_n = s_hi - s_l + 1;
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

static void draw_umbrella(void)
{
    static unsigned char c, row, n;
    n = 0;
    for (row = 0; row < 2; ++row) {
        for (c = 0; c < 4; ++c) {
            OAM_Y(n)    = UMB_Y + (row << 3);
            OAM_TILE(n) = row ? 0x02 : 0x01;
            OAM_ATTR(n) = 1;
            OAM_X(n)    = ux + (c << 3);
            ++n;
        }
    }
}

void main(void)
{
    static unsigned char i, k, pad, y, t;
    static unsigned char *op;

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
            if (y <= UMB_Y - 8) {
                /* old and new rows are both above the band: no band work,
                   no collision, always fits */
                dy[i] = y;
                continue;
            }
            band_add(dy[i], -1);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y < UMB_Y + 16 &&
                       (unsigned int)dx[i] + 8 > ux &&
                       (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                respawn(i);          /* caught by the umbrella */
            } else if (band_fits(y)) {
                dy[i] = y;
            }
            /* else: wait a frame so no line gets a 5th drop in the band */
            band_add(dy[i], 1);
        }

        /* --- OAM --- */
        draw_umbrella();
        op = OAM + 32;
        for (i = rot; i < NUM_DROPS; ++i) {
            op[0] = dy[i];
            op[1] = DROP_TILE;
            op[2] = 0;
            op[3] = dx[i];
            op += 4;
        }
        for (i = 0; i < rot; ++i) {
            op[0] = dy[i];
            op[1] = DROP_TILE;
            op[2] = 0;
            op[3] = dx[i];
            op += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
