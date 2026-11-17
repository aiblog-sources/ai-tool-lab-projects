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
#define BAND_LO     (UMB_Y - 7)          /* 193 */
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

/* add (+1) or remove (-1) a drop at row y; caller guarantees y in band range */
static void band_add_in(unsigned char y, signed char d)
{
    register unsigned char *p;
    unsigned char n;
    if (y > UMB_Y) {
        unsigned char s = y - UMB_Y;
        p = band_cnt + s;
        n = (y - (UMB_Y - 7) > UMB_H - 1) ? (UMB_H - s) : 8;
    } else {
        p = band_cnt;
        n = y - (UMB_Y - 8);        /* lines UMB_Y .. y+7 */
    }
    do {
        *p += d;
        ++p;
    } while (--n);
}

/* generic version (range check included) */
static void band_add(unsigned char y, signed char d)
{
    if (y < BAND_LO || y >= BAND_HI) return;
    band_add_in(y, d);
}

/* caller guarantees y in band range */
static unsigned char band_fits_in(unsigned char y)
{
    register unsigned char *p;
    unsigned char n;
    if (y > UMB_Y) {
        unsigned char s = y - UMB_Y;
        p = band_cnt + s;
        n = (y - (UMB_Y - 7) > UMB_H - 1) ? (UMB_H - s) : 8;
    } else {
        p = band_cnt;
        n = y - (UMB_Y - 8);
    }
    do {
        if (*p >= BAND_MAX) return 0;
        ++p;
    } while (--n);
    return 1;
}

static unsigned char band_fits(unsigned char y)
{
    if (y < BAND_LO || y >= BAND_HI) return 1;
    return band_fits_in(y);
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
    register unsigned char *o = OAM;
    unsigned char c, row, x0 = ux;
    for (row = 0; row < 2; ++row) {
        for (c = 0; c < 4; ++c) {
            o[0] = UMB_Y + (row << 3);
            o[1] = row ? 0x02 : 0x01;
            o[2] = 1;
            o[3] = x0 + (c << 3);
            o += 4;
        }
    }
}

void main(void)
{
    unsigned char i, k, pad, y, t, oy;
    register unsigned char *o;

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
            oy = dy[i];
            y = oy + dsp[i];
            if (oy >= BAND_LO && oy < BAND_HI) band_add_in(oy, -1);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y >= BAND_LO && y < BAND_HI) {
                if ((unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits_in(y)) {
                    dy[i] = y;
                }
            } else {
                dy[i] = y;               /* outside band: always fits */
            }
            oy = dy[i];
            if (oy >= BAND_LO && oy < BAND_HI) band_add_in(oy, 1);
        }

        /* --- OAM --- */
        draw_umbrella();
        o = OAM + 32;
        i = rot;
        for (k = 0; k < NUM_DROPS; ++k) {
            o[0] = dy[i];
            o[1] = DROP_TILE;
            o[2] = 0;
            o[3] = dx[i];
            o += 4;
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

I haven't compiled or run this, so the 60 fps result is unconfirmed.

- **Band bookkeeping:** drops outside the umbrella band now skip the band functions entirely. Inside the band, the counter loops walk a pointer over `band_cnt` instead of re-indexing it. That targets the slow loops in the profile.
- **Same game logic:** the band logic and the order of `rnd8` calls are unchanged. A drop that would put a 5th drop on a line still waits, and the umbrella catch test still runs only for drops in that range.
- **OAM writing:** OAM is written through a running pointer, and the drop index wraps incrementally instead of using `k + rot` and a subtract.