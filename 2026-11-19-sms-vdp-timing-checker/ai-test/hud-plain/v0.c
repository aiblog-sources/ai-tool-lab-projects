#include "SMSlib.h"

/* ------------------------------------------------------------------ */
/* Tile layout in VRAM                                                */
/*   0      blank                                                     */
/*   1..10  digits '0'..'9'                                           */
/*   11..15 letters S C O R E                                         */
/*   16     enemy sprite (sprites use the first half of tile memory)  */
/* ------------------------------------------------------------------ */
#define TILE_BLANK   0
#define TILE_DIGIT0  1
#define TILE_S       11
#define TILE_C       12
#define TILE_O       13
#define TILE_R       14
#define TILE_E       15
#define TILE_ENEMY   16

#define NUM_ENEMIES  8
#define SCORE_DIGITS 6

/* 1bpp 8x8 glyphs (converted to 4bpp by SMS_load1bppTiles) */
const unsigned char font_tiles[16 * 8] = {
  /* blank */ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  /* 0 */ 0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00,
  /* 1 */ 0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00,
  /* 2 */ 0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00,
  /* 3 */ 0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00,
  /* 4 */ 0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00,
  /* 5 */ 0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00,
  /* 6 */ 0x3C,0x60,0x7C,0x66,0x66,0x66,0x3C,0x00,
  /* 7 */ 0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00,
  /* 8 */ 0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00,
  /* 9 */ 0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00,
  /* S */ 0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00,
  /* C */ 0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00,
  /* O */ 0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00,
  /* R */ 0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00,
  /* E */ 0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00
};

const unsigned char enemy_tile[8] = {
  0x3C,0x7E,0xDB,0xFF,0xFF,0x7E,0x24,0x42
};

/* Palettes (16 entries each) */
const unsigned char bg_palette[16] = {
  RGB(0,0,1), RGB(3,3,3), RGB(3,3,0), RGB(0,3,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

const unsigned char sprite_palette[16] = {
  RGB(0,0,0), RGB(3,0,0), RGB(3,3,0), RGB(0,3,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
  RGB(0,0,0), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0)
};

/* Score kept as decimal digits, index 0 = most significant */
unsigned char score[SCORE_DIGITS];

/* Enemy state */
int enemy_x[NUM_ENEMIES];
signed char enemy_dx[NUM_ENEMIES];
unsigned char enemy_y[NUM_ENEMIES];

/* score += 10 (wraps at 999999) */
static void add_score_10(void) {
  signed char i = SCORE_DIGITS - 2;   /* tens digit */
  while (i >= 0) {
    score[i]++;
    if (score[i] < 10) break;
    score[i] = 0;
    i--;
  }
}

static void draw_score(void) {
  unsigned char i;
  SMS_setNextTileatXY(8, 0);
  for (i = 0; i < SCORE_DIGITS; i++) {
    SMS_setTile(TILE_DIGIT0 + score[i]);
  }
}

void main(void) {
  unsigned char i;

  SMS_init();
  SMS_displayOff();

  SMS_useFirstHalfTilesforSprites(1);
  SMS_setSpriteMode(SPRITEMODE_NORMAL);

  /* palettes */
  SMS_loadBGPalette(bg_palette);
  SMS_loadSpritePalette(sprite_palette);
  SMS_setBackdropColor(0);

  /* tiles */
  SMS_load1bppTiles(font_tiles, TILE_BLANK, sizeof(font_tiles), 0, 1);
  SMS_load1bppTiles(enemy_tile, TILE_ENEMY, sizeof(enemy_tile), 0, 1);

  /* clear tilemap (32x28 entries, 2 bytes each) */
  SMS_VRAMmemsetW(SMS_PNTAddress, TILE_BLANK, 32 * 28 * 2);

  /* "SCORE" label */
  SMS_setNextTileatXY(1, 0);
  SMS_setTile(TILE_S);
  SMS_setTile(TILE_C);
  SMS_setTile(TILE_O);
  SMS_setTile(TILE_R);
  SMS_setTile(TILE_E);

  /* init score */
  for (i = 0; i < SCORE_DIGITS; i++) score[i] = 0;
  draw_score();

  /* init enemies */
  for (i = 0; i < NUM_ENEMIES; i++) {
    enemy_x[i] = 16 + i * 26;
    enemy_y[i] = 40 + i * 16;
    enemy_dx[i] = (i & 1) ? -(1 + (i >> 1)) : (1 + (i >> 1));
  }

  SMS_initSprites();
  SMS_displayOn();

  for (;;) {
    /* game logic */
    add_score_10();

    for (i = 0; i < NUM_ENEMIES; i++) {
      enemy_x[i] += enemy_dx[i];
      if (enemy_x[i] <= 8) {
        enemy_x[i] = 8;
        if (enemy_dx[i] < 0) enemy_dx[i] = -enemy_dx[i];
      } else if (enemy_x[i] >= 240) {
        enemy_x[i] = 240;
        if (enemy_dx[i] > 0) enemy_dx[i] = -enemy_dx[i];
      }
    }

    /* build sprite list */
    SMS_initSprites();
    for (i = 0; i < NUM_ENEMIES; i++) {
      SMS_addSprite(enemy_x[i], enemy_y[i], TILE_ENEMY);
    }

    /* VRAM updates during vblank */
    SMS_waitForVBlank();
    SMS_copySpritestoSAT();
    draw_score();
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
