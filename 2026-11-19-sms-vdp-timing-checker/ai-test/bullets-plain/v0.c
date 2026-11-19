#include "SMSlib.h"

#define NUM_BULLETS 40
#define NUM_STARS   64

#define TILE_BLANK  0
#define TILE_STAR   1
#define TILE_BULLET 2

/* 3 tiles, 4bpp planar (4 bytes per row, 8 rows = 32 bytes per tile) */
const unsigned char tiles[3 * 32] = {
  /* tile 0: blank */
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
  /* tile 1: star (small plus, colour 1) */
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x10,0x00,0x00,0x00,
  0x38,0x00,0x00,0x00,
  0x38,0x00,0x00,0x00,
  0x10,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,
  /* tile 2: bullet (round, outer colour 1, core colour 3) */
  0x3C,0x00,0x00,0x00,
  0x7E,0x3C,0x00,0x00,
  0xFF,0x7E,0x00,0x00,
  0xFF,0x7E,0x00,0x00,
  0xFF,0x7E,0x00,0x00,
  0xFF,0x7E,0x00,0x00,
  0x7E,0x3C,0x00,0x00,
  0x3C,0x00,0x00,0x00
};

const unsigned char bg_palette[16] = {
  RGB(0,0,1), RGB(3,3,3), RGB(1,2,3), RGB(2,2,2),
  0,0,0,0,0,0,0,0,0,0,0,0
};

const unsigned char spr_palette[16] = {
  RGB(0,0,0), RGB(3,0,0), RGB(3,2,0), RGB(3,3,1),
  0,0,0,0,0,0,0,0,0,0,0,0
};

/* quarter sine table, 64 steps per circle, amplitude 384 (1.5 px/frame in 8.8) */
const int sin_q[17] = {
  0, 38, 75, 111, 147, 181, 213, 244, 272, 297, 319, 339, 355, 367, 377, 382, 384
};

static int sinv(unsigned char a)
{
  unsigned char q, k;
  a &= 63;
  q = a >> 4;
  k = a & 15;
  switch (q) {
    case 0:  return sin_q[k];
    case 1:  return sin_q[16 - k];
    case 2:  return -sin_q[k];
    default: return -sin_q[16 - k];
  }
}

unsigned int  bx[NUM_BULLETS];
unsigned int  by[NUM_BULLETS];
unsigned char bang[NUM_BULLETS];

unsigned char star_x[NUM_STARS];
unsigned char star_y[NUM_STARS];

void main(void)
{
  unsigned char i, g, frame = 0;
  unsigned int n;
  unsigned char dir;
  int dx, dy;

  SMS_displayOff();
  SMS_VDPturnOnFeature(VDPFEATURE_LEFTCOLBLANK);
  SMS_useFirstHalfTilesforSprites(1);
  SMS_loadBGPalette(bg_palette);
  SMS_loadSpritePalette(spr_palette);
  SMS_loadTiles(tiles, 0, sizeof(tiles));

  /* clear the name table */
  SMS_setAddr(SMS_PNTAddress);
  for (n = 0; n < 768; n++)
    SMS_setTile(TILE_BLANK);

  /* stars: two per column, 12 rows apart so they never overlap */
  for (i = 0; i < 32; i++) {
    star_x[i]      = i;
    star_y[i]      = (unsigned char)((i * 5) % 24);
    star_x[i + 32] = i;
    star_y[i + 32] = (unsigned char)((star_y[i] + 12) % 24);
  }
  for (i = 0; i < NUM_STARS; i++)
    SMS_setTileatXY(star_x[i], star_y[i], TILE_STAR);

  /* bullets */
  for (i = 0; i < NUM_BULLETS; i++) {
    bx[i]   = (unsigned int)124 << 8;
    by[i]   = (unsigned int)92 << 8;
    bang[i] = (unsigned char)(((unsigned int)i * 32) / 5);
  }

  SMS_initSprites();
  SMS_displayOn();

  for (;;) {
    SMS_waitForVBlank();
    SMS_copySpritestoSAT();

    /* star field: each star moves one tile row down every 8 frames
       (8 stars updated per frame, staggered) */
    g = frame & 7;
    for (i = g; i < NUM_STARS; i += 8) {
      SMS_setTileatXY(star_x[i], star_y[i], TILE_BLANK);
      star_y[i]++;
      if (star_y[i] >= 24) star_y[i] = 0;
      SMS_setTileatXY(star_x[i], star_y[i], TILE_STAR);
    }

    /* bullets */
    SMS_initSprites();
    for (i = 0; i < NUM_BULLETS; i++) {
      if ((frame & 3) == 0) bang[i]++;
      dir = bang[i] >> 2;
      dx = sinv(dir + 16);   /* cos */
      dy = sinv(dir);

      bx[i] += (unsigned int)dx;            /* wraps naturally at 256 px */
      by[i] += (unsigned int)dy;
      if (by[i] >= 49152u) {
        if (by[i] >= 55000u) by[i] += 49152u;   /* went negative */
        else                 by[i] -= 49152u;   /* past bottom */
      }
      SMS_addSprite(bx[i] >> 8, by[i] >> 8, TILE_BULLET);
    }

    frame++;
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
