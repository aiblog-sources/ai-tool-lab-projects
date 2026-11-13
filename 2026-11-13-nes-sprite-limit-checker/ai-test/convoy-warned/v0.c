/* main.c - convoy road demo for cc65 / NES */
#include "nes.h"

#define NUM_CARS   10
#define CAR_SPACING 96                       /* px between cars on the virtual road */
#define ROAD_LEN   (NUM_CARS * CAR_SPACING)  /* 960 px loop, wider than the screen */
#define CAR_Y      120
#define CAR_SPEED  2
#define PLAYER_SPEED 2

/*
 * Hardware limit: the PPU draws at most 8 sprites per scanline.
 * A 16x8 car needs 2 sprites, so 10 cars side by side (20 sprites) cannot all
 * be shown on one row without dropouts or flicker. The convoy therefore runs
 * on a virtual road of 960 px: the cars are spaced 96 px apart, and at most 3
 * of them touch the 256 px screen at once (max 6 sprites). The 16x16 player
 * adds 2 sprites per scanline, so the worst case is 8, which is exactly
 * the limit. A car that leaves on the right re-enters on the left after
 * travelling the rest of the road.
 */

static const unsigned char palette[32] = {
    /* background (all blank) */
    0x0F,0x0F,0x0F,0x0F, 0x0F,0x0F,0x0F,0x0F,
    0x0F,0x0F,0x0F,0x0F, 0x0F,0x0F,0x0F,0x0F,
    /* sprites */
    0x0F,0x16,0x2A,0x30,   /* pal 0: red   */
    0x0F,0x12,0x28,0x30,   /* pal 1: blue/yellow */
    0x0F,0x30,0x21,0x11,   /* pal 2: player */
    0x0F,0x1A,0x2A,0x30    /* pal 3 */
};

static unsigned char px = 120, py = 180;
static int base = 0;

static void update(void)
{
    unsigned char pad = pad_read();

    if (pad & PAD_LEFT)  { if (px >= PLAYER_SPEED) px -= PLAYER_SPEED; else px = 0; }
    if (pad & PAD_RIGHT) { if (px <= 240 - PLAYER_SPEED) px += PLAYER_SPEED; else px = 240; }
    if (pad & PAD_UP)    { if (py >= PLAYER_SPEED) py -= PLAYER_SPEED; else py = 0; }
    if (pad & PAD_DOWN)  { if (py <= 222 - PLAYER_SPEED) py += PLAYER_SPEED; else py = 222; }

    base += CAR_SPEED;
    if (base >= ROAD_LEN) base -= ROAD_LEN;
}

static void draw(void)
{
    unsigned char idx = 0;
    unsigned char i;
    int p, x;

    /* player: 4 sprites (2 per scanline), right column is flipped */
    OAM_Y(0) = py;     OAM_TILE(0) = 0x10; OAM_ATTR(0) = 2;    OAM_X(0) = px;
    OAM_Y(1) = py + 8; OAM_TILE(1) = 0x11; OAM_ATTR(1) = 2;    OAM_X(1) = px;
    OAM_Y(2) = py;     OAM_TILE(2) = 0x10; OAM_ATTR(2) = 0x42; OAM_X(2) = px + 8;
    OAM_Y(3) = py + 8; OAM_TILE(3) = 0x11; OAM_ATTR(3) = 0x42; OAM_X(3) = px + 8;
    idx = 4;

    /* cars: 2 sprites each, only the halves that are on screen */
    for (i = 0; i < NUM_CARS; ++i) {
        p = base + (int)i * CAR_SPACING;
        if (p >= ROAD_LEN) p -= ROAD_LEN;
        x = p - 16;                      /* left edge, -16 .. 943 */

        if (x >= 0 && x <= 248) {
            OAM_Y(idx) = CAR_Y; OAM_TILE(idx) = 0x01;
            OAM_ATTR(idx) = i & 1; OAM_X(idx) = (unsigned char)x;
            ++idx;
        }
        x += 8;
        if (x >= 0 && x <= 248) {
            OAM_Y(idx) = CAR_Y; OAM_TILE(idx) = 0x01;
            OAM_ATTR(idx) = i & 1; OAM_X(idx) = (unsigned char)x;
            ++idx;
        }
    }

    /* hide every unused entry (at most 4 + 6 = 10 are used) */
    while (idx < 64) {
        OAM_Y(idx) = 0xFF;
        ++idx;
    }
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
