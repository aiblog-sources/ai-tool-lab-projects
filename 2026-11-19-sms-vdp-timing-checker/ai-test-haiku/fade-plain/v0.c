/*
 * Palette fade demo for Sega Master System (devkitSMS / SMSlib / SDCC)
 *
 * A picture made of 8-colour background tiles fades from black to full
 * colour and back, one brightness step every 4 frames, forever.
 * 16 star sprites twinkle by cycling through three star tiles.
 *
 * Build: sdcc -c -mz80 main.c
 */

#include <SMSlib.h>

/* ------------------------------------------------------------------------ */
/* Tile data (4bpp planar: 8 rows x 4 bytes = 32 bytes per tile)            */
/* ------------------------------------------------------------------------ */

#define ON   0xFF
#define OFF  0x00

/* One solid 8x8 tile: each row has the same four bitplane bytes */
#define SOLID_TILE(p0,p1,p2,p3) \
    p0,p1,p2,p3, p0,p1,p2,p3, p0,p1,p2,p3, p0,p1,p2,p3, \
    p0,p1,p2,p3, p0,p1,p2,p3, p0,p1,p2,p3, p0,p1,p2,p3

/* Background tiles 0..7: solid tile n uses palette colour n */
static const unsigned char bg_tiles[8 * 32] = {
    SOLID_TILE(OFF,OFF,OFF,OFF),   /* tile 0: colour 0 */
    SOLID_TILE(ON ,OFF,OFF,OFF),   /* tile 1: colour 1 */
    SOLID_TILE(OFF,ON ,OFF,OFF),   /* tile 2: colour 2 */
    SOLID_TILE(ON ,ON ,OFF,OFF),   /* tile 3: colour 3 */
    SOLID_TILE(OFF,OFF,ON ,OFF),   /* tile 4: colour 4 */
    SOLID_TILE(ON ,OFF,ON ,OFF),   /* tile 5: colour 5 */
    SOLID_TILE(OFF,ON ,ON ,OFF),   /* tile 6: colour 6 */
    SOLID_TILE(ON ,ON ,ON ,OFF)    /* tile 7: colour 7 */
};

/* Star sprite tiles, VRAM tiles 16..18. Pixels use colour 1 (plane 0 only). */
#define STAR_BASE  16
#define STAR_ROW(m)  (m),0x00,0x00,0x00

static const unsigned char star_tiles[3 * 32] = {
    /* tile 16: small twinkle */
    STAR_ROW(0x00), STAR_ROW(0x00), STAR_ROW(0x00), STAR_ROW(0x18),
    STAR_ROW(0x18), STAR_ROW(0x00), STAR_ROW(0x00), STAR_ROW(0x00),
    /* tile 17: medium twinkle */
    STAR_ROW(0x00), STAR_ROW(0x18), STAR_ROW(0x18), STAR_ROW(0x7E),
    STAR_ROW(0x7E), STAR_ROW(0x18), STAR_ROW(0x18), STAR_ROW(0x00),
    /* tile 18: big twinkle */
    STAR_ROW(0x00), STAR_ROW(0x18), STAR_ROW(0x3C), STAR_ROW(0xFF),
    STAR_ROW(0xFF), STAR_ROW(0x3C), STAR_ROW(0x18), STAR_ROW(0x00)
};

/* ------------------------------------------------------------------------ */
/* Palettes (full brightness). RGB(r,g,b) takes 0..3 per channel.           */
/* ------------------------------------------------------------------------ */

static const unsigned char bg_pal_full[16] = {
    RGB(0,0,1),  /* 0: navy     */
    RGB(3,0,0),  /* 1: red      */
    RGB(3,2,0),  /* 2: orange   */
    RGB(3,3,0),  /* 3: yellow   */
    RGB(0,3,0),  /* 4: green    */
    RGB(0,2,3),  /* 5: cyan     */
    RGB(1,0,3),  /* 6: purple   */
    RGB(3,3,3),  /* 7: white    */
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
    RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

static const unsigned char spr_pal_full[16] = {
    RGB(0,0,0),  /* 0: transparent */
    RGB(3,3,2),  /* 1: star colour */
    RGB(3,3,3), RGB(3,3,3), RGB(3,3,3), RGB(3,3,3),
    RGB(3,3,3), RGB(3,3,3), RGB(3,3,3), RGB(3,3,3),
    RGB(3,3,3), RGB(3,3,3), RGB(3,3,3), RGB(3,3,3),
    RGB(3,3,3), RGB(3,3,3)
};

/* ------------------------------------------------------------------------ */
/* Picture: 24 rows x 32 columns, each digit is a background tile (0..7)    */
/* 5 = cyan sky, 0 = navy sky, 3 = sun, 1/2 = sunset, 4/6 = hills/ground   */
/* ------------------------------------------------------------------------ */

static const char picture[24][33] = {
    "55555555" "55555555" "55555555" "55555555",  /* 0  */
    "55555555" "55555555" "55555555" "55555555",  /* 1  */
    "00000000" "00003333" "33330000" "00000000",  /* 2  sun top */
    "00000000" "00333333" "33333300" "00000000",  /* 3  */
    "00000000" "00333333" "33333300" "00000000",  /* 4  */
    "00000000" "03333333" "33333330" "00000000",  /* 5  */
    "00000000" "03333333" "33333330" "00000000",  /* 6  */
    "00000000" "00333333" "33333300" "00000000",  /* 7  */
    "00000000" "00333333" "33333300" "00000000",  /* 8  */
    "00000000" "00003333" "33330000" "00000000",  /* 9  sun bottom */
    "11111111" "11111111" "11111111" "11111111",  /* 10 */
    "11111111" "11111111" "11111111" "11111111",  /* 11 */
    "22222222" "22222222" "22222222" "22222222",  /* 12 */
    "22222222" "22222222" "22222222" "22222222",  /* 13 */
    "66666666" "66666666" "66666666" "66666666",  /* 14 */
    "66666666" "66666666" "66666666" "66666666",  /* 15 */
    "44444444" "44444444" "44444444" "44444444",  /* 16 */
    "44444444" "44444444" "44444444" "44444444",  /* 17 */
    "44444444" "44444444" "44444444" "44444444",  /* 18 */
    "44444444" "44444444" "44444444" "44444444",  /* 19 */
    "77777777" "77777777" "77777777" "77777777",  /* 20 */
    "66666666" "66666666" "66666666" "66666666",  /* 21 */
    "66666666" "66666666" "66666666" "66666666",  /* 22 */
    "66666666" "66666666" "66666666" "66666666"   /* 23 */
};

/* ------------------------------------------------------------------------ */
/* Star sprite data                                                         */
/* ------------------------------------------------------------------------ */

static const unsigned char star_x[16] = {
    10, 30, 52, 64, 14, 40, 150, 184, 200, 224, 240, 248, 100, 196, 6, 180
};

static const unsigned char star_y[16] = {
    6, 22, 10, 44, 60, 72, 4, 8, 30, 50, 14, 70, 2, 64, 34, 56
};

/* twinkle cycle: small, medium, big, medium */
static const unsigned char twinkle_seq[4] = { 0, 1, 2, 1 };

/* ------------------------------------------------------------------------ */
/* Fade                                                                     */
/* ------------------------------------------------------------------------ */

/* Brightness levels 0 (black) .. 3 (full colour) */
static unsigned char bg_cur[16];
static unsigned char spr_cur[16];

static unsigned char scale_color(unsigned char c, unsigned char level)
{
    unsigned char r = c & 3;
    unsigned char g = (c >> 2) & 3;
    unsigned char b = (c >> 4) & 3;

    r = (unsigned char)((r * level + 1) / 3);
    g = (unsigned char)((g * level + 1) / 3);
    b = (unsigned char)((b * level + 1) / 3);

    return RGB(r, g, b);
}

static void apply_fade(unsigned char level)
{
    unsigned char i;

    for (i = 0; i < 16; i++) {
        bg_cur[i]  = scale_color(bg_pal_full[i],  level);
        spr_cur[i] = scale_color(spr_pal_full[i], level);
    }
}

/* ------------------------------------------------------------------------ */
/* Main                                                                     */
/* ------------------------------------------------------------------------ */

void main(void)
{
    unsigned char i, x, y;
    unsigned char frame = 0;
    unsigned char level = 0;
    unsigned char fade_timer = 0;
    signed char dir = 1;

    /* Start from black */
    apply_fade(0);
    SMS_loadBGPalette(bg_cur);
    SMS_loadSpritePalette(spr_cur);

    /* Load tiles */
    SMS_loadTiles(bg_tiles, 0, sizeof(bg_tiles));
    SMS_loadTiles(star_tiles, STAR_BASE, sizeof(star_tiles));

    /* Draw the picture into the tilemap */
    SMS_setNextTileatXY(0, 0);
    for (y = 0; y < 24; y++) {
        for (x = 0; x < 32; x++) {
            SMS_setTile(picture[y][x] - '0');
        }
    }

    /* Sprites use the first 256 tiles, so star tiles 16..18 are addressed directly */
    SMS_useFirstHalfTilesforSprites(1);
    SMS_initSprites();

    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();
        SMS_copySpritestoSAT();

        /* One brightness step every 4 frames, ping-ponging 0 -> 3 -> 0 */
        if (++fade_timer == 4) {
            fade_timer = 0;

            if (level == 3)
                dir = -1;
            else if (level == 0)
                dir = 1;

            level = (unsigned char)(level + dir);

            apply_fade(level);
            SMS_loadBGPalette(bg_cur);
            SMS_loadSpritePalette(spr_cur);
        }

        /* Build the sprite list for the next frame: twinkling stars */
        SMS_initSprites();
        for (i = 0; i < 16; i++) {
            unsigned char phase = (unsigned char)((frame >> 3) + (i * 3)) & 3;
            SMS_addSprite(star_x[i], star_y[i], STAR_BASE + twinkle_seq[phase]);
        }

        frame++;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
