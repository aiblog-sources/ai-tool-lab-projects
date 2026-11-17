```c
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
 * Speed: the constant OAM fields (Y of the boss/player, tiles, attributes)
 * are written once at startup. Each frame only the changing bytes are
 * written, using 8-bit math and a precomputed sine table.
 */
#include "nes.h"

/* quarter-wave sine table: round(36 * sin(i * 360/64)), i = 0..16 */
static const signed char qsin[17] = {
    0, 4, 7, 10, 14, 17, 20, 23, 25, 28, 30, 32, 33, 34, 35, 36, 36
};

/* full table, built once at startup (values mod 256) */
static unsigned char stab[64];

static void build_table(void)
{
    unsigned char i;
    signed char v;
    for (i = 0; i < 64; ++i) {
        if (i <= 16)      v = qsin[i];
        else if (i <= 32) v = qsin[32 - i];
        else if (i <= 48) v = -qsin[i - 32];
        else              v = -qsin[64 - i];
        stab[i] = (unsigned char)v;
    }
}

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
    unsigned char i = 0;
    unsigned char r, c, tile;

    for (r = 0; r < 6; ++r) {
        for (c = 0; c < 6; ++c) {
            tile = 0x01;
            if (r == 2 && (c == 1 || c == 4)) tile = 0x03;
            else if (r == 4 && (c == 2 || c == 3)) tile = 0x02;
            OAM_Y(i)    = (unsigned char)(BOSS_CY - BOSS_HALF + (r << 3) - 1);
            OAM_TILE(i) = tile;
            OAM_ATTR(i) = 0;
            ++i;
        }
    }
    for (c = 0; c < 8; ++c) {
        OAM_TILE(i) = 0x04;
        OAM_ATTR(i) = 2;
        ++i;
    }
    /* player */
    OAM_Y(44) = PLAYER_Y - 1;  OAM_TILE(44) = 0x06; OAM_ATTR(44) = 1;
    OAM_Y(45) = PLAYER_Y - 1;  OAM_TILE(45) = 0x06; OAM_ATTR(45) = 1 | 0x40;
    OAM_Y(46) = PLAYER_Y + 7;  OAM_TILE(46) = 0x06; OAM_ATTR(46) = 1 | 0x80;
    OAM_Y(47) = PLAYER_Y + 7;  OAM_TILE(47) = 0x06; OAM_ATTR(47) = 1 | 0xC0;
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
    unsigned char *p;
    unsigned char r, k, ang, a, cx, bx, fx;

    /* boss sway: +-72 pixels around the screen centre */
    cx = (unsigned char)(128 + (stab[(frame >> 2) & 63] << 1));

    /* boss body: only X changes */
    bx = (unsigned char)(cx - BOSS_HALF);
    p = &OAM[3];
    for (r = 0; r < 6; ++r) {
        p[0]  = bx;
        p[4]  = (unsigned char)(bx + 8);
        p[8]  = (unsigned char)(bx + 16);
        p[12] = (unsigned char)(bx + 24);
        p[16] = (unsigned char)(bx + 32);
        p[20] = (unsigned char)(bx + 40);
        p += 24;
    }

    /* 8 fireballs (sprites 36..43), p now points at OAM[3 + 144] */
    ang = frame >> 1;
    fx = (unsigned char)(cx - 4);
    p = &OAM[36 * 4];
    for (k = 0; k < 8; ++k) {
        a = (unsigned char)(ang + (k << 3));
        p[0] = (unsigned char)(BOSS_CY - 4 + stab[a & 63]);
        p[3] = (unsigned char)(fx + stab[(a + 16) & 63]);
        p += 4;
    }

    /* player X */
    OAM_X(44) = player_x;
    OAM_X(45) = (unsigned char)(player_x + 8);
    OAM_X(46) = player_x;
    OAM_X(47) = (unsigned char)(player_x + 8);
}

void main(void)
{
    ppu_setup(palette);
    oam_hide_all();
    build_table();
    init_oam();

    frame = 0;
    player_x = 120;

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
```