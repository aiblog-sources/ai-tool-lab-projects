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

/* a drop at row y touches the band iff BAND_LO <= y < BAND_HI */
#define BAND_LO     (UMB_Y - 7)
#define BAND_HI     (UMB_Y + UMB_H)

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

/* scratch statics (faster than cc65 stack locals) */
static unsigned char s_l, s_hi;

/* compute band line range for a drop at y (must be inside the band) */
#define RANGE(y) \
    s_l = ((y) > UMB_Y) ? (unsigned char)((y) - UMB_Y) : 0; \
    s_hi = (unsigned char)((y) - BAND_LO); \
    if (s_hi > UMB_H - 1) s_hi = UMB_H - 1;

/* remove a drop at row y (must be in band) */
static void __fastcall__ band_dec(unsigned char y)
{
    RANGE(y);
    do {
        --band_cnt[s_l];
    } while (++s_l <= s_hi);
}

/* add a drop at row y (must be in band) */
static void __fastcall__ band_inc(unsigned char y)
{
    RANGE(y);
    do {
        ++band_cnt[s_l];
    } while (++s_l <= s_hi);
}

/* can a drop at row y (in band) be added without exceeding BAND_MAX? */
static unsigned char __fastcall__ band_fits(unsigned char y)
{
    RANGE(y);
    do {
        if (band_cnt[s_l] >= BAND_MAX) return 0;
    } while (++s_l <= s_hi);
    return 1;
}

/* general versions for init */
static unsigned char fits_any(unsigned char y)
{
    if (y < BAND_LO || y >= BAND_HI) return 1;
    return band_fits(y);
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
    unsigned char c, x;
    unsigned char *p = OAM;
    for (c = 0; c < 4; ++c) {
        x = ux + (c << 3);
        p[0] = UMB_Y;
        p[1] = 0x01;
        p[2] = 1;
        p[3] = x;
        p[16] = UMB_Y + 8;
        p[17] = 0x02;
        p[18] = 1;
        p[19] = x;
        p += 4;
    }
}

void main(void)
{
    static unsigned char i, k, pad, y, oy, t;
    static unsigned char *p;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (i = 0; i < UMB_H; ++i) band_cnt[i] = 0;

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (!fits_any(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (t >= BAND_LO && t < BAND_HI) band_inc(t);
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
            if (oy >= BAND_LO && oy < BAND_HI) band_dec(oy);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y >= BAND_LO && y < BAND_HI) {
                if ((unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits(y)) {
                    dy[i] = y;
                }
                /* else: wait a frame */
            } else {
                dy[i] = y;
            }
            t = dy[i];
            if (t >= BAND_LO && t < BAND_HI) band_inc(t);
        }

        /* --- OAM --- */
        draw_umbrella();
        p = OAM + 32;
        for (i = rot; i < NUM_DROPS; ++i) {
            p[0] = dy[i];
            p[1] = DROP_TILE;
            p[2] = 0;
            p[3] = dx[i];
            p += 4;
        }
        for (i = 0; i < rot; ++i) {
            p[0] = dy[i];
            p[1] = DROP_TILE;
            p[2] = 0;
            p[3] = dx[i];
            p += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
