The slow path no longer removes and re-adds each drop's full 8-line footprint on every step. A drop only changes the counters on the lines it leaves and the lines it newly covers, at most 5 lines each. It also now checks only the newly covered lines for room, because the lines it already covers can't exceed the limit. The counter values, and so every decision about which drop waits, are identical to before.

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
#define BAND_END    (UMB_Y + UMB_H - 1)

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

static void draw_umbrella(void)
{
    static unsigned char *o;
    static unsigned char x;
    o = OAM;
    x = ux;
    o[0]  = UMB_Y;      o[3]  = x;
    o[4]  = UMB_Y;      o[7]  = x + 8;
    o[8]  = UMB_Y;      o[11] = x + 16;
    o[12] = UMB_Y;      o[15] = x + 24;
    o[16] = UMB_Y + 8;  o[19] = x;
    o[20] = UMB_Y + 8;  o[23] = x + 8;
    o[24] = UMB_Y + 8;  o[27] = x + 16;
    o[28] = UMB_Y + 8;  o[31] = x + 24;
}

void main(void)
{
    static unsigned char i, k, pad, y, t, old, l, hi;
    static unsigned int  top;
    static unsigned char *o;

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

    /* constant tile/attribute bytes, written once */
    o = OAM;
    for (i = 0; i < 8; ++i) {
        o[1] = (i < 4) ? 0x01 : 0x02;
        o[2] = 1;
        o += 4;
    }
    for (i = 0; i < NUM_DROPS; ++i) {
        o[1] = DROP_TILE;
        o[2] = 0;
        o += 4;
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
            if (y <= UMB_Y - 8) {
                /* entirely above the band: no band, catch or bottom logic */
                dy[i] = y;
                continue;
            }
            top = y;
            if (y >= DROP_BOTTOM ||
                (top + 8 > UMB_Y && top < UMB_Y + 16 &&
                 (unsigned int)dx[i] + 8 > ux &&
                 (unsigned int)dx[i] < (unsigned int)ux + UMB_W)) {
                /* respawn (bottom or caught): remove the old footprint */
                if (old < UMB_Y + UMB_H && old + 8 > UMB_Y) {
                    l = (old > UMB_Y) ? old : UMB_Y;
                    hi = old + 7;
                    if (hi > BAND_END) hi = BAND_END;
                    for (; l <= hi; ++l) --band_cnt[l - UMB_Y];
                }
                respawn(i);
                continue;
            }
            /* newly covered lines: old+8 .. y+7 (clipped to the band) */
            l = old + 8;
            if (l < UMB_Y) l = UMB_Y;
            hi = y + 7;
            if (hi > BAND_END) hi = BAND_END;
            for (t = l; t <= hi; ++t) {
                if (band_cnt[t - UMB_Y] >= BAND_MAX) break;
            }
            if (t <= hi) continue;   /* no room: wait a frame */

            /* add newly covered lines */
            for (; l <= hi; ++l) ++band_cnt[l - UMB_Y];
            /* remove vacated lines: old .. y-1 (clipped) */
            l = (old > UMB_Y) ? old : UMB_Y;
            hi = y - 1;
            if (hi > BAND_END) hi = BAND_END;
            for (; l <= hi; ++l) --band_cnt[l - UMB_Y];
            dy[i] = y;
        }

        /* --- OAM --- */
        draw_umbrella();
        o = OAM + 32;
        i = rot;
        for (k = 0; k < NUM_DROPS; ++k) {
            o[0] = dy[i];
            o[3] = dx[i];
            o += 4;
            ++i;
            if (i == NUM_DROPS) i = 0;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```