/* main.c - convoy of 10 cars + player, cc65 / NES, mapper 0 */
#include "nes.h"

#define NCARS     10
#define CAR_ROW   112      /* OAM Y of the convoy row */
#define CAR_SPACE 27       /* 10 * 27 = 270 ~ screen width + one car */
#define WRAP_LEN  272      /* distance cars travel before re-entering */

/* The NES draws at most 8 sprites per scanline. Ten 16x8 cars need 20
 * 8x8 sprites on one row, so they cannot all be shown in a single frame.
 * The program therefore uses the usual flicker technique: the player is
 * always first in OAM (highest priority, never dropped), and the cars follow
 * in an order whose starting car rotates every frame. The hardware keeps
 * the first 8 sprites on a line, so each frame shows a different subset
 * of cars and every car is drawn regularly. */

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
    unsigned char i, k, n, pad, c;
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

        start += 3;                 /* 3 is coprime to 10: fair rotation */
        if (start >= NCARS) start -= NCARS;

        /* ---- fill OAM ---- */
        /* player: 4 sprites, OAM 0..3, always highest priority */
        put(0, px,     py,     0x06, 0x03);
        put(1, px + 8, py,     0x06, 0x43);
        put(2, px,     py + 8, 0x06, 0x83);
        put(3, px + 8, py + 8, 0x06, 0xC3);

        n = 4;
        for (k = 0; k < NCARS; ++k) {
            i = start + k;
            if (i >= NCARS) i -= NCARS;
            x = car_x[i];
            c = i % 3;              /* palette 0..2; tile $01 = colour 1 */

            /* left half */
            if (x >= 0 && x < 256)
                put(n, (unsigned char)x, CAR_ROW, 0x01, c);
            else
                put(n, 0, 0xFF, 0x00, 0);
            ++n;

            /* right half */
            if (x + 8 >= 0 && x + 8 < 256)
                put(n, (unsigned char)(x + 8), CAR_ROW, 0x02, c);
            else
                put(n, 0, 0xFF, 0x00, 0);
            ++n;
        }

        oam_ready();
        wait_frame();
    }
}
