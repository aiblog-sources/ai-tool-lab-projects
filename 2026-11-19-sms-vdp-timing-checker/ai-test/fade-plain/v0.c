#include "SMSlib.h"

/* ---------- tile data (4bpp planar: 4 bytes per row, 8 rows per tile) ---------- */

#define SB(c) ((c) & 1 ? 0xFF : 0x00), ((c) & 2 ? 0xFF : 0x00), \
              ((c) & 4 ? 0xFF : 0x00), ((c) & 8 ? 0xFF : 0x00)
#define SOLID(c) SB(c), SB(c), SB(c), SB(c), SB(c), SB(c), SB(c), SB(c)

/* 8 solid tiles, tile n = palette color n */
const unsigned char bg_tiles[8 * 32] = {
    SOLID(0), SOLID(1), SOLID(2), SOLID(3),
    SOLID(4), SOLID(5), SOLID(6), SOLID(7)
};

#define ROW(a) (a), 0x00, 0x00, 0x00   /* color 1 */
#define ROW2(a) 0x00, (a), 0x00, 0x00  /* color 2 */
#define ROW3(a) (a), (a), 0x00, 0x00   /* color 3 */

/* 4 star frames: blank, dot, plus, big star */
const unsigned char star_tiles[4 * 32] = {
    /* 0: blank */
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    /* 1: dot */
    ROW(0x00), ROW(0x00), ROW(0x00), ROW(0x18),
    ROW(0x18), ROW(0x00), ROW(0x00), ROW(0x00),
    /* 2: plus */
    ROW2(0x00), ROW2(0x10), ROW2(0x10), ROW2(0x7C),
    ROW2(0x10), ROW2(0x10), ROW2(0x00), ROW2(0x00),
    /* 3: big star */
    ROW3(0x10), ROW3(0x10), ROW3(0x54), ROW3(0x38),
    ROW3(0xFE), ROW3(0x38), ROW3(0x54), ROW3(0x10)
};

/* ---------- palettes ---------- */

const unsigned char bg_palette_full[8] = {
    RGB(0, 0, 0),  /* 0 black   */
    RGB(3, 0, 0),  /* 1 red     */
    RGB(3, 2, 0),  /* 2 orange  */
    RGB(3, 3, 0),  /* 3 yellow  */
    RGB(0, 3, 0),  /* 4 green   */
    RGB(0, 3, 3),  /* 5 cyan    */
    RGB(0, 0, 3),  /* 6 blue    */
    RGB(3, 0, 3)   /* 7 magenta */
};

const unsigned char sprite_palette[16] = {
    RGB(0, 0, 0), RGB(3, 3, 3), RGB(3, 3, 0), RGB(0, 3, 3),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const unsigned char twinkle_seq[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };

#define NUM_STARS   16
#define MAX_LEVEL   4
#define FADE_FRAMES 4

unsigned char pal_buf[16];
signed char star_handle[NUM_STARS];

static void build_palette(unsigned char level)
{
    unsigned char i, c, r, g, b;
    for (i = 0; i < 16; i++) {
        if (i < 8) {
            c = bg_palette_full[i];
            r = (c & 3) * level / MAX_LEVEL;
            g = ((c >> 2) & 3) * level / MAX_LEVEL;
            b = ((c >> 4) & 3) * level / MAX_LEVEL;
            pal_buf[i] = RGB(r, g, b);
        } else {
            pal_buf[i] = 0;
        }
    }
}

static void draw_picture(void)
{
    unsigned char x, y, dx, dy, d;
    for (y = 0; y < 24; y++) {
        SMS_setNextTileatXY(0, y);
        dy = (y > 12 ? y - 12 : 12 - y);
        dy = dy + (dy >> 1);
        for (x = 0; x < 32; x++) {
            dx = (x > 16 ? x - 16 : 16 - x);
            d = (dx > dy) ? dx : dy;
            SMS_setTile((d >> 1) & 7);
        }
    }
}

void main(void)
{
    unsigned char i, level, frame_cnt, dir, phase;
    unsigned int frame;

    SMS_displayOff();
    SMS_waitForVBlank();

    SMS_loadTiles(bg_tiles, 0, sizeof(bg_tiles));
    SMS_loadTiles(star_tiles, 256, sizeof(star_tiles));

    draw_picture();

    level = 0;
    dir = 1;
    frame_cnt = 0;
    frame = 0;

    build_palette(0);
    SMS_loadBGPalette(pal_buf);
    SMS_loadSpritePalette(sprite_palette);
    SMS_setBackdropColor(0);

    SMS_initSprites();
    for (i = 0; i < NUM_STARS; i++) {
        unsigned char sx = 12 + (unsigned char)((i * 61) % 232);
        unsigned char sy = 8 + (unsigned char)((i * 37) % 168);
        star_handle[i] = SMS_addSprite(sx, sy, 0);
    }
    SMS_copySpritestoSAT();

    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();
        SMS_copySpritestoSAT();

        /* palette fade: one brightness step every FADE_FRAMES frames */
        frame_cnt++;
        if (frame_cnt >= FADE_FRAMES) {
            frame_cnt = 0;
            if (dir) {
                if (level < MAX_LEVEL) level++;
                if (level >= MAX_LEVEL) dir = 0;
            } else {
                if (level > 0) level--;
                if (level == 0) dir = 1;
            }
            build_palette(level);
            SMS_loadBGPalette(pal_buf);
        }

        /* star twinkle: change tile every 8 frames, per-star phase offset */
        frame++;
        if ((frame & 7) == 0) {
            for (i = 0; i < NUM_STARS; i++) {
                if (star_handle[i] >= 0) {
                    phase = (unsigned char)((frame >> 3) + i * 3) & 7;
                    SMS_updateSpriteImage(star_handle[i], twinkle_seq[phase]);
                }
            }
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
