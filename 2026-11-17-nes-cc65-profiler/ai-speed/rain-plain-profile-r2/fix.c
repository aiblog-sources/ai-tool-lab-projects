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
 * Speed: drops outside the umbrella band (the vast majority) take a fast
 * path with no band bookkeeping, and the constant OAM bytes (tiles,
 * attributes, umbrella Y) are written once instead of every frame.
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

/* a drop at y touches the band when y in [UMB_Y-7, UMB_Y+UMB_H-1] */
#define BAND_LO     (UMB_Y - 7)
#define BAND_N      (UMB_H + 7)
#define IN_BAND(v)  ((unsigned char)((v) - BAND_LO) < BAND_N)

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

/* constant umbrella bytes, written once */
static void init_umbrella(void)
{
    unsigned char n;
    for (n = 0; n < 8; ++n) {
        OAM_Y(n)    = UMB_Y + ((n >> 2) << 3);
        OAM_TILE(n) = (n >> 2) ? 0x02 : 0x01;
        OAM_ATTR(n) = 1;
    }
}

static void umbrella_x(void)
{
    unsigned char x = ux;
    OAM_X(0) = x; OAM_X(4) = x;
    x += 8;
    OAM_X(1) = x; OAM_X(5) = x;
    x += 8;
    OAM_X(2) = x; OAM_X(6) = x;
    x += 8;
    OAM_X(3) = x; OAM_X(7) = x;
}

static unsigned char *op;

void main(void)
{
    register unsigned char i;
    register unsigned char j;
    unsigned char pad, y, oy, t;

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

    init_umbrella();
    umbrella_x();
    op = OAM + 32;
    for (i = 0; i < NUM_DROPS; ++i) {
        OAM_TILE(8 + i) = DROP_TILE;
        OAM_ATTR(8 + i) = 0;
    }

    for (;;) {
        /* --- umbrella input --- */
        pad = pad_read();
        if (pad & (PAD_LEFT | PAD_RIGHT)) {
            if (pad & PAD_LEFT) {
                ux = (ux > UMB_SPEED) ? ux - UMB_SPEED : 0;
            }
            if (pad & PAD_RIGHT) {
                ux += UMB_SPEED;
                if (ux > 256 - UMB_W) ux = 256 - UMB_W;
            }
            umbrella_x();
        }

        /* --- drops --- */
        for (i = 0; i < NUM_DROPS; ++i) {
            oy = dy[i];
            y = oy + dsp[i];
            if (!IN_BAND(oy) && !IN_BAND(y) && y < DROP_BOTTOM) {
                dy[i] = y;           /* fast path: nothing to do with the band */
                continue;
            }
            if (IN_BAND(oy)) band_add(oy, -1);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y + 8 > UMB_Y && y < UMB_Y + 16 &&
                       (unsigned int)dx[i] + 8 > ux &&
                       (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                respawn(i);          /* caught by the umbrella */
            } else if (band_fits(y)) {
                dy[i] = y;
            }
            /* else: wait a frame so no line gets a 5th drop in the band */
            y = dy[i];
            if (IN_BAND(y)) band_add(y, 1);
        }

        /* --- OAM: slot 8+k shows drop (k+rot) mod NUM_DROPS --- */
        op = OAM + 32;
        for (j = rot; j < NUM_DROPS; ++j) {
            op[0] = dy[j];
            op[3] = dx[j];
            op += 4;
        }
        for (j = 0; j < rot; ++j) {
            op[0] = dy[j];
            op[3] = dx[j];
            op += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
