/* Boss fight demo for cc65 / NES
 * - 48x48 boss built from 36 8x8 sprites, swaying near the top
 * - 8 fireballs orbiting it on a circle of radius 36
 * - 16x16 player (4 sprites) moved with the d-pad along the bottom
 *
 * Sprite-per-scanline budget (hardware limit is 8):
 * The boss uses 6 sprites on each of its lines. The orbit radius (36) keeps
 * the fireballs outside the boss except around the left/right sides, where at
 * most one fireball per side overlaps the boss rows. That gives at most
 * 6 + 2 = 8 on any line. The player is far below the boss, so a line never
 * mixes the two groups.
 *
 * Speed-up: the constant OAM fields (boss/player Y, tiles, attributes, fireball
 * tile/attr) are written once at start; each frame only the changing bytes
 * are updated, using a full 64-entry sine table and 8-bit math.
 */
#include "nes.h"

/* full sine table: round(36 * sin(i * 360/64)), i = 0..63 */
static const signed char sintab[64] = {
      0,   4,   7,  10,  14,  17,  20,  23,  25,  28,  30,  32,  33,  34,  35,  36,
     36,  36,  35,  34,  33,  32,  30,  28,  25,  23,  20,  17,  14,  10,   7,   4,
      0,  -4,  -7, -10, -14, -17, -20, -23, -25, -28, -30, -32, -33, -34, -35, -36,
    -36, -36, -35, -34, -33, -32, -30, -28, -25, -23, -20, -17, -14, -10,  -7,  -4
};

/* 32-byte palette: 16 background, 16 sprite */
static const unsigned char palette[32] = {
    /* background */
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    /* sprites: 0 boss, 1 player, 2 fireballs, 3 spare */
    0x0F, 0x16, 0x27, 0x30,
    0x0F, 0x11, 0x2A, 0x30,
    0x0F, 0x27, 0x37, 0x30,
    0x0F, 0x16, 0x27, 0x30
};

#define BOSS_CY   56
#define BOSS_HALF 24
#define PLAYER_Y  200
#define PLAYER_MIN_X 8
#define PLAYER_MAX_X 232

static unsigned char frame;
static unsigned char player_x;
static unsigned char pad;

static void init_oam(void)
{
    unsigned char r, c, i = 0, tile;
    unsigned char *p = OAM;

    for (r = 0; r < 6; ++r) {
        for (c = 0; c < 6; ++c) {
            tile = 0x01;
            if (r == 2 && (c == 1 || c == 4)) tile = 0x03;
            else if (r == 4 && (c == 2 || c == 3)) tile = 0x02;
            p[0] = (unsigned char)(BOSS_CY - BOSS_HALF + (r << 3) - 1);
            p[1] = tile;
            p[2] = 0;
            p += 4;
        }
    }
    for (i = 0; i < 8; ++i) {
        p[1] = 0x04;
        p[2] = 2;
        p += 4;
    }
    /* player */
    p[0] = PLAYER_Y - 1;     p[1] = 0x06; p[2] = 1 | 0x00;
    p[4] = PLAYER_Y - 1;     p[5] = 0x06; p[6] = 1 | 0x40;
    p[8] = PLAYER_Y + 7;     p[9] = 0x06; p[10] = 1 | 0x80;
    p[12] = PLAYER_Y + 7;    p[13] = 0x06; p[14] = 1 | 0xC0;
}

static void update(void)
{
    ++frame;
    pad = pad_read();
    if ((pad & PAD_LEFT) && player_x > PLAYER_MIN_X)  player_x -= 2;
    if ((pad & PAD_RIGHT) && player_x < PLAYER_MAX_X) player_x += 2;
}

static void draw(void)
{
    unsigned char r, c, k, a, x, cx;
    unsigned char *p;

    cx = (unsigned char)(128 + sintab[frame >> 2] * 2);

    /* boss X positions */
    p = OAM + 3;
    for (r = 0; r < 6; ++r) {
        x = cx - BOSS_HALF;
        for (c = 0; c < 6; ++c) {
            *p = x;
            p += 4;
            x += 8;
        }
    }

    /* fireballs (p now points at OAM_Y... of sprite 36 offset +3) */
    a = frame >> 1;
    for (k = 0; k < 8; ++k) {
        p[-3] = (unsigned char)(BOSS_CY - 5 + sintab[a & 63]);
        *p    = (unsigned char)(cx - 4 + sintab[(a + 16) & 63]);
        p += 4;
        a += 8;
    }

    /* player X */
    x = player_x;
    p[0] = x;
    p[4] = x + 8;
    p[8] = x;
    p[12] = x + 8;
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    frame = 0;
    player_x = 120;
    init_oam();

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
