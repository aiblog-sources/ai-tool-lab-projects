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
 * Speed: tile/attribute/Y bytes of every sprite are constant, so they are
 * written once at startup. Each loop only rewrites the X bytes (boss only
 * when it moved) and the fireball Y bytes, using a precomputed sine table.
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

/* full table built once at startup */
static signed char stab[64];

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
static unsigned char last_cx;

static void init_oam(void)
{
    unsigned char r, c, i, tile;
    unsigned char *p = OAM;

    /* boss: 36 sprites */
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
    /* fireballs: 8 sprites */
    for (i = 0; i < 8; ++i) {
        p[1] = 0x04;
        p[2] = 2;
        p += 4;
    }
    /* player: 4 sprites */
    p[0] = PLAYER_Y - 1;       p[1] = 0x06; p[2] = 1;
    p[4] = PLAYER_Y - 1;       p[5] = 0x06; p[6] = 1 | 0x40;
    p[8] = PLAYER_Y + 8 - 1;   p[9] = 0x06; p[10] = 1 | 0x80;
    p[12] = PLAYER_Y + 8 - 1;  p[13] = 0x06; p[14] = 1 | 0xC0;
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

    /* boss sway: +-72 pixels around the screen centre */
    cx = (unsigned char)(128 + stab[(frame >> 2) & 63] * 2);

    if (cx != last_cx) {
        last_cx = cx;
        p = OAM + 3;
        for (r = 0; r < 6; ++r) {
            x = cx - BOSS_HALF;
            for (c = 0; c < 6; ++c) {
                *p = x;
                p += 4;
                x += 8;
            }
        }
    }

    /* 8 fireballs orbiting the boss centre (OAM entries 36..43) */
    a = frame >> 1;
    p = OAM + 36 * 4;
    for (k = 0; k < 8; ++k) {
        p[0] = (unsigned char)(BOSS_CY - 5 + stab[a & 63]);
        p[3] = (unsigned char)(cx - 4 + stab[(a + 16) & 63]);
        p += 4;
        a += 8;
    }

    /* player (OAM entries 44..47) */
    p = OAM + 44 * 4 + 3;
    p[0] = player_x;
    p[4] = player_x + 8;
    p[8] = player_x;
    p[12] = player_x + 8;
}

void main(void)
{
    unsigned char i;

    ppu_setup(palette);
    oam_hide_all();

    for (i = 0; i < 64; ++i) stab[i] = sin64(i);

    frame = 0;
    player_x = 120;
    last_cx = 0;
    init_oam();

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
