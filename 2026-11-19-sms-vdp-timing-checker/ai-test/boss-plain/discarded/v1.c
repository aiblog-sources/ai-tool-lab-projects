/*
 * main.c - Sega Master System boss demo (devkitSMS / SMSlib, SDCC)
 *
 * - 48x32 px boss (6x4 = 24 background tiles) in the middle of the screen.
 * - Its 4 animation frames (4 x 24 tiles) are all uploaded to VRAM once at
 *   start-up (display off). Every frame (60 fps on NTSC) the 24-entry tile
 *   map is repointed to the current frame's tiles in VBlank (48 bytes).
 * - 16x16 player sprite (4 hardware sprites) moves left/right below it.
 *
 * Build:
 *   sdcc -c -mz80 main.c
 *   sdcc -o main.ihx -mz80 --no-std-crt0 --data-loc 0xC000 crt0_sms.rel main.rel SMSlib.lib
 *   makesms main.ihx main.sms
 */

#include "SMSlib.h"

/* ------------------------------------------------------------------ */
/* Palettes                                                            */
/* ------------------------------------------------------------------ */

/* Background palette: 0 backdrop, 1 body, 2 eyes/mouth, 3 moving highlight */
static const unsigned char bg_palette[16] = {
    RGB(0,0,1), RGB(2,0,1), RGB(3,3,0), RGB(3,2,2),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(3,3,3)
};

/* Sprite palette: 0 transparent, 1 hull, 2 cockpit */
static const unsigned char spr_palette[16] = {
    RGB(0,0,0), RGB(0,2,3), RGB(3,3,3), RGB(3,3,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(3,3,3)
};

/* ------------------------------------------------------------------ */
/* Boss art: left half (24 px) of each of 32 rows, mirrored at load.   */
/* Bit 7 of byte 0 is the leftmost pixel.                              */
/* ------------------------------------------------------------------ */

static const unsigned char boss_body[32][3] = {
    {0xC0,0x00,0x0F}, {0xE0,0x03,0xFF}, {0x70,0x0F,0xFF}, {0x3C,0x3F,0xFF},
    {0x1F,0xFF,0xFF}, {0x0F,0xFF,0xFF}, {0x1F,0xFF,0xFF}, {0x3F,0xFF,0xFF},
    {0x7F,0xFF,0xFF}, {0x7F,0xFF,0xFF}, {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF},
    {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF},
    {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF},
    {0xFF,0xFF,0xFF}, {0xFF,0xFF,0xFF}, {0x7F,0xFF,0xFF}, {0x7F,0xFF,0xFF},
    {0x3F,0xFF,0xFF}, {0x3F,0xFF,0xFF}, {0x1F,0xFF,0xFF}, {0x1F,0xE7,0xFF},
    {0x0F,0xC3,0xFF}, {0x0F,0x81,0xFF}, {0x06,0x00,0xE7}, {0x04,0x00,0x42}
};

/* Eyes (rows 9-12) and a toothy mouth (rows 20-23) */
static const unsigned char boss_detail[32][3] = {
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0x03,0xF0,0x00}, {0x03,0xF0,0x00}, {0x03,0xF0,0x00},
    {0x03,0xF0,0x00}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0x00,0x03,0xFF}, {0x00,0x02,0xAA}, {0x00,0x03,0xFF}, {0x00,0x01,0x55},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0},
    {0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}
};

/* ------------------------------------------------------------------ */
/* Player art: 16 rows x 16 px masks                                   */
/* ------------------------------------------------------------------ */

static const unsigned int player_body[16] = {
    0x0180, 0x0180, 0x03C0, 0x03C0, 0x07E0, 0x07E0, 0x0FF0, 0x1FF8,
    0x3FFC, 0x7FFE, 0x7FFE, 0xFFFF, 0xFFFF, 0xF99F, 0xE007, 0xC003
};

static const unsigned int player_cockpit[16] = {
    0, 0, 0, 0, 0x03C0, 0x0180, 0x0180, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------ */
/* RAM buffers                                                         */
/* ------------------------------------------------------------------ */

#define BOSS_TILES      24
#define BOSS_FIRST_TILE 1          /* tile 0 stays blank for the background */
#define BOSS_X          13         /* (32 - 6) / 2 */
#define BOSS_Y          10         /* (24 - 4) / 2 */

/* 4 x 768 bytes, contiguous: all frames are loaded to VRAM once */
static unsigned char boss_frames[4][BOSS_TILES * 32];
static unsigned char player_tiles[4 * 32];
/* One 6x4 tile map (16-bit entries) per animation frame */
static unsigned int boss_maps[4][BOSS_TILES];

/* Write one pixel (color 0..3) into a 4bpp tile buffer. */
static void put_pixel(unsigned char *tile, unsigned char px, unsigned char py,
                      unsigned char color)
{
    unsigned char *row = tile + (py << 2);
    unsigned char bit = (unsigned char)(0x80 >> px);
    if (color & 1) row[0] |= bit;
    if (color & 2) row[1] |= bit;
}

static unsigned char boss_bit(const unsigned char m[][3], unsigned char y,
                              unsigned char x)
{
    if (x >= 24) x = 47 - x;
    return (m[y][x >> 3] >> (7 - (x & 7))) & 1;
}

static void build_boss_frames(void)
{
    unsigned char f, x, y, c;
    unsigned char *tile;

    for (f = 0; f < 4; f++) {
        for (y = 0; y < 32; y++) {
            for (x = 0; x < 48; x++) {
                c = 0;
                if (boss_bit(boss_body, y, x)) {
                    c = 1;
                    if (((y + f) & 3) == 0) c = 3;       /* moving highlight */
                    if (boss_bit(boss_detail, y, x)) {
                        c = 2;
                        if (f == 2) c = 1;               /* blink on frame 2 */
                    }
                }
                if (c) {
                    tile = &boss_frames[f][((y >> 3) * 6 + (x >> 3)) * 32];
                    put_pixel(tile, x & 7, y & 7, c);
                }
            }
        }
    }
}

static void build_player_tiles(void)
{
    unsigned char x, y, c;
    unsigned char *tile;

    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            unsigned int m = 0x8000u >> x;
            c = 0;
            if (player_body[y] & m) c = 1;
            if (player_cockpit[y] & m) c = 2;
            if (c) {
                tile = &player_tiles[((y >> 3) * 2 + (x >> 3)) * 32];
                put_pixel(tile, x & 7, y & 7, c);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

void main(void)
{
    unsigned char frame = 0;
    unsigned char player_x = 120;
    const unsigned char player_y = 160;
    unsigned char i, f;
    unsigned int keys;

    SMS_displayOff();
    SMS_init();

    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);
    SMS_setBackdropColor(0);

    build_boss_frames();
    build_player_tiles();

    /* Tile map for each animation frame: 24 consecutive tiles per frame */
    for (f = 0; f < 4; f++)
        for (i = 0; i < BOSS_TILES; i++)
            boss_maps[f][i] = BOSS_FIRST_TILE + f * BOSS_TILES + i;

    /* All four frames (3072 bytes) uploaded once, display is off */
    SMS_loadTiles(boss_frames, BOSS_FIRST_TILE, sizeof(boss_frames));
    SMS_loadTileMapArea(BOSS_X, BOSS_Y, boss_maps[0], 6, 4);

    /* Sprite tiles live in the second half of VRAM tiles (index 256+) */
    SMS_loadTiles(player_tiles, 256, sizeof(player_tiles));

    SMS_initSprites();
    SMS_addTwoAdjoiningSprites(player_x, player_y, 0);
    SMS_addTwoAdjoiningSprites(player_x, player_y + 8, 2);
    SMS_copySpritestoSAT();

    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();

        /* VBlank work: sprite table, then repoint the boss map (48 bytes) */
        SMS_copySpritestoSAT();
        SMS_loadTileMapArea(BOSS_X, BOSS_Y, boss_maps[frame], 6, 4);
        frame = (frame + 1) & 3;

        /* Game logic */
        keys = SMS_getKeysStatus();
        if ((keys & PORT_A_KEY_LEFT) && player_x > 8)
            player_x -= 2;
        if ((keys & PORT_A_KEY_RIGHT) && player_x < 232)
            player_x += 2;

        SMS_initSprites();
        SMS_addTwoAdjoiningSprites(player_x, player_y, 0);      /* top row    */
        SMS_addTwoAdjoiningSprites(player_x, player_y + 8, 2);  /* bottom row */
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
