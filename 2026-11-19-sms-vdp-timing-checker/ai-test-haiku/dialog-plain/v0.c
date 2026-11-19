/* ============================================================================
 * dialog.c - SMS dialog box demo (devkitSMS / SMSlib / SDCC)
 *
 *  - Bottom 6 rows of the screen hold a framed dialog box.
 *  - Text is typed one character per frame, then the box is cleared and the
 *    next message starts.
 *  - A 16x16 character walks back and forth above the box.
 *  - All tiles and palettes are C arrays in this file.
 *
 *  Build: sdcc -c -mz80 main.c
 *         link with crt0_sms.rel and SMSlib.lib, then makesms
 * ==========================================================================*/

#include <SMSlib.h>

/* SMSlib only exposes the VDP control port (0xBF); declare the data port (0xBE) */
__sfr __at (0xBE) VDP_DATA;

/* ---------------------------------------------------------------------------
 * VRAM tile layout
 * ------------------------------------------------------------------------- */
#define FONT_BASE       0      /* tiles  0..31  : font glyphs, colour 3 (text)  */
#define FRAME_BASE      32     /* tiles 32..39  : box frame,   colour 1         */
#define SPR_RIGHT       128    /* tiles 128..131: character, facing right       */
#define SPR_LEFT        132    /* tiles 132..135: character, facing left        */

/* Dialog box geometry (in 8x8 tile units; screen is 32x24 tiles) */
#define BOX_X0          1
#define BOX_Y0          18     /* bottom 6 rows: 18..23 */
#define BOX_X1          30
#define BOX_Y1          23

/* Text area inside the box */
#define TXT_X           2
#define TXT_Y           19
#define TXT_W           28
#define TXT_H           4

/* Character */
#define CHAR_Y          118    /* top of the 16x16 sprite (box starts at y=144) */
#define CHAR_MIN_X      8
#define CHAR_MAX_X      232

/* Dialog timing */
#define HOLD_FRAMES     150    /* frames to keep a finished message on screen */
#define NUM_MESSAGES    4

/* Typewriter states */
#define ST_TYPE         0
#define ST_HOLD         1
#define ST_CLEAR        2

/* ---------------------------------------------------------------------------
 * Palettes (SMS CRAM format: 0000BBGGRR, 2 bits per channel via RGB())
 * ------------------------------------------------------------------------- */
static const unsigned char bg_palette[16] = {
    RGB(0,0,1),   /* 0: backdrop (dark navy)  */
    RGB(2,2,3),   /* 1: dialog frame (light blue) */
    RGB(0,0,0),   /* 2: unused */
    RGB(3,3,3),   /* 3: dialog text (white) */
    0,0,0,0,0,0,0,0,0,0,0,0
};

static const unsigned char spr_palette[16] = {
    RGB(0,0,0),   /* 0: transparent */
    RGB(3,2,1),   /* 1: skin */
    RGB(2,1,0),   /* 2: hair / eyes / legs */
    RGB(0,2,3),   /* 3: shirt */
    0,0,0,0,0,0,0,0,0,0,0,0
};

/* ---------------------------------------------------------------------------
 * Font: 1bpp 8x8 glyphs (bit 7 = leftmost pixel, row 0 = top)
 * Order: 0 ' ', 1 '!', 2 ',', 3 '-', 4 '.', 5 '?', 6..31 'A'..'Z'
 * ------------------------------------------------------------------------- */
static const unsigned char font[32][8] = {
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* ' ' */
    { 0x20,0x20,0x20,0x20,0x20,0x00,0x20,0x00 }, /* '!' */
    { 0x00,0x00,0x00,0x00,0x00,0x20,0x20,0x40 }, /* ',' */
    { 0x00,0x00,0x00,0xF8,0x00,0x00,0x00,0x00 }, /* '-' */
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00 }, /* '.' */
    { 0x70,0x88,0x08,0x10,0x20,0x00,0x20,0x00 }, /* '?' */
    { 0x70,0x88,0x88,0xF8,0x88,0x88,0x88,0x00 }, /* 'A' */
    { 0xF0,0x88,0x88,0xF0,0x88,0x88,0xF0,0x00 }, /* 'B' */
    { 0x70,0x88,0x80,0x80,0x80,0x88,0x70,0x00 }, /* 'C' */
    { 0xF0,0x88,0x88,0x88,0x88,0x88,0xF0,0x00 }, /* 'D' */
    { 0xF8,0x80,0x80,0xF0,0x80,0x80,0xF8,0x00 }, /* 'E' */
    { 0xF8,0x80,0x80,0xF0,0x80,0x80,0x80,0x00 }, /* 'F' */
    { 0x70,0x88,0x80,0xB8,0x88,0x88,0x78,0x00 }, /* 'G' */
    { 0x88,0x88,0x88,0xF8,0x88,0x88,0x88,0x00 }, /* 'H' */
    { 0x70,0x20,0x20,0x20,0x20,0x20,0x70,0x00 }, /* 'I' */
    { 0x38,0x10,0x10,0x10,0x10,0x90,0x60,0x00 }, /* 'J' */
    { 0x88,0x90,0xA0,0xC0,0xA0,0x90,0x88,0x00 }, /* 'K' */
    { 0x80,0x80,0x80,0x80,0x80,0x80,0xF8,0x00 }, /* 'L' */
    { 0x88,0xD8,0xA8,0xA8,0x88,0x88,0x88,0x00 }, /* 'M' */
    { 0x88,0xC8,0xA8,0x98,0x88,0x88,0x88,0x00 }, /* 'N' */
    { 0x70,0x88,0x88,0x88,0x88,0x88,0x70,0x00 }, /* 'O' */
    { 0xF0,0x88,0x88,0xF0,0x80,0x80,0x80,0x00 }, /* 'P' */
    { 0x70,0x88,0x88,0x88,0xA8,0x90,0x68,0x00 }, /* 'Q' */
    { 0xF0,0x88,0x88,0xF0,0xA0,0x90,0x88,0x00 }, /* 'R' */
    { 0x78,0x80,0x80,0x70,0x08,0x08,0xF0,0x00 }, /* 'S' */
    { 0xF8,0x20,0x20,0x20,0x20,0x20,0x20,0x00 }, /* 'T' */
    { 0x88,0x88,0x88,0x88,0x88,0x88,0x70,0x00 }, /* 'U' */
    { 0x88,0x88,0x88,0x88,0x88,0x50,0x20,0x00 }, /* 'V' */
    { 0x88,0x88,0x88,0xA8,0xA8,0xA8,0x50,0x00 }, /* 'W' */
    { 0x88,0x88,0x50,0x20,0x50,0x88,0x88,0x00 }, /* 'X' */
    { 0x88,0x88,0x50,0x20,0x20,0x20,0x20,0x00 }, /* 'Y' */
    { 0xF8,0x08,0x10,0x20,0x40,0x80,0xF8,0x00 }  /* 'Z' */
};

/* ---------------------------------------------------------------------------
 * Dialog frame tiles: 1bpp, drawn in colour 1
 * Order: 0 TL, 1 top, 2 TR, 3 left, 4 right, 5 BL, 6 bottom, 7 BR
 * ------------------------------------------------------------------------- */
static const unsigned char frame_tiles[8][8] = {
    { 0xFF,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0 }, /* top-left     */
    { 0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* top          */
    { 0xFF,0x03,0x03,0x03,0x03,0x03,0x03,0x03 }, /* top-right    */
    { 0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0 }, /* left         */
    { 0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03 }, /* right        */
    { 0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFF }, /* bottom-left  */
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF }, /* bottom       */
    { 0x03,0x03,0x03,0x03,0x03,0x03,0x03,0xFF }  /* bottom-right */
};

/* ---------------------------------------------------------------------------
 * Character: 16x16 pixel art, one char per pixel = colour index 0..3
 * ------------------------------------------------------------------------- */
static const char char_art[16][17] = {
    "0000222222220000",
    "0002222222222000",
    "0002211111122000",
    "0002121111212000",
    "0000111111110000",
    "0000011111100000",
    "0000033333300000",
    "0003333333333000",
    "0013333333333100",
    "0013333333333100",
    "0001333333331000",
    "0000033333330000",
    "0000022022200000",
    "0000022022200000",
    "0000222002220000",
    "0002222002222000"
};

/* ---------------------------------------------------------------------------
 * Dialog messages (uppercase, '\n' starts a new line, max 4 lines x 28 chars)
 * ------------------------------------------------------------------------- */
static const char *const messages[NUM_MESSAGES] = {
    "HELLO! WELCOME TO THE\nSEGA MASTER SYSTEM.",
    "THIS DIALOG BOX IS DRAWN\nONE CHARACTER PER FRAME.",
    "THE TILES, PALETTES AND\nSPRITES ARE ALL C ARRAYS.",
    "PRESS NOTHING - JUST\nWATCH THE CHARACTER WALK!"
};

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/* Convert a 1bpp 8x8 glyph into 4bpp VRAM tile using the given colour index */
static void loadGlyph1bpp(const unsigned char *g, unsigned char tile, unsigned char color)
{
    unsigned char r, p;

    SMS_setAddr(TILEtoADDR(tile));
    for (r = 0; r < 8; r++) {
        for (p = 0; p < 4; p++) {
            VDP_DATA = ((color >> p) & 1) ? g[r] : 0;
        }
    }
}

/* Convert one 8x8 quadrant of the 16x16 character into a 4bpp VRAM tile.
 * flip=1 mirrors the whole character horizontally (used for facing left). */
static void loadCharTile(unsigned char tile, unsigned char tx, unsigned char ty, unsigned char flip)
{
    unsigned char r, p, x, b, sx, col;
    unsigned char py, X;

    SMS_setAddr(TILEtoADDR(tile));
    for (r = 0; r < 8; r++) {
        py = (unsigned char)(ty * 8 + r);
        for (p = 0; p < 4; p++) {
            b = 0;
            for (x = 0; x < 8; x++) {
                X  = (unsigned char)(tx * 8 + x);
                sx = flip ? (unsigned char)(15 - X) : X;
                col = (unsigned char)(char_art[py][sx] - '0');
                if ((col >> p) & 1)
                    b |= (unsigned char)(0x80 >> x);
            }
            VDP_DATA = b;
        }
    }
}

/* Map an ASCII character to a font tile index (unknown chars -> space) */
static unsigned char glyphTile(unsigned char c)
{
    if (c == ' ') return 0;
    if (c == '!') return 1;
    if (c == ',') return 2;
    if (c == '-') return 3;
    if (c == '.') return 4;
    if (c == '?') return 5;
    if (c >= 'A' && c <= 'Z') return (unsigned char)(6 + (c - 'A'));
    return 0;
}

/* Put one tile into the background tilemap at (x,y) */
static void putTile(unsigned char x, unsigned char y, unsigned char t)
{
    SMS_setNextTileatXY(x, y);
    SMS_setTile(t);
}

/* Draw the framed dialog box on the bottom 6 rows */
static void drawDialogFrame(void)
{
    unsigned char x, y;

    putTile(BOX_X0, BOX_Y0, FRAME_BASE + 0);  /* top-left     */
    putTile(BOX_X1, BOX_Y0, FRAME_BASE + 2);  /* top-right    */
    putTile(BOX_X0, BOX_Y1, FRAME_BASE + 5);  /* bottom-left  */
    putTile(BOX_X1, BOX_Y1, FRAME_BASE + 7);  /* bottom-right */

    for (x = BOX_X0 + 1; x < BOX_X1; x++) {
        putTile(x, BOX_Y0, FRAME_BASE + 1);   /* top edge     */
        putTile(x, BOX_Y1, FRAME_BASE + 6);   /* bottom edge  */
    }
    for (y = BOX_Y0 + 1; y < BOX_Y1; y++) {
        putTile(BOX_X0, y, FRAME_BASE + 3);   /* left edge    */
        putTile(BOX_X1, y, FRAME_BASE + 4);   /* right edge   */
    }
}

/* ---------------------------------------------------------------------------
 * Main
 * ------------------------------------------------------------------------- */
void main(void)
{
    unsigned char i, y, c;
    unsigned char msgIdx = 0;
    unsigned char pos    = 0;
    unsigned char curX   = TXT_X;
    unsigned char curY   = TXT_Y;
    unsigned char state  = ST_TYPE;
    unsigned int  timer  = 0;
    unsigned int  tick   = 0;
    int           charX  = 120;
    int           charY;
    int           dir    = 1;
    unsigned char base;
    const char   *msg;

    SMS_displayOff();

    /* palettes */
    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    /* sprites use VRAM tiles 0..255 (we keep them at 128+) */
    SMS_useFirstHalfTilesforSprites(1);

    /* tiles */
    for (i = 0; i < 32; i++)
        loadGlyph1bpp(font[i], (unsigned char)(FONT_BASE + i), 3);
    for (i = 0; i < 8; i++)
        loadGlyph1bpp(frame_tiles[i], (unsigned char)(FRAME_BASE + i), 1);
    for (i = 0; i < 4; i++) {
        loadCharTile((unsigned char)(SPR_RIGHT + i), i & 1, i >> 1, 0);
        loadCharTile((unsigned char)(SPR_LEFT  + i), i & 1, i >> 1, 1);
    }

    /* clear the whole background tilemap (32x24 tiles, 2 bytes each) */
    SMS_VRAMmemset(XYtoADDR(0, 0), 0, 32 * 24 * 2);

    drawDialogFrame();

    SMS_displayOn();

    for (;;) {
        /* ---------- character: walk back and forth above the box ---------- */
        SMS_initSprites();

        charX += dir;
        if (charX <= CHAR_MIN_X) {
            charX = CHAR_MIN_X;
            dir = 1;
        } else if (charX >= CHAR_MAX_X) {
            charX = CHAR_MAX_X;
            dir = -1;
        }

        /* small up/down bob while walking */
        charY = CHAR_Y + ((tick >> 3) & 1);
        base  = (dir > 0) ? SPR_RIGHT : SPR_LEFT;

        SMS_addTwoAdjoiningSprites(charX, charY,     base);      /* top row    */
        SMS_addTwoAdjoiningSprites(charX, charY + 8, base + 2);  /* bottom row */

        SMS_waitForVBlank();
        SMS_copySpritestoSAT();
        tick++;

        /* ---------- dialog: typewriter, one character per frame ---------- */
        msg = messages[msgIdx];

        switch (state) {
        case ST_TYPE:
            c = (unsigned char)msg[pos];
            if (c == 0) {
                state = ST_HOLD;
                timer = HOLD_FRAMES;
            } else {
                pos++;
                if (c == '\n') {
                    curX = TXT_X;
                    curY++;
                } else {
                    SMS_setNextTileatXY(curX, curY);
                    SMS_setTile(glyphTile(c));
                    curX++;
                }
            }
            break;

        case ST_HOLD:
            if (--timer == 0)
                state = ST_CLEAR;
            break;

        case ST_CLEAR:
            /* clear the text area (interior of the box) */
            for (y = TXT_Y; y < TXT_Y + TXT_H; y++)
                SMS_VRAMmemset(XYtoADDR(TXT_X, y), 0, TXT_W * 2);

            msgIdx = (unsigned char)((msgIdx + 1) % NUM_MESSAGES);
            pos    = 0;
            curX   = TXT_X;
            curY   = TXT_Y;
            state  = ST_TYPE;
            break;
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
