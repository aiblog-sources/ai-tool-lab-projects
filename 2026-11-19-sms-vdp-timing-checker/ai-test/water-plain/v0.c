#include "SMSlib.h"

/* ---------- palettes ---------- */
const unsigned char bg_palette[16] = {
    RGB(0,0,2), RGB(0,1,3), RGB(1,2,3), RGB(3,3,3),
    0,0,0,0, 0,0,0,0, 0,0,0,0
};

const unsigned char spr_palette[16] = {
    RGB(0,0,0), RGB(2,1,0), RGB(3,3,3), RGB(3,0,0),
    0,0,0,0, 0,0,0,0, 0,0,0,0
};

/* ---------- water tile: 4 animation frames (32 bytes each) ---------- */
/* each row = 4 bitplane bytes; color 1 = plane0, color 2 = plane1 */
#define ROW(p0,p1) (p0),(p1),0,0
#define WAVE(S,T) \
    ROW(0,0), ROW(0,0), ROW(0,S), ROW(S,0), \
    ROW(0,0), ROW(0,0), ROW(0,T), ROW(T,0)

const unsigned char water_frames[4][32] = {
    { WAVE(0xC0,0x0C) },
    { WAVE(0x30,0x03) },
    { WAVE(0x0C,0xC0) },
    { WAVE(0x03,0x30) }
};

/* ---------- boat sprite: 4 tiles (TL, TR, BL, BR) ---------- */
const unsigned char boat_tiles[128] = {
    /* top-left: mast (brown) + white sail */
    0x01,0x00,0,0,
    0x01,0x02,0,0,
    0x01,0x06,0,0,
    0x01,0x0E,0,0,
    0x01,0x1E,0,0,
    0x01,0x3E,0,0,
    0x01,0x7E,0,0,
    0x01,0x7E,0,0,
    /* top-right */
    0x80,0x00,0,0,
    0x80,0x40,0,0,
    0x80,0x60,0,0,
    0x80,0x70,0,0,
    0x80,0x78,0,0,
    0x80,0x7C,0,0,
    0x80,0x7E,0,0,
    0x80,0x7E,0,0,
    /* bottom-left: mast end + hull */
    0x01,0x00,0,0,
    0xFF,0xFF,0,0,   /* red stripe */
    0xFF,0x00,0,0,
    0xFF,0x00,0,0,
    0x7F,0x00,0,0,
    0x3F,0x00,0,0,
    0x00,0x00,0,0,
    0x00,0x00,0,0,
    /* bottom-right */
    0x80,0x00,0,0,
    0xFF,0xFF,0,0,
    0xFF,0x00,0,0,
    0xFF,0x00,0,0,
    0xFE,0x00,0,0,
    0xFC,0x00,0,0,
    0x00,0x00,0,0,
    0x00,0x00,0,0
};

const signed char bob_table[16] = {
    0, 1, 1, 2, 2, 2, 1, 1, 0, -1, -1, -2, -2, -2, -1, -1
};

void main(void)
{
    unsigned int i;
    unsigned char frame_count = 0;
    unsigned char water_frame = 0;
    unsigned char bob_index = 0;
    unsigned char bob_timer = 0;
    unsigned char boat_y;

    SMS_displayOff();

    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    /* water tile = BG tile 0; boat = sprite tiles 256..259 */
    SMS_loadTiles(water_frames[0], 0, 32);
    SMS_loadTiles(boat_tiles, 256, 128);

    /* fill the whole 32x24 screen with tile 0 (address auto-increments) */
    SMS_setNextTileatXY(0, 0);
    for (i = 0; i < 32u * 24u; i++) {
        SMS_setTile(0);
    }

    SMS_initSprites();
    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();

        /* VRAM writes right after vblank starts */
        SMS_copySpritestoSAT();

        frame_count++;
        if ((frame_count & 7) == 0) {
            water_frame = (water_frame + 1) & 3;
            SMS_loadTiles(water_frames[water_frame], 0, 32);
        }

        bob_timer++;
        if (bob_timer >= 6) {
            bob_timer = 0;
            bob_index = (bob_index + 1) & 15;
        }

        boat_y = (unsigned char)(88 + bob_table[bob_index]);

        SMS_initSprites();
        SMS_addTwoAdjoiningSprites(120, boat_y, 0);       /* tiles 0,1 */
        SMS_addTwoAdjoiningSprites(120, boat_y + 8, 2);   /* tiles 2,3 */
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
