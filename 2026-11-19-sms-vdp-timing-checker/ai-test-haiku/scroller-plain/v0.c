/*
 * Horizontal scroller for the Sega Master System
 * devkitSMS / SMSlib / SDCC
 *
 * - 64-tile-wide level (512 px), 32-column tilemap (256 px, wraps in hardware)
 * - Background scrolls 1 pixel per frame to the right
 * - Each level column is written into the tilemap just before it becomes visible
 * - Tiles, palette and level are all defined as C arrays in this file
 *
 * Build:
 *   sdcc -c -mz80 main.c
 *   (link with crt0_sms.rel and SMSlib.lib, then run makesms)
 */

#include <SMSlib.h>

/* ------------------------------------------------------------------ */
/* Configuration                                                       */
/* ------------------------------------------------------------------ */

#define LEVEL_COLS      64              /* level width in tiles            */
#define LEVEL_PX        (LEVEL_COLS*8)  /* level width in pixels (512)     */
#define MAP_COLS        32              /* tilemap width (hardware wrap)   */
#define VIS_ROWS        24              /* visible tile rows (192 lines)   */

#define NO_BRICK        0xFF            /* column has no brick             */
#define NO_GROUND       24              /* ground row >= 24: none visible  */

/* Tile indices in VRAM */
#define TILE_SKY        0
#define TILE_GRASS      1
#define TILE_DIRT       2
#define TILE_BRICK      3

/* ------------------------------------------------------------------ */
/* Palette (16 entries, RGB() from SMSlib.h: 0..3 per channel)         */
/* ------------------------------------------------------------------ */

static const unsigned char bg_palette[16] = {
    RGB(0,0,0),     /* 0: backdrop / black    */
    RGB(1,2,3),     /* 1: sky blue            */
    RGB(2,1,0),     /* 2: brown               */
    RGB(0,3,0),     /* 3: grass green         */
    RGB(1,0,0),     /* 4: dark brown          */
    RGB(3,0,0),     /* 5: brick red           */
    RGB(3,3,3),     /* 6: mortar / white      */
    0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------ */
/* Tile pixel data: 8x8, one palette index per pixel (row-major)       */
/* ------------------------------------------------------------------ */

static const unsigned char tile_sky[64] = {
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1
};

static const unsigned char tile_grass[64] = {
    3,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,
    2,2,4,2,2,2,2,2,
    2,2,2,2,2,4,2,2,
    2,4,2,2,2,2,2,2,
    2,2,2,2,4,2,2,2,
    2,2,2,2,2,2,2,2
};

static const unsigned char tile_dirt[64] = {
    2,2,2,2,2,2,2,2,
    2,2,4,2,2,2,2,2,
    2,2,2,2,2,2,4,2,
    2,2,2,2,2,2,2,2,
    4,2,2,2,2,2,2,2,
    2,2,2,4,2,2,2,2,
    2,2,2,2,2,2,2,4,
    2,2,2,2,2,2,2,2
};

static const unsigned char tile_brick[64] = {
    6,6,6,6,6,6,6,6,
    5,5,5,5,5,5,5,6,
    5,5,5,5,5,5,5,6,
    5,5,5,5,5,5,5,6,
    6,6,6,6,6,6,6,6,
    6,5,5,5,6,5,5,5,
    6,5,5,5,6,5,5,5,
    6,5,5,5,6,5,5,5
};

/* 4 tiles x 32 bytes of SMS planar 4bpp data, built at startup */
static unsigned char tile_buf[4 * 32];

/* ------------------------------------------------------------------ */
/* Level data: one entry per column (64 columns)                       */
/* ------------------------------------------------------------------ */

/* Tile row where the ground (grass + dirt) starts. >= NO_GROUND = pit */
static const unsigned char col_ground[LEVEL_COLS] = {
/* 0-15  */ 20,20,20,20,20,20,20,20,20,20,20,20,24,24,24,20,
/* 16-31 */ 20,20,20,20,19,18,18,19,20,20,20,20,20,20,20,20,
/* 32-47 */ 20,20,24,24,20,20,20,20,20,20,20,20,20,20,20,20,
/* 48-63 */ 20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20
};

/* Tile row of a floating brick in this column, or NO_BRICK */
static const unsigned char col_brick[LEVEL_COLS] = {
/* 0-15  */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,15,15,15,15,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
/* 16-31 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,14,14,14,14,0xFF,0xFF,
/* 32-47 */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,16,16,16,16,16,0xFF,0xFF,
/* 48-63 */ 0xFF,0xFF,15,15,15,15,0xFF,0xFF,0xFF,0xFF,15,15,15,15,0xFF,0xFF
};

/* ------------------------------------------------------------------ */
/* Scrolling state                                                     */
/* ------------------------------------------------------------------ */

static unsigned int scroll;     /* current scroll position, 0..LEVEL_PX-1 */
static unsigned int next_col;   /* next (unwrapped) level column to write  */

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Convert 8x8 palette-index pixels to SMS 4bpp planar tile format.
   Each pixel row is 4 bytes (one per bitplane), bit 7 = leftmost pixel. */
static void encode_tile(const unsigned char *px, unsigned char *dst)
{
    unsigned char y, x, plane;

    for (y = 0; y < 8; y++) {
        for (plane = 0; plane < 4; plane++) {
            unsigned char bits = 0;
            for (x = 0; x < 8; x++) {
                if (px[y * 8 + x] & (1 << plane))
                    bits |= (unsigned char)(0x80 >> x);
            }
            *dst++ = bits;
        }
    }
}

static void load_tiles(void)
{
    encode_tile(tile_sky,   &tile_buf[TILE_SKY   * 32]);
    encode_tile(tile_grass, &tile_buf[TILE_GRASS * 32]);
    encode_tile(tile_dirt,  &tile_buf[TILE_DIRT  * 32]);
    encode_tile(tile_brick, &tile_buf[TILE_BRICK * 32]);

    SMS_loadTiles(tile_buf, 0, sizeof(tile_buf));
}

/* Write one level column into the tilemap.
   'c' is an unwrapped column index: level column = c % 64,
   tilemap column = c % 32. */
static void draw_column(unsigned int c)
{
    unsigned char lc  = (unsigned char)(c & (LEVEL_COLS - 1));
    unsigned char cx  = (unsigned char)(c & (MAP_COLS - 1));
    unsigned char ground = col_ground[lc];
    unsigned char brick  = col_brick[lc];
    unsigned char row, t;

    for (row = 0; row < VIS_ROWS; row++) {
        if (row >= ground) {
            t = (row == ground) ? TILE_GRASS : TILE_DIRT;
        } else if (row == brick) {
            t = TILE_BRICK;
        } else {
            t = TILE_SKY;
        }
        SMS_setTileatXY(cx, row, t);
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

void main(void)
{
    unsigned int needed;

    SMS_displayOff();

    /* Hide screen column 0 (left 8 px). The 32-column map wraps, so the
       fine-scroll seam lands in that strip and stays invisible. */
    SMS_VDPturnOnFeature(VDPFEATURE_LEFTCOLBLANK);

    load_tiles();
    SMS_loadBGPalette(bg_palette);

    /* Scroll 0 shows tilemap columns 0..31 (level columns 0..31) */
    scroll = 0;
    next_col = 0;
    while (next_col <= 31) {
        draw_column(next_col);
        next_col++;
    }

    SMS_setBGScrollX(0);
    SMS_displayOn();

    for (;;) {
        SMS_waitForVBlank();

        /* Apply the scroll for this frame (VDP register is 8 bits) */
        SMS_setBGScrollX((unsigned char)(scroll & 0xFF));

        /* Advance one pixel for the next frame */
        scroll++;
        if (scroll >= LEVEL_PX) {
            /* Level loops seamlessly: 512 px and 64 columns are one period */
            scroll -= LEVEL_PX;
            next_col -= LEVEL_COLS;
        }

        /* Rightmost pixel visible next frame is scroll+255.
           Write every column up to and including the one containing it. */
        needed = (scroll + 255) / 8;
        while (next_col <= needed) {
            draw_column(next_col);
            next_col++;
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
