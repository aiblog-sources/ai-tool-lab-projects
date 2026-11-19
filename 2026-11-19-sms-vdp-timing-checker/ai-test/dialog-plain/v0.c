/* Dialog box + walking sprite for Sega Master System (devkitSMS / SMSlib, SDCC) */
#include "SMSlib.h"

/* ---------- palettes ---------- */
static const unsigned char bg_palette[16] = {
    RGB(0,0,1),   /* 0: dark blue background */
    RGB(3,3,3),   /* 1: white text */
    RGB(3,3,0),   /* 2: yellow frame */
    0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const unsigned char spr_palette[16] = {
    RGB(0,0,0),   /* 0: transparent */
    RGB(0,0,0),   /* 1: outline */
    RGB(3,2,1),   /* 2: skin */
    RGB(3,0,0),   /* 3: shirt */
    RGB(0,1,3),   /* 4: pants */
    0,0,0,0,0,0,0,0,0,0,0
};

/* ---------- background tiles (1bpp) ---------- */
/* glyph order: space, A-Z, '.', ',', '!', '?'  => tile index 0..30 */
#define GLYPH_COUNT 31
#define FRAME_BASE  32

static const unsigned char font_1bpp[GLYPH_COUNT * 8] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* space */
    0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00, /* A */
    0x7C,0x42,0x42,0x7C,0x42,0x42,0x7C,0x00, /* B */
    0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0x00, /* C */
    0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00, /* D */
    0x7E,0x40,0x40,0x7C,0x40,0x40,0x7E,0x00, /* E */
    0x7E,0x40,0x40,0x7C,0x40,0x40,0x40,0x00, /* F */
    0x3C,0x42,0x40,0x4E,0x42,0x42,0x3C,0x00, /* G */
    0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x00, /* H */
    0x3E,0x08,0x08,0x08,0x08,0x08,0x3E,0x00, /* I */
    0x1E,0x04,0x04,0x04,0x44,0x44,0x38,0x00, /* J */
    0x42,0x44,0x48,0x70,0x48,0x44,0x42,0x00, /* K */
    0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0x00, /* L */
    0x42,0x66,0x5A,0x42,0x42,0x42,0x42,0x00, /* M */
    0x42,0x62,0x52,0x4A,0x46,0x42,0x42,0x00, /* N */
    0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0x00, /* O */
    0x7C,0x42,0x42,0x7C,0x40,0x40,0x40,0x00, /* P */
    0x3C,0x42,0x42,0x42,0x4A,0x44,0x3A,0x00, /* Q */
    0x7C,0x42,0x42,0x7C,0x48,0x44,0x42,0x00, /* R */
    0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0x00, /* S */
    0x7E,0x08,0x08,0x08,0x08,0x08,0x08,0x00, /* T */
    0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0x00, /* U */
    0x42,0x42,0x42,0x42,0x24,0x24,0x18,0x00, /* V */
    0x42,0x42,0x42,0x42,0x5A,0x66,0x42,0x00, /* W */
    0x42,0x24,0x18,0x18,0x18,0x24,0x42,0x00, /* X */
    0x42,0x42,0x24,0x18,0x18,0x18,0x18,0x00, /* Y */
    0x7E,0x04,0x08,0x10,0x20,0x40,0x7E,0x00, /* Z */
    0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00, /* . */
    0x00,0x00,0x00,0x00,0x18,0x18,0x08,0x10, /* , */
    0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00, /* ! */
    0x3C,0x42,0x02,0x0C,0x10,0x00,0x10,0x00  /* ? */
};

/* frame pieces: 0 TL, 1 T, 2 TR, 3 L, 4 R, 5 BL, 6 B, 7 BR */
#define F_TL 0
#define F_T  1
#define F_TR 2
#define F_L  3
#define F_R  4
#define F_BL 5
#define F_B  6
#define F_BR 7

static const unsigned char frame_1bpp[8 * 8] = {
    0x00,0x00,0x00,0x3F,0x3F,0x30,0x30,0x30, /* TL */
    0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00, /* T  */
    0x00,0x00,0x00,0xFC,0xFC,0x0C,0x0C,0x0C, /* TR */
    0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30, /* L  */
    0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C, /* R  */
    0x30,0x30,0x30,0x3F,0x3F,0x00,0x00,0x00, /* BL */
    0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,0x00, /* B  */
    0x0C,0x0C,0x0C,0xFC,0xFC,0x00,0x00,0x00  /* BR */
};

/* ---------- sprite art: 2 walk frames, 16x16, chars '.'=0, '1'..'4' = palette index ---------- */
static const char sprite_art[2][16][17] = {
    {
        "....11111111....",
        "...1222222221...",
        "...1222222221...",
        "...1212222121...",
        "...1222222221...",
        "....12222221....",
        "..333333333333..",
        "..233333333332..",
        "..233333333332..",
        "...3333333333...",
        "...3333333333...",
        "....44444444....",
        "....444..444....",
        "....444..444....",
        "....111..111....",
        "...1111..1111..."
    },
    {
        "....11111111....",
        "...1222222221...",
        "...1222222221...",
        "...1212222121...",
        "...1222222221...",
        "....12222221....",
        "..333333333333..",
        "..233333333332..",
        "..233333333332..",
        "...3333333333...",
        "...3333333333...",
        "....44444444....",
        ".....444444.....",
        ".....444444.....",
        ".....111111.....",
        "....11111111...."
    }
};

/* ---------- dialog messages (uppercase only, max 28 chars per line, 4 lines) ---------- */
#define NUM_MSGS 4
static const char *const messages[NUM_MSGS][4] = {
    { "HELLO, TRAVELER!",
      "WELCOME TO THE VILLAGE",
      "OF PIXELS." },
    { "THE ROAD AHEAD IS LONG,",
      "AND THE NIGHTS ARE COLD.",
      "TAKE CARE OUT THERE." },
    { "HAVE YOU SEEN MY HAT?",
      "I LOST IT NEAR THE RIVER.",
      "IT WAS RED. I THINK.",
      "OR MAYBE BLUE?" },
    { "WELL, GOOD LUCK!",
      "COME BACK SOON." }
};

/* ---------- layout ---------- */
#define BOX_TOP     18
#define BOX_BOTTOM  23
#define TEXT_X      2
#define TEXT_Y      19
#define SPRITE_Y    112
#define WAIT_FRAMES 90

enum { ST_TYPE, ST_WAIT, ST_CLEAR };

/* ---------- helpers ---------- */
static unsigned char glyph_tile(char c)
{
    if (c >= 'A' && c <= 'Z') return (unsigned char)(c - 'A' + 1);
    if (c == '.') return 27;
    if (c == ',') return 28;
    if (c == '!') return 29;
    if (c == '?') return 30;
    return 0;
}

/* build planar 4bpp sprite tiles from the char art.
   variant v: bit0 = walk frame, bit1 = mirrored. 4 tiles each (TL, TR, BL, BR). */
static void load_sprite_tiles(void)
{
    unsigned char buf[32];
    unsigned char v, q, r, b, col, p0, p1, p2, p3, bit, px;
    unsigned char f, mirror, qx, qy;
    char ch;

    for (v = 0; v < 4; v++) {
        f = v & 1;
        mirror = v >> 1;
        for (q = 0; q < 4; q++) {
            qx = q & 1;
            qy = q >> 1;
            for (r = 0; r < 8; r++) {
                p0 = p1 = p2 = p3 = 0;
                for (b = 0; b < 8; b++) {
                    col = (unsigned char)(qx * 8 + b);
                    if (mirror) col = (unsigned char)(15 - col);
                    ch = sprite_art[f][qy * 8 + r][col];
                    px = (ch == '.') ? 0 : (unsigned char)(ch - '0');
                    bit = (unsigned char)(0x80 >> b);
                    if (px & 1) p0 |= bit;
                    if (px & 2) p1 |= bit;
                    if (px & 4) p2 |= bit;
                    if (px & 8) p3 |= bit;
                }
                buf[r * 4 + 0] = p0;
                buf[r * 4 + 1] = p1;
                buf[r * 4 + 2] = p2;
                buf[r * 4 + 3] = p3;
            }
            /* sprite tiles live in the second half of VRAM tiles (256+) */
            SMS_loadTiles(buf, 256 + v * 4 + q, 32);
        }
    }
}

static void draw_frame(void)
{
    unsigned char x, y;

    SMS_setNextTileatXY(0, BOX_TOP);
    SMS_setTile(FRAME_BASE + F_TL);
    for (x = 1; x < 31; x++) SMS_setTile(FRAME_BASE + F_T);
    SMS_setTile(FRAME_BASE + F_TR);

    for (y = BOX_TOP + 1; y < BOX_BOTTOM; y++) {
        SMS_setNextTileatXY(0, y);
        SMS_setTile(FRAME_BASE + F_L);
        for (x = 1; x < 31; x++) SMS_setTile(0);
        SMS_setTile(FRAME_BASE + F_R);
    }

    SMS_setNextTileatXY(0, BOX_BOTTOM);
    SMS_setTile(FRAME_BASE + F_BL);
    for (x = 1; x < 31; x++) SMS_setTile(FRAME_BASE + F_B);
    SMS_setTile(FRAME_BASE + F_BR);
}

static void clear_text_row(unsigned char row)
{
    unsigned char x;
    SMS_setNextTileatXY(1, row);
    for (x = 1; x < 31; x++) SMS_setTile(0);
}

void main(void)
{
    unsigned int i;
    unsigned char x = 8;
    signed char dir = 1;
    unsigned char anim = 0;
    unsigned char tick = 0;
    unsigned char variant, base;

    unsigned char state = ST_TYPE;
    unsigned char msg = 0;
    unsigned char line = 0;
    unsigned char col = 0;
    unsigned char wait = 0;
    unsigned char clr_row = 0;
    const char *s;

    SMS_displayOff();
    SMS_setSpriteMode(SPRITEMODE_NORMAL);
    SMS_useFirstHalfTilesforSprites(0);

    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);
    SMS_setBackdropColor(0);

    /* font glyphs at tiles 0.., frame pieces at FRAME_BASE.. */
    SMS_load1bppTiles(font_1bpp, 0, sizeof(font_1bpp), 0, 1);
    SMS_load1bppTiles(frame_1bpp, FRAME_BASE, sizeof(frame_1bpp), 0, 2);
    load_sprite_tiles();

    /* clear whole name table (32x28) */
    SMS_setNextTileatXY(0, 0);
    for (i = 0; i < 32 * 28; i++) SMS_setTile(0);

    draw_frame();
    SMS_initSprites();
    SMS_displayOn();

    for (;;) {
        /* --- sprite logic --- */
        tick++;
        if ((tick & 1) == 0) {
            x = (unsigned char)(x + dir);
            if (x >= 232) dir = -1;
            if (x <= 8)   dir = 1;
        }
        anim++;
        variant = (unsigned char)(((anim >> 3) & 1) | (dir < 0 ? 2 : 0));
        base = (unsigned char)(variant * 4);

        SMS_initSprites();
        SMS_addTwoAdjoiningSprites(x, SPRITE_Y, base);
        SMS_addTwoAdjoiningSprites(x, SPRITE_Y + 8, base + 2);

        SMS_waitForVBlank();
        SMS_copySpritestoSAT();

        /* --- dialog logic (VRAM writes right after vblank start) --- */
        switch (state) {
        case ST_TYPE:
            /* skip finished / missing lines */
            while (line < 4 && messages[msg][line] &&
                   messages[msg][line][col] == 0) {
                line++;
                col = 0;
            }
            if (line >= 4 || !messages[msg][line]) {
                wait = WAIT_FRAMES;
                state = ST_WAIT;
            } else {
                s = messages[msg][line];
                SMS_setTileatXY(TEXT_X + col, TEXT_Y + line, glyph_tile(s[col]));
                col++;
            }
            break;

        case ST_WAIT:
            if (--wait == 0) {
                clr_row = 0;
                state = ST_CLEAR;
            }
            break;

        case ST_CLEAR:
            clear_text_row(TEXT_Y + clr_row);
            clr_row++;
            if (clr_row >= 4) {
                msg++;
                if (msg >= NUM_MSGS) msg = 0;
                line = 0;
                col = 0;
                state = ST_TYPE;
            }
            break;
        }
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
