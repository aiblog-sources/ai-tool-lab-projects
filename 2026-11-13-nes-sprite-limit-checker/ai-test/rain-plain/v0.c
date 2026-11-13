/* main.c - Rain: 40 falling drops (8x8) and a d-pad-controlled umbrella (32x16).
 *
 * Sprite budget: the umbrella takes OAM slots 0-7 (the highest priority) so
 * it never loses sprites to the 8-per-scanline limit. The 40 drops take
 * slots 8-47. Their priority order rotates every frame, so in the rare case
 * of more than 8 sprites on one line the loss is spread across drops as
 * flicker instead of one drop always vanishing.
 */
#include "nes.h"

#define NUM_DROPS   40
#define DROP_TILE   0x08   /* small dot */
#define UMB_Y       200    /* OAM Y of the canopy's top row */
#define UMB_W       32
#define UMB_SPEED   3
#define DROP_BOTTOM 232

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

static unsigned char ux;
static unsigned char rot;

static void respawn(unsigned char i)
{
    unsigned char x = rnd8();
    if (x > 247) x -= 8;
    dx[i] = x;
    dy[i] = 0;
}

static void draw_umbrella(void)
{
    unsigned char c, row, n = 0;
    for (row = 0; row < 2; ++row) {
        for (c = 0; c < 4; ++c) {
            OAM_Y(n)    = UMB_Y + (row << 3);
            OAM_TILE(n) = row ? 0x02 : 0x01;
            OAM_ATTR(n) = 1;
            OAM_X(n)    = ux + (c << 3);
            ++n;
        }
    }
}

void main(void)
{
    unsigned char i, k, pad, slot, y;
    unsigned int  top;

    ppu_setup(palette);
    oam_hide_all();

    ux = (256 - UMB_W) / 2;
    rot = 0;

    for (i = 0; i < NUM_DROPS; ++i) {
        dx[i] = rnd8();
        if (dx[i] > 247) dx[i] -= 8;
        dy[i] = rnd8() % DROP_BOTTOM;
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
            y = dy[i] + dsp[i];
            top = y;
            if (y >= DROP_BOTTOM) {
                respawn(i);
            } else if (top + 8 > UMB_Y && top < UMB_Y + 16 &&
                       (unsigned int)dx[i] + 8 > ux &&
                       (unsigned int)dx[i] < (unsigned int)ux + UMB_W) {
                respawn(i);          /* caught by the umbrella */
            } else {
                dy[i] = y;
            }
        }

        /* --- OAM --- */
        draw_umbrella();
        for (k = 0; k < NUM_DROPS; ++k) {
            i = k + rot;
            if (i >= NUM_DROPS) i -= NUM_DROPS;
            slot = 8 + k;
            OAM_Y(slot)    = dy[i];
            OAM_TILE(slot) = DROP_TILE;
            OAM_ATTR(slot) = 0;
            OAM_X(slot)    = dx[i];
        }
        ++rot;
        if (rot >= NUM_DROPS) rot = 0;

        oam_ready();
        wait_frame();
    }
}
