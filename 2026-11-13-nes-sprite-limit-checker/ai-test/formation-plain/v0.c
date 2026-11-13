/* main.c - NES shoot-em-up scene for cc65
 *
 * Two rows of 10 enemy ships (16x16) marching left/right as a formation,
 * a 16x16 player ship moved with the d-pad, A fires 8x8 bullets (max 6).
 *
 * Hardware limit: the PPU draws at most 8 sprites per scanline. A 16x16
 * ship needs two 8-pixel-wide sprites on every line, so a full row of 10
 * ships would need 20 per line. Also, 20 ships x 4 sprites = 80 > 64 OAM
 * entries. So the enemies are drawn in 3 interleaved groups (ship index
 * mod 3), one group per frame. That is at most 4 ships = 8 sprites per row
 * per line, and 32 enemy sprites per frame. The result is a 20 Hz flicker.
 * Bullets and the player are written first in OAM so they have priority if
 * a line overflows.
 */
#include "nes.h"

#define NUM_BULLETS 6
#define COLS        10
#define SPACING     20
#define FORM_MIN    8
#define FORM_MAX    52          /* 52 + 9*20 + 16 = 248 */
#define ROW0_Y      40
#define ROW1_Y      64
#define PLAYER_Y    208
#define PLAYER_MIN  8
#define PLAYER_MAX  232
#define BULLET_SPEED 4

#define TILE_BULLET 0x07
#define TILE_PLAYER 0x06
#define TILE_ENEMY  0x0B

static const unsigned char pal[32] = {
    /* background */
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    /* sprites: player, enemies, bullets, spare */
    0x0F, 0x21, 0x11, 0x30,  0x0F, 0x16, 0x27, 0x37,
    0x0F, 0x28, 0x18, 0x38,  0x0F, 0x2A, 0x1A, 0x3A
};

static unsigned char bon[NUM_BULLETS];
static unsigned char bx[NUM_BULLETS];
static unsigned char by[NUM_BULLETS];

static unsigned char player_x;
static unsigned char form_x;
static unsigned char form_dir;      /* 0 = right, 1 = left */
static unsigned char frame;
static unsigned char pad, prev_pad;
static unsigned char idx;           /* next free OAM entry */

static void put(unsigned char x, unsigned char y,
                unsigned char tile, unsigned char attr)
{
    unsigned char *o = OAM + ((unsigned int)idx << 2);
    o[0] = y - 1;                   /* OAM Y is top line - 1 */
    o[1] = tile;
    o[2] = attr;
    o[3] = x;
    ++idx;
}

/* 16x16 object from four mirrored 8x8 quadrants */
static void put_big(unsigned char x, unsigned char y,
                    unsigned char tile, unsigned char pal_no)
{
    put(x,     y,     tile, pal_no);
    put(x + 8, y,     tile, pal_no | 0x40);
    put(x,     y + 8, tile, pal_no | 0x80);
    put(x + 8, y + 8, tile, pal_no | 0xC0);
}

static void fire(void)
{
    unsigned char i;
    for (i = 0; i < NUM_BULLETS; ++i) {
        if (!bon[i]) {
            bon[i] = 1;
            bx[i] = player_x + 4;
            by[i] = PLAYER_Y - 8;
            return;
        }
    }
}

static void update(void)
{
    unsigned char i;

    prev_pad = pad;
    pad = pad_read();

    /* player */
    if ((pad & PAD_LEFT) && player_x > PLAYER_MIN)   player_x -= 2;
    if ((pad & PAD_RIGHT) && player_x < PLAYER_MAX)  player_x += 2;
    if ((pad & PAD_A) && !(prev_pad & PAD_A))        fire();

    /* bullets */
    for (i = 0; i < NUM_BULLETS; ++i) {
        if (bon[i]) {
            if (by[i] < BULLET_SPEED + 8) bon[i] = 0;
            else by[i] -= BULLET_SPEED;
        }
    }

    /* formation: 1 pixel per frame, reverse at the edges */
    if (form_dir == 0) {
        if (form_x >= FORM_MAX) form_dir = 1; else ++form_x;
    } else {
        if (form_x <= FORM_MIN) form_dir = 0; else --form_x;
    }
}

static void draw(void)
{
    unsigned char i, phase;

    idx = 0;

    /* bullets first: highest priority on a crowded scanline */
    for (i = 0; i < NUM_BULLETS; ++i) {
        if (bon[i]) put(bx[i], by[i], TILE_BULLET, 2);
    }

    /* player */
    put_big(player_x, PLAYER_Y, TILE_PLAYER, 0);

    /* enemies: one of three interleaved groups per frame */
    phase = frame % 3;
    for (i = phase; i < COLS; i += 3) {
        unsigned char x = form_x + i * SPACING;
        put_big(x, ROW0_Y, TILE_ENEMY, 1);
        put_big(x, ROW1_Y, TILE_ENEMY, 1);
    }

    /* hide the rest */
    while (idx < 64) {
        OAM_Y(idx) = 0xFF;
        ++idx;
    }
}

void main(void)
{
    unsigned char i;

    ppu_setup(pal);
    oam_hide_all();

    for (i = 0; i < NUM_BULLETS; ++i) bon[i] = 0;
    player_x = 120;
    form_x   = FORM_MIN;
    form_dir = 0;
    frame    = 0;
    pad      = 0;
    prev_pad = 0;

    while (1) {
        update();
        draw();
        oam_ready();
        wait_frame();
        ++frame;
    }
}
