/*
 * main.c - Sega Master System
 * Animated 48x32 background boss (6x4 tiles, 4 frames, one frame per VBlank)
 * and a 16x16 player sprite that moves left/right.
 *
 * Build: sdcc -c -mz80 main.c  (link with crt0_sms.rel + SMSlib.lib, then makesms)
 */

#include "SMSlib.h"

/* ------------------------------------------------------------------ */
/* Layout constants                                                    */
/* ------------------------------------------------------------------ */

#define BOSS_TILE_BASE   1      /* BG VRAM tiles 1..24 hold the boss     */
#define BOSS_COLS        6
#define BOSS_ROWS        4
#define BOSS_X           13     /* (32 - 6) / 2 : centred horizontally   */
#define BOSS_Y           4      /* top row of the boss on screen         */
#define BOSS_FRAMES      4

#define PLAYER_Y         160    /* sprite top, below the boss            */
#define PLAYER_SPEED     2
#define PLAYER_START_X   120

/* 1bpp pool tile ids (index into boss_pool_1bpp) */
#define T_SOLID          0
#define T_CTL            1      /* corner top-left                       */
#define T_CTR            2      /* corner top-right                      */
#define T_CBL            3      /* corner bottom-left                    */
#define T_CBR            4      /* corner bottom-right                   */
#define T_EYE_OPEN       5
#define T_EYE_HALF       6
#define T_EYE_CLOSED     7
#define T_ETOP           8      /* top edge                              */
#define T_EBOT           9      /* bottom edge                           */
#define T_EYE_WIDE       10
#define BOSS_POOL_COUNT  11

#define SPR_TILE_COUNT   4      /* player: TL, TR, BL, BR (8x8 each)     */

/* ------------------------------------------------------------------ */
/* Art: 1bpp source (8 bytes per tile, one byte per row, bit7 = left)  */
/* Colour index 1 = bit set, colour index 0 = background               */
/* ------------------------------------------------------------------ */

static const unsigned char boss_pool_1bpp[BOSS_POOL_COUNT * 8] = {
    /* T_SOLID */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* T_CTL */
    0x00,0x0F,0x3F,0x7F,0x7F,0xFF,0xFF,0xFF,
    /* T_CTR */
    0x00,0xF0,0xFC,0xFE,0xFE,0xFF,0xFF,0xFF,
    /* T_CBL */
    0xFF,0xFF,0xFF,0xFF,0xFF,0x7F,0x3F,0x00,
    /* T_CBR */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFC,0x00,
    /* T_EYE_OPEN */
    0xFF,0xFF,0xC3,0x81,0x81,0xC3,0xFF,0xFF,
    /* T_EYE_HALF */
    0xFF,0xFF,0xFF,0xC3,0xC3,0xFF,0xFF,0xFF,
    /* T_EYE_CLOSED */
    0xFF,0xFF,0xFF,0xFF,0x00,0xFF,0xFF,0xFF,
    /* T_ETOP */
    0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* T_EBOT */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,
    /* T_EYE_WIDE */
    0xFF,0xC3,0x81,0x81,0x81,0x81,0xC3,0xFF
};

/* Player ship, 16x16 split into four 8x8 tiles: TL, TR, BL, BR */
static const unsigned char spr_1bpp[SPR_TILE_COUNT * 8] = {
    /* TL */
    0x01,0x03,0x07,0x0F,0x1F,0x3F,0x7F,0xFF,
    /* TR */
    0x80,0xC0,0xE0,0xF0,0xF8,0xFC,0xFE,0xFF,
    /* BL */
    0xFF,0x7F,0x7F,0x3F,0x1F,0x1F,0x0F,0x06,
    /* BR */
    0xFF,0xFE,0xFE,0xFC,0xF8,0xF8,0xF0,0x60
};

/* Boss animation frames: 4 frames x 4 rows x 6 columns of pool ids.  */
/* Each frame replaces the graphics of all 24 boss tiles.             */
static const unsigned char boss_frames[BOSS_FRAMES][BOSS_ROWS][BOSS_COLS] = {
    /* frame 0: wide eyes */
    {
        { T_CTL, T_ETOP, T_ETOP, T_ETOP, T_ETOP, T_CTR },
        { T_SOLID, T_SOLID, T_EYE_WIDE, T_EYE_WIDE, T_SOLID, T_SOLID },
        { T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID },
        { T_CBL, T_EBOT, T_EBOT, T_EBOT, T_EBOT, T_CBR }
    },
    /* frame 1: open eyes */
    {
        { T_CTL, T_ETOP, T_ETOP, T_ETOP, T_ETOP, T_CTR },
        { T_SOLID, T_SOLID, T_EYE_OPEN, T_EYE_OPEN, T_SOLID, T_SOLID },
        { T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID },
        { T_CBL, T_EBOT, T_EBOT, T_EBOT, T_EBOT, T_CBR }
    },
    /* frame 2: half-closed eyes */
    {
        { T_CTL, T_ETOP, T_ETOP, T_ETOP, T_ETOP, T_CTR },
        { T_SOLID, T_SOLID, T_EYE_HALF, T_EYE_HALF, T_SOLID, T_SOLID },
        { T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID },
        { T_CBL, T_EBOT, T_EBOT, T_EBOT, T_EBOT, T_CBR }
    },
    /* frame 3: blink (closed eyes) */
    {
        { T_CTL, T_ETOP, T_ETOP, T_ETOP, T_ETOP, T_CTR },
        { T_SOLID, T_SOLID, T_EYE_CLOSED, T_EYE_CLOSED, T_SOLID, T_SOLID },
        { T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID, T_SOLID },
        { T_CBL, T_EBOT, T_EBOT, T_EBOT, T_EBOT, T_CBR }
    }
};

/* Palettes: SMS colour format, RGB(r,g,b) with components 0..3 */
static const unsigned char bg_palette[16] = {
    RGB(0,0,1),   /* 0: backdrop / eyes */
    RGB(3,2,0),   /* 1: boss body (orange) */
    RGB(3,3,3),   /* 2: unused */
    RGB(1,1,2),   /* 3: unused */
    0,0,0,0,0,0,0,0,0,0,0,0
};

static const unsigned char spr_palette[16] = {
    0,            /* 0: transparent */
    RGB(3,3,0),   /* 1: player ship (yellow) */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/* ------------------------------------------------------------------ */
/* RAM buffers holding 4bpp planar tiles (expanded once at start-up)   */
/* ------------------------------------------------------------------ */

static unsigned char boss_pool_4bpp[BOSS_POOL_COUNT * 32];
static unsigned char spr_4bpp[SPR_TILE_COUNT * 32];

/* Convert 1bpp row bytes to SMS 4bpp planar tiles.
   Each row is 4 bytes (planes 0..3); only plane 0 is set, so
   colour index 1 = pixel set, colour index 0 = pixel clear. */
static void expand_1bpp_to_4bpp(const unsigned char *src,
                                unsigned char *dst,
                                unsigned char count)
{
    unsigned char t, y;
    unsigned char *d;

    for (t = 0; t < count; t++) {
        for (y = 0; y < 8; y++) {
            d = dst + (unsigned int)t * 32 + (unsigned int)y * 4;
            d[0] = src[(unsigned int)t * 8 + y];
            d[1] = 0;
            d[2] = 0;
            d[3] = 0;
        }
    }
}

/* Replace the graphics of all 24 boss tiles with the given frame. */
static void boss_upload_frame(unsigned char f)
{
    unsigned char r, c;
    unsigned int  dst;
    unsigned int  src;

    for (r = 0; r < BOSS_ROWS; r++) {
        for (c = 0; c < BOSS_COLS; c++) {
            dst = (unsigned int)(BOSS_TILE_BASE + r * BOSS_COLS + c) * 32;
            src = (unsigned int)boss_frames[f][r][c] * 32;
            SMS_VRAMmemcpy(dst, &boss_pool_4bpp[src], 32);
        }
    }
}

/* Player sprite: 16x16 made of four 8x8 sprites.
   Sprite tiles 0..3 live at VRAM tiles 256..259 (second half). */
static void draw_player(unsigned char x)
{
    unsigned int xr = (unsigned int)x + 8;

    SMS_addSprite(x,  PLAYER_Y,     0);
    SMS_addSprite(xr, PLAYER_Y,     1);
    SMS_addSprite(x,  PLAYER_Y + 8, 2);
    SMS_addSprite(xr, PLAYER_Y + 8, 3);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

void main(void)
{
    unsigned char frame = 0;
    unsigned char px    = PLAYER_START_X;
    unsigned int  keys;
    unsigned char r, c;

    SMS_displayOff();

    /* Clear tile 0 (used as blank background) and the name table */
    SMS_VRAMmemset(0x0000, 0, 32);
    SMS_VRAMmemset(SMS_PNTAddress_READ, 0, 1792);

    /* Prepare 4bpp tile data in RAM and load player tiles */
    expand_1bpp_to_4bpp(boss_pool_1bpp, boss_pool_4bpp, BOSS_POOL_COUNT);
    expand_1bpp_to_4bpp(spr_1bpp, spr_4bpp, SPR_TILE_COUNT);

    SMS_useFirstHalfTilesforSprites(0);          /* sprites use tiles 256..511 */
    SMS_VRAMmemcpy(256 * 32, spr_4bpp, SPR_TILE_COUNT * 32);
    SMS_setSpriteMode(SPRITEMODE_NORMAL);

    /* Background: place the 24 boss tiles in the name table once.
       Only their graphics change afterwards. */
    for (r = 0; r < BOSS_ROWS; r++) {
        for (c = 0; c < BOSS_COLS; c++) {
            SMS_setTileatXY(BOSS_X + c, BOSS_Y + r,
                            BOSS_TILE_BASE + r * BOSS_COLS + c);
        }
    }

    boss_upload_frame(0);

    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(spr_palette);

    SMS_displayOn();

    /* Main loop: one iteration per VBlank (60 Hz NTSC / 50 Hz PAL),
       so the boss advances one animation frame per VBlank. */
    for (;;) {
        keys = SMS_getKeysHeld();

        if ((keys & PORT_A_KEY_LEFT) && (px >= PLAYER_SPEED)) {
            px -= PLAYER_SPEED;
        }
        if ((keys & PORT_A_KEY_RIGHT) && (px <= 240 - PLAYER_SPEED)) {
            px += PLAYER_SPEED;
        }

        SMS_initSprites();
        draw_player(px);

        SMS_waitForVBlank();
        SMS_copySpritestoSAT();
        boss_upload_frame(frame);

        frame = (frame + 1) & 3;   /* 4 animation frames, looping */
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
