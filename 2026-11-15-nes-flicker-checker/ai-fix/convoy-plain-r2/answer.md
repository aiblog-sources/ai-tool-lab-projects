The checker's "fully shown" figure was already at its upper bound of 52.4%, so the only thing left to improve is how evenly that visibility is shared. The old code rotated through cars by their index in the on-screen list. That list changes size and shifts as cars enter and leave, which caused the uneven 4-frame gaps.

This version tracks how many frames each car has gone without being drawn. Each frame it fills the 8 sprite slots on the row with the cars that have waited longest. It only picks a car if both of its halves fit in the remaining slots, so no car is drawn as a half. Cars that don't fit are written to OAM after the chosen ones, so the PPU drops them while the chosen cars are still fully drawn. The objects, sizes, positions and player behavior are unchanged.

I haven't built this or run it through the checker.

```c
/* main.c - convoy of 10 cars + player, cc65 / NES, mapper 0 */
#include "nes.h"

#define NCARS     10
#define CAR_ROW   112      /* OAM Y of the convoy row */
#define CAR_SPACE 27       /* 10 * 27 = 270 ~ screen width + one car */
#define WRAP_LEN  272      /* distance cars travel before re-entering */
#define OAM_USED  24       /* 4 player sprites + 2 per car */
#define LINE_MAX  8        /* hardware sprites per scanline */

/* The NES draws at most 8 sprites per scanline. Ten 16x8 cars need up to 20
 * 8x8 sprites on one row, so at most ~4 cars can be shown per frame.
 * Each car keeps an age (frames since it was last drawn). Every frame the
 * visible cars are sorted oldest-first and whole cars are picked until the
 * 8 slots of the row are used, so the wait is shared evenly regardless of
 * cars entering or leaving. Picked cars go first in OAM; the rest follow
 * (the PPU drops them). The player is always first in OAM. */

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
static unsigned char age[NCARS];
static unsigned char cnt[NCARS];
static unsigned char chosen[NCARS];
static unsigned char ord[NCARS];
static unsigned char oam_n;

static void put(unsigned char n, unsigned char x, unsigned char y,
                unsigned char tile, unsigned char attr)
{
    OAM_Y(n)    = y;
    OAM_TILE(n) = tile;
    OAM_ATTR(n) = attr;
    OAM_X(n)    = x;
}

static void emit_car(unsigned char i)
{
    int x = car_x[i];
    unsigned char c = i % 3;   /* palette 0..2; tile $01 = colour 1 */

    /* left half */
    if (x >= 0 && x < 256) {
        put(oam_n, (unsigned char)x, CAR_ROW, 0x01, c);
        ++oam_n;
    }
    /* right half */
    if (x + 8 >= 0 && x + 8 < 256) {
        put(oam_n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);
        ++oam_n;
    }
}

void main(void)
{
    unsigned char i, k, pad, nv, p, budget;
    int x;

    ppu_setup(palette);
    oam_hide_all();

    for (i = 0; i < NCARS; ++i) {
        car_x[i] = (int)i * CAR_SPACE - 16;
        age[i] = 0;
    }

    px = 120;
    py = 176;

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

        /* collect cars with sprites on screen, sorted oldest-first */
        nv = 0;
        for (i = 0; i < NCARS; ++i) {
            unsigned char m = 0;
            x = car_x[i];
            chosen[i] = 0;
            if (x >= 0 && x < 256) ++m;
            if (x + 8 >= 0 && x + 8 < 256) ++m;
            cnt[i] = m;
            if (m) {
                p = nv;
                while (p > 0 && age[ord[p - 1]] < age[i]) {
                    ord[p] = ord[p - 1];
                    --p;
                }
                ord[p] = i;
                ++nv;
            }
        }

        /* pick whole cars until the row's 8 slots are used */
        budget = LINE_MAX;
        for (k = 0; k < nv; ++k) {
            i = ord[k];
            if (cnt[i] <= budget) {
                budget -= cnt[i];
                chosen[i] = 1;
                age[i] = 0;
            } else if (age[i] < 255) {
                ++age[i];
            }
        }

        oam_n = 4;
        for (k = 0; k < nv; ++k)
            if (chosen[ord[k]]) emit_car(ord[k]);
        for (k = 0; k < nv; ++k)
            if (!chosen[ord[k]]) emit_car(ord[k]);

        /* hide unused slots */
        while (oam_n < OAM_USED) {
            put(oam_n, 0, 0xFF, 0x00, 0);
            ++oam_n;
        }

        oam_ready();
        wait_frame();
    }
}
```