/*
 * boss.c for Sega Master System (devkitSMS / SMSlib / SDCC)
 *
 *   sdcc -c -mz80 main.c
 *   (link with crt0_sms.rel + SMSlib.lib, then makesms)
 *
 * - 48x32 px boss = 6x4 = 24 background tiles, centred on screen.
 * - Its 24 pattern tiles are overwritten every frame with one of 4
 *   animation frames (60 frames/s on NTSC).
 * - 16x16 player sprite (4 hardware sprites) moves left/right at the bottom.
 * - All art and palettes are C arrays. No external assets.
 *
 * Hardware notes:
 * - All VRAM writes happen right after SMS_waitForVBlank(), while the
 *   VDP is not drawing. 768 bytes of tile data are written with an
 *   unrolled OUTI sequence (16 T-states per byte, about 12.3k cycles).
 *   That fits in the ~16k-cycle NTSC VBlank together with the SAT copy.
 * - Interrupts are disabled while the VDP address latch is in use, so
 *   the frame ISR cannot reset the latch in the middle of a write.
 */

#include <SMSlib.h>

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */
#define BOSS_TILE_BASE      1     /* BG tiles 1..24 hold the boss (contiguous) */
#define BOSS_TILES          24    /* 6 x 4 tiles = 48 x 32 pixels */
#define BOSS_TILE_COLS      6
#define BOSS_TILE_ROWS      4
#define BOSS_COL            13    /* top-left tile column: (32-6)/2 */
#define BOSS_ROW            10    /* top-left tile row:    (24-4)/2 */
#define BOSS_FRAMES         4
#define BOSS_FRAME_BYTES    (BOSS_TILES * 32)   /* 768 bytes of 4bpp data */

#define PLAYER_TILE_BASE    128   /* sprite tiles 128..131 (first sprite bank) */
#define PLAYER_Y            152
#define PLAYER_MIN_X        0
#define PLAYER_MAX_X        240   /* 256 - 16 */
#define PLAYER_SPEED        2

/* ------------------------------------------------------------------ */
/* Art source data (1 bpp: bit 7 = leftmost pixel, set bit = colour 1) */
/* Boss rows are 6 bytes (48 px). 4 frames x 32 rows.                 */
/* ------------------------------------------------------------------ */
#define ROW_TOP0    0x00,0x00,0xFF,0xFF,0x00,0x00
#define ROW_TOP1    0x00,0x0F,0xFF,0xFF,0xF0,0x00
#define ROW_TOP2    0x00,0xFF,0xFF,0xFF,0xFF,0x00
#define ROW_TOP3    0x03,0xFF,0xFF,0xFF,0xFF,0xC0
#define ROW_BODY    0x0F,0xFF,0xFF,0xFF,0xFF,0xF0
#define ROW_EYES    0x0F,0xF0,0x3F,0xFC,0x0F,0xF0
#define ROW_SLIT    0x0F,0xFF,0x00,0x00,0xFF,0xF0
#define ROW_MOUTHA  0x0F,0xFF,0xC0,0x03,0xFF,0xF0
#define ROW_MOUTHB  0x0F,0xFC,0x00,0x00,0x3F,0xF0
#define ROW_BOT3    0x03,0xFF,0xFF,0xFF,0xFF,0xC0
#define ROW_BOT2    0x00,0xFF,0xFF,0xFF,0xFF,0x00
#define ROW_BOT1    0x00,0x0F,0xFF,0xFF,0xF0,0x00
#define ROW_BOT0    0x00,0x00,0xFF,0xFF,0x00,0x00
#define ROW_BLANK   0x00,0x00,0x00,0x00,0x00,0x00

static const unsigned char boss_art[BOSS_FRAMES][32 * 6] = {
    { /* frame 0: eyes open, mouth shut */
        ROW_TOP0, ROW_TOP1, ROW_TOP2, ROW_TOP3,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_EYES, ROW_EYES, ROW_EYES, ROW_EYES,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_SLIT,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_BOT3, ROW_BOT2, ROW_BOT1, ROW_BOT0,
        ROW_BLANK
    },
    { /* frame 1: eyes half closed, mouth opening */
        ROW_TOP0, ROW_TOP1, ROW_TOP2, ROW_TOP3,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_BODY,
        ROW_EYES,
        ROW_BODY, ROW_BODY,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_MOUTHA, ROW_MOUTHB, ROW_MOUTHB, ROW_MOUTHA,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_BOT3, ROW_BOT2, ROW_BOT1, ROW_BOT0,
        ROW_BLANK
    },
    { /* frame 2: eyes closed, mouth wide open */
        ROW_TOP0, ROW_TOP1, ROW_TOP2, ROW_TOP3,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_MOUTHA,
        ROW_MOUTHB, ROW_MOUTHB, ROW_MOUTHB, ROW_MOUTHB,
        ROW_MOUTHA,
        ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_BOT3, ROW_BOT2, ROW_BOT1, ROW_BOT0,
        ROW_BLANK
    },
    { /* frame 3: eyes wide open, mouth shut */
        ROW_TOP0, ROW_TOP1, ROW_TOP2, ROW_TOP3,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_EYES, ROW_EYES, ROW_EYES, ROW_EYES, ROW_EYES, ROW_EYES,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_SLIT,
        ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY, ROW_BODY,
        ROW_BOT3, ROW_BOT2, ROW_BOT1, ROW_BOT0,
        ROW_BLANK
    }
};

/* Player 16x16 ship: 2 bytes per row, 16 rows */
static const unsigned char player_art[32] = {
    0x01,0x80, /* row  0 */
    0x03,0xC0, /* row  1 */
    0x03,0xC0, /* row  2 */
    0x07,0xE0, /* row  3 */
    0x07,0xE0, /* row  4 */
    0x0F,0xF0, /* row  5 */
    0x1F,0xF8, /* row  6 */
    0x3F,0xFC, /* row  7 */
    0xFF,0xFF, /* row  8 */
    0xFF,0xFF, /* row  9 */
    0xE7,0xE7, /* row 10 */
    0xC3,0xC3, /* row 11 */
    0x81,0x81, /* row 12 */
    0x81,0x81, /* row 13 */
    0x00,0x00, /* row 14 */
    0x00,0x00  /* row 15 */
};

/* Palettes (SMS: 2 bits per channel, RGB() from SMSlib.h) */
static const unsigned char bg_palette[16] = {
    RGB(0,0,1), RGB(3,1,0), 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

static const unsigned char sprite_palette[16] = {
    RGB(0,0,0), RGB(3,3,0), 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0
};

/* ------------------------------------------------------------------ */
/* RAM buffers                                                        */
/* ------------------------------------------------------------------ */
static unsigned char boss_frame[BOSS_FRAMES][BOSS_FRAME_BYTES]; /* expanded 4bpp */
static unsigned char player_tiles[128];

/* read by the inline assembly in upload_boss_frame() */
const unsigned char *upload_src;

/* ------------------------------------------------------------------ */
/* Conversion: 1 bpp rows -> SMS 4bpp tile (32 bytes)                 */
/* SMS tile layout: 8 rows x 4 bytes, one byte per bitplane per row.  */
/* Set pixels use plane 0 only, so they get colour index 1.           */
/* ------------------------------------------------------------------ */
static void expand_tile(const unsigned char *src, unsigned char stride, unsigned char *dst)
{
    unsigned char r;
    for (r = 0; r < 8; r++) {
        dst[0] = src[0];   /* bitplane 0 */
        dst[1] = 0;        /* bitplane 1 */
        dst[2] = 0;        /* bitplane 2 */
        dst[3] = 0;        /* bitplane 3 */
        src += stride;
        dst += 4;
    }
}

static void build_graphics(void)
{
    unsigned char f;
    unsigned int t, tr, tc;

    for (f = 0; f < BOSS_FRAMES; f++) {
        for (t = 0; t < BOSS_TILES; t++) {
            tr = t / BOSS_TILE_COLS;
            tc = t % BOSS_TILE_COLS;
            /* tile row tr starts at byte tr*8 rows * 6 bytes/row */
            expand_tile(&boss_art[f][tr * 48 + tc], 6, &boss_frame[f][t * 32]);
        }
    }

    for (t = 0; t < 4; t++) {
        tr = t >> 1;
        tc = t & 1;
        expand_tile(&player_art[tr * 16 + tc], 2, &player_tiles[t * 32]);
    }
}

/* ------------------------------------------------------------------ */
/* VBlank upload: writes the 24 boss tiles (768 bytes) to VRAM.       */
/* Must be called only during VBlank.                                 */
/* ------------------------------------------------------------------ */
static void upload_boss_frame(const unsigned char *src)
{
    upload_src = src;

    __asm
        di
    __endasm;

    SMS_setAddr(TILEtoADDR(BOSS_TILE_BASE));

    __asm
        ld   hl,(_upload_src)
        ld   c,#0xBE
        ld   d,#24
    boss_upload_loop:
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        outi
        dec  d
        jp   nz,boss_upload_loop
        ei
    __endasm;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
void main(void)
{
    unsigned char frame = 0;
    unsigned char px = 120;
    unsigned char tr, tc;
    unsigned int keys;

    SMS_displayOff();

    build_graphics();

    /* Tile 0 = blank, name table cleared to tile 0 */
    SMS_VRAMmemset(0, 0, 32);
    SMS_VRAMmemsetW(SMS_PNTAddress, 0, 1792);

    /* Boss pattern tiles (frame 0) and player sprite tiles */
    SMS_loadTiles(boss_frame[0], BOSS_TILE_BASE, BOSS_FRAME_BYTES);
    SMS_loadTiles(player_tiles, PLAYER_TILE_BASE, 128);

    /* Boss name table: the 24 tiles stay in place; only their pattern data changes */
    for (tr = 0; tr < BOSS_TILE_ROWS; tr++) {
        for (tc = 0; tc < BOSS_TILE_COLS; tc++) {
            SMS_setTileatXY(BOSS_COL + tc, BOSS_ROW + tr,
                            BOSS_TILE_BASE + tr * BOSS_TILE_COLS + tc);
        }
    }

    SMS_loadBGPalette(bg_palette);
    SMS_loadSpritePalette(sprite_palette);
    SMS_useFirstHalfTilesforSprites(1);   /* sprites read tiles 0..255 */
    SMS_initSprites();

    SMS_displayOn();

    while (1) {
        /* 1. Game logic (outside VBlank, no VRAM access) */
        keys = SMS_getKeysHeld();
        if ((keys & PORT_A_KEY_LEFT) && px >= PLAYER_MIN_X + PLAYER_SPEED) {
            px -= PLAYER_SPEED;
        }
        if ((keys & PORT_A_KEY_RIGHT) && px <= PLAYER_MAX_X - PLAYER_SPEED) {
            px += PLAYER_SPEED;
        }

        /* 2. Build the sprite list in RAM */
        SMS_initSprites();
        SMS_addSprite(px,     PLAYER_Y,     PLAYER_TILE_BASE + 0);
        SMS_addSprite(px + 8, PLAYER_Y,     PLAYER_TILE_BASE + 1);
        SMS_addSprite(px,     PLAYER_Y + 8, PLAYER_TILE_BASE + 2);
        SMS_addSprite(px + 8, PLAYER_Y + 8, PLAYER_TILE_BASE + 3);

        /* 3. VBlank: every VRAM write happens here */
        SMS_waitForVBlank();

        __asm
            di
        __endasm;
        SMS_copySpritestoSAT();

        upload_boss_frame(boss_frame[frame]);   /* ends with EI */

        frame = (frame + 1) & 3;
    }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
