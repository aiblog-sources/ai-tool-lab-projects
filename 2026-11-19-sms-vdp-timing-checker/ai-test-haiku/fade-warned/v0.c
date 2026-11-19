/*
 * Palette fade demo for Sega Master System (devkitSMS / SMSlib / SDCC)
 *
 * - 32x24 background picture of concentric rectangles, 8 colors (8 solid tiles)
 * - Palette fades black -> full color -> black, one step every 4 frames
 * - 16 star sprites twinkle by changing their tile
 *
 * All CRAM, VRAM and SAT writes happen right after SMS_waitForVBlank(),
 * so the program is safe on real hardware (no CRAM/VRAM writes during
 * active display).
 *
 * Build: sdcc -c -mz80 main.c
 */

#include <SMSlib.h>

/* ------------------------------------------------------------------ */
/* Palettes: SMS CRAM format 00BBGGRR (2 bits per channel, 0..3)      */
/* ------------------------------------------------------------------ */

/* Background palette: entries 0..7 are the 8 picture colors */
static const unsigned char bgSourcePalette[16] = {
    RGB(3,0,0),   /* 0: red     */
    RGB(3,2,0),   /* 1: orange  */
    RGB(3,3,0),   /* 2: yellow  */
    RGB(0,3,0),   /* 3: green   */
    RGB(0,3,3),   /* 4: cyan    */
    RGB(0,0,3),   /* 5: blue    */
    RGB(3,0,3),   /* 6: magenta */
    RGB(3,3,3),   /* 7: white   */
    0, 0, 0, 0, 0, 0, 0, 0
};

/* Sprite palette: entry 0 is the backdrop (black), entry 1 is star color */
static const unsigned char spriteSourcePalette[16] = {
    0,            /* 0: backdrop (black) */
    RGB(3,3,3),   /* 1: star color       */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------ */
/* Tile graphics                                                      */
/* ------------------------------------------------------------------ */

/* Planar 4bpp planes for a solid 8x8 tile of color k.
 * Plane p is 0xFF where bit p of k is set, else 0x00. */
static const unsigned char solidTilePlanes[8][4] = {
    {0x00,0x00,0x00,0x00},  /* color 0 */
    {0xFF,0x00,0x00,0x00},  /* color 1 */
    {0x00,0xFF,0x00,0x00},  /* color 2 */
    {0xFF,0xFF,0x00,0x00},  /* color 3 */
    {0x00,0x00,0xFF,0x00},  /* color 4 */
    {0xFF,0x00,0xFF,0x00},  /* color 5 */
    {0x00,0xFF,0xFF,0x00},  /* color 6 */
    {0xFF,0xFF,0xFF,0x00}   /* color 7 */
};

/* 1bpp star shape masks, 8x8 pixels, bit 7 = leftmost pixel.
 * Shapes are drawn with sprite color index 1. */
static const unsigned char starMask[3][8] = {
    {0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00},  /* 0: small dot */
    {0x10,0x10,0x10,0xFE,0x10,0x10,0x10,0x00},  /* 1: plus      */
    {0x92,0x54,0x38,0xFE,0x38,0x54,0x92,0x00}   /* 2: sparkle   */
};

/* Star positions and twinkle phases */
static const unsigned char starX[16] = {
    12, 40, 70, 96, 130, 150, 176, 200,
    20, 55, 84, 112, 140, 166, 192, 230
};
static const unsigned char starY[16] = {
    10, 30, 18, 50, 12, 40, 60, 24,
    80, 100, 70, 120, 96, 150, 130, 170
};
static const unsigned char starPhase[16] = {
    0, 3, 1, 2,  2, 0, 3, 1,  1, 2, 0, 3,  3, 1, 2, 0
};

/* Twinkle cycle: tile numbers for the three star shapes */
static const unsigned char twinkleSeq[4] = { 0, 1, 2, 1 };

/* ------------------------------------------------------------------ */
/* RAM buffers (filled once at startup)                               */
/* ------------------------------------------------------------------ */

static unsigned char bgTileBuf[8 * 32];     /* 8 background tiles   */
static unsigned char starTileBuf[3 * 32];   /* 3 sprite tiles       */
static unsigned int  mapBuf[32 * 24];       /* tilemap 32x24        */
static unsigned char bgPal[16];             /* current faded colors */
static unsigned char spPal[16];

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static void buildTiles(void)
{
    unsigned char k, r, p;

    for (k = 0; k < 8; k++) {
        for (r = 0; r < 8; r++) {
            for (p = 0; p < 4; p++) {
                bgTileBuf[k * 32 + r * 4 + p] = solidTilePlanes[k][p];
            }
        }
    }

    for (k = 0; k < 3; k++) {
        for (r = 0; r < 8; r++) {
            starTileBuf[k * 32 + r * 4 + 0] = starMask[k][r]; /* plane 0 = shape */
            starTileBuf[k * 32 + r * 4 + 1] = 0x00;           /* plane 1 */
            starTileBuf[k * 32 + r * 4 + 2] = 0x00;           /* plane 2 */
            starTileBuf[k * 32 + r * 4 + 3] = 0x00;           /* plane 3 */
        }
    }
}

/* Concentric rectangles picture: tile color = ring index (0..7) */
static void buildMap(void)
{
    unsigned char x, y;
    int dx, dy, d;

    for (y = 0; y < 24; y++) {
        for (x = 0; x < 32; x++) {
            dx = (int)x - 16;
            if (dx < 0) dx = -dx;
            dy = (int)y - 12;
            if (dy < 0) dy = -dy;
            d = (dx > dy) ? dx : dy;               /* 0..16 */
            mapBuf[y * 32 + x] = (unsigned int)((d * 8) / 17); /* 0..7 */
        }
    }
}

/* Scale a 2-bit channel by level (0..7) with rounding */
static unsigned char scaleChannel(unsigned char c, unsigned char level)
{
    return (unsigned char)((c * level + 3) / 7);
}

static unsigned char fadeColor(unsigned char src, unsigned char level)
{
    unsigned char r = src & 3;
    unsigned char g = (src >> 2) & 3;
    unsigned char b = (src >> 4) & 3;

    return RGB(scaleChannel(r, level),
               scaleChannel(g, level),
               scaleChannel(b, level));
}

static void updatePalettes(unsigned char level)
{
    unsigned char i;

    for (i = 0; i < 16; i++) {
        bgPal[i] = fadeColor(bgSourcePalette[i], level);
        spPal[i] = fadeColor(spriteSourcePalette[i], level);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

void main(void)
{
    unsigned char level = 0;   /* fade level 0 (black) .. 7 (full color) */
    int dir = 1;               /* +1 fading in, -1 fading out            */
    unsigned int frame = 0;
    unsigned char i, t;

    buildTiles();
    buildMap();
    updatePalettes(0);

    /* Initial load while display is off */
    SMS_displayOff();

    SMS_loadTiles(bgTileBuf, 0, sizeof(bgTileBuf));        /* BG tiles 0..7   */
    SMS_loadTiles(starTileBuf, 256, sizeof(starTileBuf));  /* sprite tiles 0..2 */
    SMS_loadTileMap(0, 0, mapBuf, sizeof(mapBuf));

    SMS_loadBGPalette(bgPal);
    SMS_loadSpritePalette(spPal);

    SMS_initSprites();
    SMS_copySpritestoSAT();

    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();

        /* ---- VBlank: safe window for CRAM and VRAM writes ---- */
        SMS_loadBGPalette(bgPal);
        SMS_loadSpritePalette(spPal);
        SMS_copySpritestoSAT();

        /* ---- Active CPU work for the next frame ---- */
        frame++;

        /* One brightness step every 4 frames */
        if ((frame & 3) == 0) {
            level = (unsigned char)(level + dir);
            if (level == 7) {
                dir = -1;
            } else if (level == 0) {
                dir = 1;
            }
            updatePalettes(level);
        }

        /* Rebuild sprite list (RAM only) with twinkling tiles */
        SMS_initSprites();
        for (i = 0; i < 16; i++) {
            t = twinkleSeq[((frame >> 3) + starPhase[i]) & 3];
            SMS_addSprite(starX[i], starY[i], t);
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
