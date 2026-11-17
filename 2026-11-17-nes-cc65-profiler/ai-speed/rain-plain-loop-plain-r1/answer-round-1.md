```c
/* main.c - Rain: 40 falling drops (8x8) and a d-pad-controlled umbrella (32x16).
 *
 * Same behaviour as before, but cheaper per loop:
 *  - fast path: a drop that stays above the umbrella band rows only does
 *    dy += speed (it can't touch the band counters or the umbrella)
 *  - band counters use direct index math (no range clamps, no signed adds)
 *  - OAM drop slots are written with an 8-bit running offset
 *  - loop state is static, so cc65 avoids the software stack
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

#define BAND_TOP    (UMB_Y - 7)          /* 193: first y that overlaps the band */
#define BAND_N      (UMB_Y + UMB_H - BAND_TOP)   /* 23 rows of y overlap it */

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

/* loop state (static = absolute addressing, no stack frame) */
static unsigned char i, k, pad, y, t, old, inb, off;
static unsigned char bj, bl, bh;
static unsigned int  xl;

#define OAM_DY ((unsigned char *)0x0220)   /* Y of slot 8 */
#define OAM_DX ((unsigned char *)0x0223)   /* X of slot 8 */

/* set bl/bh = first/last band line index touched by a drop at row yy
   (caller guarantees yy in [BAND_TOP, BAND_TOP+BAND_N-1]) */
#define BAND_RANGE(yy)                       \
    bj = (unsigned char)((yy) - BAND_TOP);   \
    bl = (bj > 7) ? (unsigned char)(bj - 7) : 0; \
    bh = (bj > 15) ? 15 : bj;

static void band_inc(unsigned char yy)
{
    BAND_RANGE(yy)
    do {
        ++band_cnt[bl];
    } while (++bl <= bh);
}

static void band_dec(unsigned char yy)
{
    BAND_RANGE(yy)
    do {
        --band_cnt[bl];
    } while (++bl <= bh);
}

static unsigned char band_fits(unsigned char yy)
{
    BAND_RANGE(yy)
    do {
        if (band_cnt[bl] >= BAND_MAX) return 0;
    } while (++bl <= bh);
    return 1;
}

static void respawn(unsigned char n)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[n] = x;
    dy[n] = 0;
}

/* generic (init-time) helpers keeping the original semantics */
static unsigned char fits_any(unsigned char yy)
{
    if (yy + 8 <= UMB_Y || yy >= UMB_Y + UMB_H) return 1;
    return band_fits(yy);
}

static void add_any(unsigned char yy)
{
    if (yy + 8 <= UMB_Y || yy >= UMB_Y + UMB_H) return;
    band_inc(yy);
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    /* constant OAM fields */
    for (i = 0; i < 8; ++i) {
        OAM_Y(i)    = (i < 4) ? UMB_Y : UMB_Y + 8;
        OAM_TILE(i) = (i < 4) ? 0x01 : 0x02;
        OAM_ATTR(i) = 1;
    }
    for (i = 8; i < 8 + NUM_DROPS; ++i) {
        OAM_TILE(i) = DROP_TILE;
        OAM_ATTR(i) = 0;
    }

    for (i = 0; i < UMB_H; ++i) band_cnt[i] = 0;

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        t = rnd8() % DROP_BOTTOM;
        while (!fits_any(t)) t = rnd8() % DROP_BOTTOM;
        dy[i] = t;
        add_any(t);
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

            if (y < BAND_TOP) {          /* fast path: nowhere near the band */
                dy[i] = y;
                continue;
            }

            /* old row in the band? (old < y so old < 237, no wrap issue) */
            inb = (unsigned char)(old - BAND_TOP) < BAND_N;
            if (inb) band_dec(old);
            t = inb;                     /* t = old was in band */

            inb = 0;
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (y < UMB_Y + UMB_H) {      /* y in the band */
                xl = dx[i];
                if (xl + 8 > ux && xl < (unsigned int)ux + UMB_W) {
                    respawn(i);          /* caught by the umbrella */
                } else if (band_fits(y)) {
                    dy[i] = y;
                    inb = 1;
                } else {
                    inb = t;             /* waits at the old row */
                }
            } else {
                dy[i] = y;               /* below the band */
            }
            if (inb) band_inc(dy[i]);
        }

        /* --- OAM --- */
        t = ux;
        OAM_X(0) = t;       OAM_X(4) = t;
        t += 8;
        OAM_X(1) = t;       OAM_X(5) = t;
        t += 8;
        OAM_X(2) = t;       OAM_X(6) = t;
        t += 8;
        OAM_X(3) = t;       OAM_X(7) = t;

        off = 0;
        for (k = rot; k < NUM_DROPS; ++k) {
            OAM_DY[off] = dy[k];
            OAM_DX[off] = dx[k];
            off += 4;
        }
        for (k = 0; k < rot; ++k) {
            OAM_DY[off] = dy[k];
            OAM_DX[off] = dx[k];
            off += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
```

I haven't built or run this, so I can't confirm the worst-case loop now fits in a frame.

A drop that stays above row 193 now only does `dy += speed`. The band counters, the umbrella collision test and the per-drop function calls are all skipped for it. Behaviour for drops in or near the umbrella rows is unchanged: same counter updates, same wait-a-frame rule, same respawn conditions and rotation order.