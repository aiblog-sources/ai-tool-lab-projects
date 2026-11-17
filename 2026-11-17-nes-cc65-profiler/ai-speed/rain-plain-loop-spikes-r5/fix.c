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
 * Speed-ups: when a drop moves down, only the lines it newly covers need to
 * be checked/incremented and only the lines it leaves need to be
 * decremented (lines it keeps are already counted, and the counters never
 * exceed BAND_MAX), so no full remove/test/add pass is needed. Working
 * variables are statics (absolute addressing) instead of software-stack
 * locals, and constant OAM bytes are written once.
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

/* a drop at row y touches the band iff y in 193..215 */
#define IN_BAND(y)  ((unsigned char)((y) - (UMB_Y - 7)) < (UMB_H + 7))

#define DROP_OAM_Y ((unsigned char *)0x0220)
#define DROP_OAM_X ((unsigned char *)0x0223)

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

/* scratch (statics: cheaper than software-stack locals) */
static unsigned char i, j, pad, y, old, lo, hi, t;

/* lines lo..hi are absolute rows within UMB_Y..UMB_Y+UMB_H-1 (or empty) */
static void rng_inc(void)
{
    for (j = lo; j <= hi; ++j) ++band_cnt[j - UMB_Y];
}

static void rng_dec(void)
{
    for (j = lo; j <= hi; ++j) --band_cnt[j - UMB_Y];
}

static unsigned char rng_ok(void)
{
    for (j = lo; j <= hi; ++j) {
        if (band_cnt[j - UMB_Y] >= BAND_MAX) return 0;
    }
    return 1;
}

/* range of lines covered by a drop at row t (t must be IN_BAND) */
static void cover(unsigned char r)
{
    lo = (r > UMB_Y) ? r : UMB_Y;
    hi = r + 7;
    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
}

static void respawn(void)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[i] = x;
    dy[i] = 0;
}

/* drop i is leaving row old for good: free its lines */
static void release(void)
{
    if (IN_BAND(old)) {
        cover(old);
        rng_dec();
    }
}

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
        for (;;) {
            if (!IN_BAND(t)) break;
            cover(t);
            if (rng_ok()) break;
            t = rnd8() % DROP_BOTTOM;
        }
        dy[i] = t;
        if (IN_BAND(t)) {
            cover(t);
            rng_inc();
        }
        dsp[i] = 1 + (i & 3) + ((i >> 2) & 1);   /* speeds 1..5 */
    }

    /* constant OAM bytes: umbrella */
    for (i = 0; i < 8; ++i) {
        j = i << 2;
        OAM[j]     = UMB_Y + ((i >> 2) << 3);
        OAM[j + 1] = (i >> 2) ? 0x02 : 0x01;
        OAM[j + 2] = 1;
    }
    /* constant OAM bytes: drops */
    for (i = 0; i < NUM_DROPS; ++i) {
        j = i << 2;
        OAM[32 + j + 1] = DROP_TILE;
        OAM[32 + j + 2] = 0;
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
            if (y >= DROP_BOTTOM) {
                release();
                respawn();
            } else if (!IN_BAND(old) && !IN_BAND(y)) {
                dy[i] = y;                      /* nowhere near the band */
            } else {
                if (IN_BAND(y) &&
                    (unsigned int)dx[i] + 8 > ux &&
                    (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                    release();
                    respawn();                   /* caught by the umbrella */
                } else {
                    /* lines newly covered by the drop at y */
                    lo = old + 8;
                    if (lo < UMB_Y) lo = UMB_Y;
                    hi = y + 7;
                    if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
                    if (rng_ok()) {
                        rng_inc();
                        /* lines left behind */
                        lo = (old > UMB_Y) ? old : UMB_Y;
                        hi = old + 7;
                        if (hi > y - 1) hi = y - 1;
                        if (hi > UMB_Y + UMB_H - 1) hi = UMB_Y + UMB_H - 1;
                        rng_dec();
                        dy[i] = y;
                    }
                    /* else: wait a frame so no line gets a 5th drop */
                }
            }
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

        j = 0;
        for (i = rot; i < NUM_DROPS; ++i) {
            DROP_OAM_Y[j] = dy[i];
            DROP_OAM_X[j] = dx[i];
            j += 4;
        }
        for (i = 0; i < rot; ++i) {
            DROP_OAM_Y[j] = dy[i];
            DROP_OAM_X[j] = dx[i];
            j += 4;
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
