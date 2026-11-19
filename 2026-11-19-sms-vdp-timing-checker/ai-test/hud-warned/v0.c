#include "SMSlib.h"

/* Tile layout in VRAM (BG and sprites share the first 256 tiles):
   0 = blank, 1..10 = digits 0-9, 11..15 = S C O R E, 16 = enemy */
#define TILE_BLANK   0
#define TILE_DIGIT0  1
#define TILE_S       11
#define TILE_C       12
#define TILE_O       13
#define TILE_R       14
#define TILE_E       15
#define TILE_ENEMY   16

#define NUM_ENEMIES  8
#define NUM_DIGITS   6

/* 1bpp font: 16 tiles x 8 bytes (expanded to 4bpp by SMS_load1bppTiles) */
static const unsigned char font1bpp[16 * 8] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, /* blank */
  0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00, /* 0 */
  0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00, /* 1 */
  0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00, /* 2 */
  0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00, /* 3 */
  0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00, /* 4 */
  0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00, /* 5 */
  0x3C,0x60,0x7C,0x66,0x66,0x66,0x3C,0x00, /* 6 */
  0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00, /* 7 */
  0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00, /* 8 */
  0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00, /* 9 */
  0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00, /* S */
  0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00, /* C */
  0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00, /* O */
  0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00, /* R */
  0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00  /* E */
};

/* Enemy sprite: 4bpp planar, 4 bytes per row (plane0..plane3) */
static const unsigned char enemyTile[32] = {
  0x3C,0x00,0x00,0x00,
  0x7E,0x00,0x00,0x00,
  0xDB,0x00,0x00,0x00,
  0xFF,0x24,0x00,0x00,
  0xFF,0x00,0x00,0x00,
  0x5A,0x00,0x00,0x00,
  0x81,0x00,0x00,0x00,
  0x42,0x00,0x00,0x00
};

/* Background palette: 0 black, 1 white, rest unused */
static const unsigned char bgPalette[16] = {
  RGB(0,0,0), RGB(3,3,3), RGB(3,3,0), RGB(0,3,3),
  0,0,0,0,0,0,0,0,0,0,0,0
};

/* Sprite palette: 0 (transparent), 1 red, 2 yellow, 3 white */
static const unsigned char sprPalette[16] = {
  RGB(0,0,0), RGB(3,0,0), RGB(3,3,0), RGB(3,3,3),
  0,0,0,0,0,0,0,0,0,0,0,0
};

static const unsigned char labelTiles[5] = { TILE_S, TILE_C, TILE_O, TILE_R, TILE_E };

static unsigned char digits[NUM_DIGITS];       /* digits[0] = most significant */
static unsigned char enemyX[NUM_ENEMIES];
static unsigned char enemyY[NUM_ENEMIES];
static signed char   enemyDX[NUM_ENEMIES];

static void addScore10(void)
{
  signed char i = NUM_DIGITS - 2;   /* tens digit */
  for (;;) {
    if (++digits[i] <= 9) break;
    digits[i] = 0;
    if (i == 0) break;              /* wrap at 999999 */
    i--;
  }
}

static void updateEnemies(void)
{
  unsigned char i;
  int nx;
  for (i = 0; i < NUM_ENEMIES; i++) {
    nx = (int)enemyX[i] + enemyDX[i];
    if (nx <= 8) {
      nx = 8;
      enemyDX[i] = -enemyDX[i];
    } else if (nx >= 240) {
      nx = 240;
      enemyDX[i] = -enemyDX[i];
    }
    enemyX[i] = (unsigned char)nx;
  }
}

void main(void)
{
  unsigned char i;

  /* ---- Setup with display off (VRAM safe to access) ---- */
  SMS_displayOff();
  SMS_useFirstHalfTilesforSprites(1);
  SMS_setSpriteMode(SPRITEMODE_NORMAL);

  SMS_loadBGPalette(bgPalette);
  SMS_loadSpritePalette(sprPalette);
  SMS_setBackdropColor(0);

  SMS_load1bppTiles(font1bpp, TILE_BLANK, sizeof(font1bpp), 0, 1);
  SMS_loadTiles(enemyTile, TILE_ENEMY, sizeof(enemyTile));

  /* clear name table (32x28 entries, 2 bytes each) */
  SMS_VRAMmemsetW(XYtoADDR(0, 0), TILE_BLANK, 32 * 28 * 2);

  /* static label "SCORE" */
  SMS_setNextTileatXY(1, 0);
  for (i = 0; i < 5; i++)
    SMS_setTile(labelTiles[i]);

  for (i = 0; i < NUM_DIGITS; i++)
    digits[i] = 0;

  for (i = 0; i < NUM_ENEMIES; i++) {
    enemyX[i]  = 16 + i * 24;
    enemyY[i]  = 40 + i * 16;
    enemyDX[i] = (i & 1) ? -(1 + (i >> 1) % 3) : (1 + (i >> 1) % 3);
  }

  SMS_initSprites();
  for (i = 0; i < NUM_ENEMIES; i++)
    SMS_addSprite(enemyX[i], enemyY[i], TILE_ENEMY);
  SMS_copySpritestoSAT();

  SMS_displayOn();

  /* ---- Main loop ---- */
  for (;;) {
    /* game logic first (no VRAM access) */
    addScore10();
    updateEnemies();

    /* all VRAM writes right after VBlank starts */
    SMS_waitForVBlank();

    SMS_setNextTileatXY(7, 0);
    for (i = 0; i < NUM_DIGITS; i++)
      SMS_setTile(TILE_DIGIT0 + digits[i]);

    SMS_initSprites();
    for (i = 0; i < NUM_ENEMIES; i++)
      SMS_addSprite(enemyX[i], enemyY[i], TILE_ENEMY);
    SMS_copySpritestoSAT();
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
