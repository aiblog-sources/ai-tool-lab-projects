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

/* shared scratch (static = cheaper than the cc65 software stack) */
static unsigned char s_l, s_hi, s_n;
static unsigned char *s_p;

/* compute s_l, s_hi for a row y that is inside the band */
#define BAND_RANGE(y)                                   \
    s_l = ((y) > UMB_Y) ? (y) : UMB_Y;                  \
    s_hi = (y) + 7;                                     \
    if (s_hi > UMB_Y + UMB_H - 1) s_hi = UMB_Y + UMB_H - 1;

static void band_inc(unsigned char y)
{
    BAND_RANGE(y)
    s_n = s_hi - s_l + 1;
    s_p = band_cnt + (s_l - UMB_Y);
    do {
        ++*s_p;
        ++s_p;
    } while (--s_n);
}

static void band_dec(unsigned char y)
{
    BAND_RANGE(y)
    s_n = s_hi - s_l + 1;
    s_p = band_cnt + (s_l - UMB_Y);
    do {
        --*s_p;
        ++s_p;
    } while (--s_n);
}

/* can a drop at row y be added without exceeding BAND_MAX on any line? */
static unsigned char band_fits(unsigned char y)
{
    if (y < BAND_LO || y >= BAND_HI) return 1;
    BAND_RANGE(y)
    s_n = s_hi - s_l + 1;
    s_p = band_cnt + (s_l - UMB_Y);
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

static void draw_umbrella_x(void)
{
    unsigned char x = ux;
    OAM_X(0) = x;
    OAM_X(4) = x;
    x += 8;
    OAM_X(1) = x;
    OAM_X(5) = x;
    x += 8;
    OAM_X(2) = x;
    OAM_X(6) = x;
    x += 8;
    OAM_X(3) = x;
    OAM_X(7) = x;
}

static unsigned char pad, y, t, d, k, i;
static unsigned char *op;

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
        while (!band_fits(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (t >= BAND_LO && t < BAND_HI) band_inc(t);
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM fields: umbrella */
    for (i = 0; i < 8; ++i) {
        OAM_Y(i)    = UMB_Y + ((i >> 2) << 3);
        OAM_TILE(i) = (i >> 2) ? 0x02 : 0x01;
        OAM_ATTR(i) = 1;
    }
    /* constant OAM fields: drops */
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
            d = dy[i];
            y = d + dsp[i];
            if (d >= BAND_LO && d < BAND_HI) band_dec(d);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y >= BAND_LO && y < BAND_HI) {
                if ((unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits(y)) {
                    dy[i] = y;
                }
                /* else: wait a frame so no line gets a 5th drop */
            } else {
                dy[i] = y;               /* outside band: always fits */
            }
            d = dy[i];
            if (d >= BAND_LO && d < BAND_HI) band_inc(d);
        }

        /* --- OAM --- */
        draw_umbrella_x();
        op = OAM + 32;
        for (i = rot; i < NUM_DROPS; ++i) {
            op[0] = dy[i];
            op[3] = dx[i];
            op += 4;
        }
        for (i = 0; i < rot; ++i) {
            op[0] = dy[i];
            op[3] = dx[i];
            op += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```

I haven't compiled or profiled this. These are the changes:

- **Band counters:** they are only touched when a drop is actually in the band. Most drops skip `band_add` and `band_fits` entirely.
- **Collision test:** it only runs when the new `y` is in the band range, which is the same condition the original tested first.
- **Band loops:** the loops use a pointer and a precomputed count instead of an indexed `band_cnt[l - UMB_Y]`.
- **Static variables:** hot variables are `static` rather than stack locals, which is cheaper on cc65.
- **Constant OAM fields:** the umbrella's Y/tile/attribute and the drops' tile/attribute are written once at startup. Only the umbrella X and the drop Y/X are rewritten each loop.
- **Drop OAM copy:** the rotating copy is split into two loops, so it needs no wraparound check.

This assumes the library never overwrites the OAM shadow's constant fields after `oam_hide_all`.