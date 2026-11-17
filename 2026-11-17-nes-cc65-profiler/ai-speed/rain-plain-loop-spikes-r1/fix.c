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

/* a drop at row y touches the band iff BAND_LO < y < BAND_HI */
#define BAND_LO     (UMB_Y - 8)          /* 192 */
#define BAND_HI     (UMB_Y + UMB_H)      /* 216 */

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

/* scratch statics (faster than stack locals in cc65) */
static unsigned char s_l, s_hi;
static unsigned char s_y, s_oy;
static unsigned char s_i, s_o;

/* general versions, used only during init */
static void band_add(unsigned char y, signed char d)
{
    unsigned char l, hi;
    if (y + 8 <= UMB_Y || y >= UMB_Y + UMB_H) return;
    l = (y > UMB_Y) ? y : UMB_Y;
    hi = y + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
    for (; l <= hi; ++l) band_cnt[l - UMB_Y] += d;
}

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

/* fast versions: caller guarantees BAND_LO < y < BAND_HI */
static void band_inc(void)
{
    /* uses s_y */
    s_l = (s_y < UMB_Y) ? 0 : (unsigned char)(s_y - UMB_Y);
    s_hi = s_y - (UMB_Y - 7);
    if (s_hi > UMB_H - 1) s_hi = UMB_H - 1;
    for (; s_l <= s_hi; ++s_l) ++band_cnt[s_l];
}

static void band_dec(void)
{
    s_l = (s_y < UMB_Y) ? 0 : (unsigned char)(s_y - UMB_Y);
    s_hi = s_y - (UMB_Y - 7);
    if (s_hi > UMB_H - 1) s_hi = UMB_H - 1;
    for (; s_l <= s_hi; ++s_l) --band_cnt[s_l];
}

static unsigned char band_ok(void)
{
    s_l = (s_y < UMB_Y) ? 0 : (unsigned char)(s_y - UMB_Y);
    s_hi = s_y - (UMB_Y - 7);
    if (s_hi > UMB_H - 1) s_hi = UMB_H - 1;
    for (; s_l <= s_hi; ++s_l) {
        if (band_cnt[s_l] >= BAND_MAX) return 0;
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
    unsigned char k, pad, t;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (s_i = 0; s_i < UMB_H; ++s_i) band_cnt[s_i] = 0;

    for (s_i = 0; s_i < NUM_DROPS; ++s_i) {
        dx[s_i] = rnd8();
        if (dx[s_i] > 247) dx[s_i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (!band_fits(t)) t = rnd8() % DROP_BOTTOM;
        dy[s_i] = t;
        band_add(t, 1);
        dsp[s_i] = 1 + (s_i & 3) + ((s_i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM fields: umbrella Y/tile/attr, drop tile/attr */
    for (k = 0; k < 8; ++k) {
        s_o = k << 2;
        OAM[s_o]     = (k < 4) ? UMB_Y : UMB_Y + 8;
        OAM[s_o + 1] = (k < 4) ? 0x01 : 0x02;
        OAM[s_o + 2] = 1;
    }
    for (s_o = 32; s_o < 192; s_o += 4) {
        OAM[s_o + 1] = DROP_TILE;
        OAM[s_o + 2] = 0;
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
        for (s_i = 0; s_i < NUM_DROPS; ++s_i) {
            s_oy = dy[s_i];
            s_y = s_oy + dsp[s_i];
            if (s_oy > BAND_LO && s_oy < BAND_HI) {
                s_y = s_oy;
                band_dec();
                s_y = s_oy + dsp[s_i];
            }
            if (s_y >= DROP_BOTTOM) {
                respawn(s_i);
            } else if (s_y > BAND_LO) {
                if (s_y < BAND_HI) {
                    if ((unsigned int)dx[s_i] + 8 > ux &&
                        (unsigned int)dx[s_i] < (unsigned int)ux + UMB_W) {
                        respawn(s_i);        /* caught by the umbrella */
                    } else if (band_ok()) {
                        dy[s_i] = s_y;
                    }
                } else {
                    dy[s_i] = s_y;
                }
            } else {
                dy[s_i] = s_y;
            }
            s_y = dy[s_i];
            if (s_y > BAND_LO && s_y < BAND_HI) band_inc();
        }

        /* --- OAM --- */
        OAM[3]  = ux;
        OAM[7]  = ux + 8;
        OAM[11] = ux + 16;
        OAM[15] = ux + 24;
        OAM[19] = ux;
        OAM[23] = ux + 8;
        OAM[27] = ux + 16;
        OAM[31] = ux + 24;

        s_o = 32;
        for (s_i = rot; s_i < NUM_DROPS; ++s_i) {
            OAM[s_o]     = dy[s_i];
            OAM[s_o + 3] = dx[s_i];
            s_o += 4;
        }
        for (s_i = 0; s_i < rot; ++s_i) {
            OAM[s_o]     = dy[s_i];
            OAM[s_o + 3] = dx[s_i];
            s_o += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
