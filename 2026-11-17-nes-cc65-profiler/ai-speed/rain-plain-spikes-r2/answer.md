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

/* a drop at row y touches the band iff UMB_Y-7 <= y < UMB_Y+UMB_H */
#define IN_BAND(y) ((unsigned char)((y) - (UMB_Y - 7)) < (UMB_H + 7))

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

/* caller guarantees IN_BAND(y); computes band_cnt index range */
static void band_inc(unsigned char y)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    hi = (unsigned char)(y - (UMB_Y - 7));
    if (hi > UMB_H - 1) hi = UMB_H - 1;
    for (; l <= hi; ++l) ++band_cnt[l];
}

static void band_dec(unsigned char y)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    hi = (unsigned char)(y - (UMB_Y - 7));
    if (hi > UMB_H - 1) hi = UMB_H - 1;
    for (; l <= hi; ++l) --band_cnt[l];
}

/* caller guarantees IN_BAND(y) */
static unsigned char band_ok(unsigned char y)
{
    unsigned char l, hi;
    l = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    hi = (unsigned char)(y - (UMB_Y - 7));
    if (hi > UMB_H - 1) hi = UMB_H - 1;
    for (; l <= hi; ++l) {
        if (band_cnt[l] >= BAND_MAX) return 0;
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

static void draw_umbrella(void)
{
    unsigned char c, n = 0;
    unsigned char *p = OAM;
    unsigned char x = ux;
    for (c = 0; c < 4; ++c) {
        p[0] = UMB_Y;
        p[1] = 0x01;
        p[2] = 1;
        p[3] = x;
        p[16] = UMB_Y + 8;
        p[17] = 0x02;
        p[18] = 1;
        p[19] = x;
        p += 4;
        x += 8;
    }
    (void)n;
}

void main(void)
{
    unsigned char i, pad, y, t, old, nd;
    unsigned char *p;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (i = 0; i < UMB_H; ++i) band_cnt[i] = 0;

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (IN_BAND(t) && !band_ok(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        if (IN_BAND(t)) band_inc(t);
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
            old = dy[i];
            y = old + dsp[i];
            if (IN_BAND(old)) band_dec(old);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (IN_BAND(y)) {
                if (y + 8 > UMB_Y && y < UMB_Y + 16 &&
                    (unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_ok(y)) {
                    dy[i] = y;
                }
            } else {
                dy[i] = y;
            }
            nd = dy[i];
            if (IN_BAND(nd)) band_inc(nd);
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
```

I haven't compiled or profiled this, so the 60 fps result is unconfirmed. The changes aim to keep behaviour identical:

- **Band check inlined:** most drops are far from the umbrella band, so the band functions are no longer called for them at all.
- **Cheaper band loops:** the band loops now index `band_cnt` directly instead of subtracting `UMB_Y` on every pass.
- **Collision test:** it runs only when the new row is inside the band.
- **OAM copy:** it now uses a pointer and two loops instead of the wrap check and the shifted slot index.