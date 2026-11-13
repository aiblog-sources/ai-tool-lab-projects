/* Beat-em-up scene for cc65 / NES (mapper 0).
 *
 * 6 characters of 16x32 on one ground line would need 12 sprites per
 * scanline, but the PPU only draws 8. So each frame we draw the player
 * (always) plus 3 of the 5 enemies, rotating which 2 enemies sit out.
 * That is exactly 8 sprites per scanline, and the enemies flicker evenly.
 *
 * Each character = 2 columns x 2 rows of 8x16 sprites (4 OAM entries).
 */
#include "nes.h"

#define GROUND_Y   144   /* top of the characters on the shared ground line */
#define PLAYER_MIN_Y 128
#define PLAYER_MAX_Y 160
#define X_MAX      240   /* 256 - 16 */
#define NUM_ENEMIES 5

static const unsigned char palette[32] = {
    /* background */
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
    /* sprites: player (cyan/white), red, green, orange */
    0x0F, 0x30, 0x21, 0x11,  0x0F, 0x16, 0x26, 0x06,
    0x0F, 0x1A, 0x2A, 0x0A,  0x0F, 0x17, 0x27, 0x07
};

/* enemy state: x in 8.8 fixed point, speed in 8.8 fixed point per frame */
static unsigned int  ex[NUM_ENEMIES]    = { 0x1000, 0x4000, 0x7800, 0xA000, 0xD000 };
static const unsigned int espeed[NUM_ENEMIES] = { 0x0060, 0x00A0, 0x0100, 0x0170, 0x0220 };
static unsigned char edir[NUM_ENEMIES]  = { 1, 0, 1, 0, 1 };   /* 1 = right */
static const unsigned char epal[NUM_ENEMIES] = { 1, 2, 3, 1, 2 };

static unsigned char px = 120, py = GROUND_Y;
static unsigned char oam_n;      /* next free OAM entry */
static unsigned char frame;      /* 0..4, rotates the hidden enemies */

/* Draw a 16x32 character with top-left at (x, y). Uses 4 OAM entries. */
static void draw_char(unsigned char x, unsigned char y, unsigned char pal)
{
    unsigned char *p = OAM + ((unsigned int)oam_n << 2);
    unsigned char sy = y - 1;          /* OAM Y is one line above */

    /* head row, left / right (right mirrored) */
    p[0] = sy;      p[1] = 0x12; p[2] = pal;        p[3] = x;
    p[4] = sy;      p[5] = 0x12; p[6] = pal | 0x40; p[7] = x + 8;
    /* body row (8x16 sprite is 16 tall, so next row starts 16 lower) */
    p[8]  = sy + 16; p[9]  = 0x10; p[10] = pal;        p[11] = x;
    p[12] = sy + 16; p[13] = 0x10; p[14] = pal | 0x40; p[15] = x + 8;

    oam_n += 4;
}

static void update_player(void)
{
    unsigned char pad = pad_read();

    if (pad & PAD_LEFT)  { if (px >= 2) px -= 2; else px = 0; }
    if (pad & PAD_RIGHT) { if (px <= X_MAX - 2) px += 2; else px = X_MAX; }
    if (pad & PAD_UP)    { if (py > PLAYER_MIN_Y) --py; }
    if (pad & PAD_DOWN)  { if (py < PLAYER_MAX_Y) ++py; }
}

static void update_enemies(void)
{
    unsigned char i;
    for (i = 0; i < NUM_ENEMIES; ++i) {
        if (edir[i]) {
            ex[i] += espeed[i];
            if (ex[i] >= ((unsigned int)X_MAX << 8)) {
                ex[i] = (unsigned int)X_MAX << 8;
                edir[i] = 0;
            }
        } else {
            if (ex[i] < espeed[i]) {
                ex[i] = 0;
                edir[i] = 1;
            } else {
                ex[i] -= espeed[i];
            }
        }
    }
}

static void fill_oam(void)
{
    unsigned char i, h1, h2;

    oam_n = 0;
    draw_char(px, py, 0);              /* player: always drawn, no flicker */

    /* hide two enemies per frame, rotating, so only 4 characters
       (8 sprites) cross any scanline */
    h1 = frame;
    h2 = frame + 1;
    if (h2 >= NUM_ENEMIES) h2 -= NUM_ENEMIES;

    for (i = 0; i < NUM_ENEMIES; ++i) {
        if (i == h1 || i == h2) continue;
        draw_char((unsigned char)(ex[i] >> 8), GROUND_Y, epal[i]);
    }
    /* remaining OAM entries stay hidden from oam_hide_all() */
}

void main(void)
{
    ppu_setup(palette);
    ppu_ctrl |= CTRL_SPR_8X16;         /* 8x16 sprites */
    oam_hide_all();

    for (;;) {
        update_player();
        update_enemies();

        /* entries not rewritten this frame must stay hidden */
        oam_hide_all();
        fill_oam();
        oam_ready();

        if (++frame >= NUM_ENEMIES) frame = 0;
        wait_frame();
    }
}
