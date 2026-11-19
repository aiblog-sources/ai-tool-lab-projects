#include "SMSlib.h"

/* ---------- Tile data ---------- */

/* One row of a solid 8-pixel line in colour c (3 bitplanes used, 4th = 0) */
#define ROW(c) ((c)&1?0xFF:0x00),((c)&2?0xFF:0x00),((c)&4?0xFF:0x00),0x00
#define SOLID_TILE(c) ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c),ROW(c)

/* 8 background tiles, each a solid colour index 0..7 */
const unsigned char bg_tiles[8 * 32] = {
  SOLID_TILE(0), SOLID_TILE(1), SOLID_TILE(2), SOLID_TILE(3),
  SOLID_TILE(4), SOLID_TILE(5), SOLID_TILE(6), SOLID_TILE(7)
};

/* 3 star frames (bitplane 0 = outer shape, bitplane 1 = bright core) */
#define SROW(p0,p1) (p0),(p1),0x00,0x00
const unsigned char star_tiles[3 * 32] = {
  /* 0: small dot */
  SROW(0x00,0x00),SROW(0x00,0x00),SROW(0x00,0x00),SROW(0x18,0x00),
  SROW(0x18,0x00),SROW(0x00,0x00),SROW(0x00,0x00),SROW(0x00,0x00),
  /* 1: small plus */
  SROW(0x00,0x00),SROW(0x00,0x00),SROW(0x18,0x00),SROW(0x3C,0x18),
  SROW(0x3C,0x18),SROW(0x18,0x00),SROW(0x00,0x00),SROW(0x00,0x00),
  /* 2: big star */
  SROW(0x18,0x00),SROW(0x18,0x00),SROW(0x18,0x18),SROW(0xFF,0x18),
  SROW(0xFF,0x18),SROW(0x18,0x18),SROW(0x18,0x00),SROW(0x18,0x00)
};

/* ---------- Palettes ---------- */

/* 8 colours of the picture (remaining 8 entries stay black) */
const unsigned char bg_pal_full[8] = {
  RGB(0,0,1), RGB(0,0,3), RGB(0,2,3), RGB(0,3,2),
  RGB(1,3,0), RGB(3,3,0), RGB(3,1,0), RGB(3,0,1)
};

const unsigned char spr_pal[16] = {
  RGB(0,0,0), RGB(3,3,3), RGB(3,3,0), RGB(3,3,2),
  0,0,0,0,0,0,0,0,0,0,0,0
};

/* fade_tab[level][value] : scaled 2-bit channel value */
const unsigned char fade_tab[4][4] = {
  {0,0,0,0},
  {0,1,1,1},
  {0,1,2,2},
  {0,1,2,3}
};

/* brightness level per fade step: in then out, repeating */
const unsigned char level_seq[8] = {0,1,2,3,3,2,1,0};

/* star animation sequence */
const unsigned char star_seq[4] = {0,1,2,1};

#define NUM_STARS 16

const unsigned char star_x[NUM_STARS] = {
  20, 200, 90, 150, 40, 230, 120, 60, 180, 10, 210, 100, 140, 70, 240, 30
};

signed char star_h[NUM_STARS];
unsigned char pal_buf[16];

void build_palette(unsigned char level)
{
  unsigned char i, c, r, g, b;
  for (i = 0; i < 16; i++) pal_buf[i] = 0;
  for (i = 0; i < 8; i++) {
    c = bg_pal_full[i];
    r = fade_tab[level][c & 3];
    g = fade_tab[level][(c >> 2) & 3];
    b = fade_tab[level][(c >> 4) & 3];
    pal_buf[i] = RGB(r, g, b);
  }
}

void main(void)
{
  unsigned char x, y, i;
  unsigned char tick = 0, step = 0, frame = 0, dirty;

  /* Setup with display off: VRAM access is safe */
  SMS_displayOff();
  SMS_VDPturnOnFeature(VDPFEATURE_FRAMEIRQ);
  SMS_useFirstHalfTilesforSprites(0);
  SMS_setSpriteMode(SPRITEMODE_NORMAL);
  SMS_setBGScrollX(0);
  SMS_setBGScrollY(0);

  SMS_zeroBGPalette();
  SMS_loadSpritePalette(spr_pal);
  SMS_setBackdropColor(0);

  SMS_loadTiles(bg_tiles, 0, sizeof(bg_tiles));
  SMS_loadTiles(star_tiles, 256, sizeof(star_tiles));

  /* diagonal colour bands across the whole name table */
  for (y = 0; y < 28; y++) {
    SMS_setNextTileatXY(0, y);
    for (x = 0; x < 32; x++) {
      SMS_setTile((unsigned int)(((x + y) >> 2) & 7));
    }
  }

  SMS_initSprites();
  for (i = 0; i < NUM_STARS; i++) {
    star_h[i] = SMS_addSprite(star_x[i], 8 + i * 11, 0);
  }
  SMS_copySpritestoSAT();

  SMS_displayOn();

  for (;;) {
    dirty = 0;
    if (tick == 0) {
      build_palette(level_seq[step]);
      dirty = 1;
    }

    /* twinkle: change sprite tile (RAM copy only) */
    for (i = 0; i < NUM_STARS; i++) {
      SMS_updateSpriteImage(star_h[i],
                            star_seq[((frame >> 3) + i * 3) & 3]);
    }

    SMS_waitForVBlank();
    /* all VRAM/CRAM writes happen right after VBlank starts */
    SMS_copySpritestoSAT();
    if (dirty) SMS_loadBGPalette(pal_buf);

    frame++;
    tick++;
    if (tick == 4) {
      tick = 0;
      step = (step + 1) & 7;
    }
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
