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
 * Speed: the band counters are only touched for drops that are inside the
 * band (y 193..215), the helpers take their argument in a register, and the
 * loop variables are static so cc65 does not use its software stack.
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

/* first/last band_cnt index touched by a drop at y (IN_BAND(y) must hold) */
static void __fastcall__ band_range(unsigned char y)
{
    bl = (y > UMB_Y) ? (unsigned char)(y - UMB_Y) : 0;
    bh = y - (UMB_Y - 7);          /* y + 7 - UMB_Y */
    if (bh > UMB_H - 1) bh = UMB_H - 1;
}

/* y must be inside the band */
static void __fastcall__ band_inc(unsigned char y)
{
    band_range(y);
    for (; bl <= bh; ++bl) ++band_cnt[bl];
}

static void __fastcall__ band_dec(unsigned char y)
{
    band_range(y);
    for (; bl <= bh; ++bl) --band_cnt[bl];
}

/* can a drop at row y be added without exceeding BAND_MAX on any line? */
static unsigned char __fastcall__ band_fits(unsigned char y)
{
    if (!IN_BAND(y)) return 1;
    band_range(y);
    for (; bl <= bh; ++bl) {
        if (band_cnt[bl] >= BAND_MAX) return 0;
    }
    return 1;
}

static void __fastcall__ respawn(unsigned char i)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[i] = x;
    dy[i] = 0;
}

static unsigned char ux_c, ux_x;

static void draw_umbrella(void)
{
    ux_x = ux;
    for (ux_c = 0; ux_c < 16; ux_c += 4) {
        OAM[ux_c]      = UMB_Y;
        OAM[ux_c + 1]  = 0x01;
        OAM[ux_c + 2]  = 1;
        OAM[ux_c + 3]  = ux_x;
        OAM[ux_c + 16] = UMB_Y + 8;
        OAM[ux_c + 17] = 0x02;
        OAM[ux_c + 18] = 1;
        OAM[ux_c + 19] = ux_x;
        ux_x += 8;
    }
}

#define DROPS_OAM ((unsigned char *)0x0220)   /* slot 8 */

static unsigned char i, k, pad, y, t, oy, x;

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
            oy = dy[i];
            y = oy + dsp[i];
            if (IN_BAND(oy)) band_dec(oy);
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (IN_BAND(y) &&
                       (unsigned int)dx[i] + 8 > ux &&
                       (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                respawn(i);          /* caught by the umbrella */
            } else if (band_fits(y)) {
                dy[i] = y;
            }
            /* else: wait a frame so no line gets a 5th drop in the band */
            t = dy[i];
            if (IN_BAND(t)) band_inc(t);
        }

        /* --- OAM --- */
        draw_umbrella();
        x = 0;
        k = rot;
        for (i = 0; i < NUM_DROPS; ++i) {
            DROPS_OAM[x]     = dy[k];
            DROPS_OAM[x + 1] = DROP_TILE;
            DROPS_OAM[x + 2] = 0;
            DROPS_OAM[x + 3] = dx[k];
            x += 4;
            ++k;
            if (k >= NUM_DROPS) k = 0;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```

I haven't compiled or profiled this. The changes aim to keep behavior identical, including the order of `rnd8` calls.

- **Band counters:** `band_add` and `band_fits` were called for every drop, even though most drops are nowhere near the umbrella band. The band check now happens inline, so the helpers only run for drops in y 193..215. Everything else skips them.
- **Collision test:** the collision test uses the same in-band check first, so the 16-bit comparisons only run for drops in the band.
- **Helpers:** they are `__fastcall__` and split into `band_inc` and `band_dec`. This removes the signed `d` argument and the software-stack pushes.
- **Static variables:** the loop variables in `main` are static, so cc65 doesn't spend cycles on the software stack for them.
- **OAM writes:** the drop sprites are written with a byte index into a fixed address (`0x0220`, slot 8), and the rotation wraps with a simple compare. This replaces the per-sprite shift and multiply.
- **Umbrella:** it is written with the same index approach, one pass over the 4 columns for both rows.