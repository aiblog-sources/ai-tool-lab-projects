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
 */
#include "nes.h"

/* quarter-wave sine table: round(36 * sin(i * 360/64)), i = 0..16 */
static const signed char qsin[17] = {
    0, 4, 7, 10, 14, 17, 20, 23, 25, 28, 30, 32, 33, 34, 35, 36, 36
};

static signed char sin64(unsigned char i)
{
    i &= 63;
    if (i <= 16) return qsin[i];
    if (i <= 32) return qsin[32 - i];
    if (i <= 48) return -qsin[i - 32];
    return -qsin[64 - i];
}

#define cos64(i) sin64((unsigned char)((i) + 16))

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
#define ORBIT_R   36
#define PLAYER_Y  200
#define PLAYER_MIN_X 8
#define PLAYER_MAX_X 232

static unsigned char frame;
static unsigned char player_x;
static unsigned char pad;

static void put_sprite(unsigned char i, int x, int y,
                       unsigned char tile, unsigned char attr)
{
    OAM_Y(i)    = (unsigned char)(y - 1);
    OAM_TILE(i) = tile;
    OAM_ATTR(i) = attr;
    OAM_X(i)    = (unsigned char)x;
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
    unsigned char i = 0;
    unsigned char r, c, k, ang, tile;
    int cx, bx, by;

    /* boss sway: +-72 pixels around the screen centre */
    cx = 128 + (int)sin64((unsigned char)(frame >> 2)) * 2;

    /* boss body: 6 x 6 sprites */
    bx = cx - BOSS_HALF;
    by = BOSS_CY - BOSS_HALF;
    for (r = 0; r < 6; ++r) {
        for (c = 0; c < 6; ++c) {
            tile = 0x01;
            if (r == 2 && (c == 1 || c == 4)) tile = 0x03;      /* eyes */
            else if (r == 4 && (c == 2 || c == 3)) tile = 0x02; /* mouth */
            put_sprite(i++, bx + (c << 3), by + (r << 3), tile, 0);
        }
    }

    /* 8 fireballs orbiting the boss centre */
    ang = frame >> 1;
    for (k = 0; k < 8; ++k) {
        unsigned char a = (unsigned char)(ang + (k << 3));
        put_sprite(i++,
                   cx + cos64(a) - 4,
                   BOSS_CY + sin64(a) - 4,
                   0x04, 2);
    }

    /* player: 16x16 from four mirrored ship tiles */
    put_sprite(i++, player_x,     PLAYER_Y,     0x06, 1);
    put_sprite(i++, player_x + 8, PLAYER_Y,     0x06, 1 | 0x40);
    put_sprite(i++, player_x,     PLAYER_Y + 8, 0x06, 1 | 0x80);
    put_sprite(i++, player_x + 8, PLAYER_Y + 8, 0x06, 1 | 0xC0);
    /* remaining OAM entries stay hidden from oam_hide_all() */
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();

    frame = 0;
    player_x = 120;

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
