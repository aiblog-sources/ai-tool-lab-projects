/* main.c - convoy of 10 cars + player, cc65 / NES, mapper 0 */
#include "nes.h"

#define NCARS     10
#define CAR_ROW   112      /* OAM Y of the convoy row */
#define CAR_SPACE 27       /* 10 * 27 = 270 ~ screen width + one car */
#define WRAP_LEN  272      /* distance cars travel before re-entering */
#define OAM_USED  24       /* 4 player sprites + 2 per car */

/* The NES draws at most 8 sprites per scanline. Ten 16x8 cars need up to 20
 * 8x8 sprites on one row, so at most ~4 cars can be shown per frame.
 * This is a hardware limit; the program uses flicker to share the 8 slots.
 * The player is always first in OAM (never dropped). Only cars that are
 * on screen are ranked, and each frame the starting point advances by one
 * full group of 4 cars so every visible car gets an equal share of frames. */

static const unsigned char palette[32] = {
    /* background */
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    /* sprites */
    0x0F,0x16,0x27,0x30, 0x0F,0x11,0x21,0x30,
    0x0F,0x1A,0x2A,0x30, 0x0F,0x14,0x24,0x30
};

static int car_x[NCARS];
static unsigned char px, py;
static unsigned char start;
static unsigned char vis[NCARS];

static void put(unsigned char n, unsigned char x, unsigned char y,
                unsigned char tile, unsigned char attr)
{
    OAM_Y(n)    = y;
    OAM_TILE(n) = tile;
    OAM_ATTR(n) = attr;
    OAM_X(n)    = x;
}

void main(void)
{
    unsigned char i, j, k, n, pad, c, nv, s;
    int x;

    ppu_setup(palette);
    oam_hide_all();

    for (i = 0; i < NCARS; ++i)
        car_x[i] = (int)i * CAR_SPACE - 16;

    px = 120;
    py = 176;
    start = 0;

    for (;;) {
        /* ---- update ---- */
        pad = pad_read();
        if ((pad & PAD_LEFT)  && px >= 2)   px -= 2;
        if ((pad & PAD_RIGHT) && px <= 238) px += 2;
        if ((pad & PAD_UP)    && py >= 2)   py -= 2;
        if ((pad & PAD_DOWN)  && py <= 221) py += 2;

        for (i = 0; i < NCARS; ++i) {
            car_x[i] += 1;
            if (car_x[i] >= 256)
                car_x[i] -= WRAP_LEN;
        }

        /* ---- fill OAM ---- */
        /* player: 4 sprites, OAM 0..3, always highest priority */
        put(0, px,     py,     0x06, 0x03);
        put(1, px + 8, py,     0x06, 0x43);
        put(2, px,     py + 8, 0x06, 0x83);
        put(3, px + 8, py + 8, 0x06, 0xC3);

        /* collect cars with at least one half on screen */
        nv = 0;
        for (i = 0; i < NCARS; ++i) {
            x = car_x[i];
            if (x < 256 && x + 8 >= 0 && (x >= 0 || x + 8 < 256 || x + 8 >= 0))
                vis[nv++] = i;
        }

        n = 4;
        if (nv) {
            s = start % nv;
            for (k = 0; k < nv; ++k) {
                j = s + k;
                if (j >= nv) j -= nv;
                i = vis[j];
                x = car_x[i];
                c = i % 3;          /* palette 0..2; tile $01 = colour 1 */

                /* left half */
                if (x >= 0 && x < 256) {
                    put(n, (unsigned char)x, CAR_ROW, 0x01, c);
                    ++n;
                }
                /* right half */
                if (x + 8 >= 0 && x + 8 < 256) {
                    put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);
                    ++n;
                }
            }
            /* next frame starts one group of 4 cars later */
            start = (unsigned char)((s + 4) % nv);
        }

        /* hide unused slots */
        while (n < OAM_USED) {
            put(n, 0, 0xFF, 0x00, 0);
            ++n;
        }

        oam_ready();
        wait_frame();
    }
}
