#include "SMSlib.h"

/* Boss: 48x32 px = 6x4 BG tiles, VRAM tiles 1..24.
   Player: 16x16 sprite built from two 8x16 sprites (tall sprite mode). */

#define BOSS_TX     13
#define BOSS_TY     8
#define BOSS_W      6
#define BOSS_H      4
#define BOSS_TILES  (BOSS_W * BOSS_H)   /* 24 tiles = 768 bytes */
#define BOSS_FIRST  1                   /* VRAM tile index of first boss tile */
#define PLAYER_Y    160

/* ---------------- Palettes (6-bit SMS colors) ---------------- */
const unsigned char bgPalette[16] = {
  RGB(0,0,1),   /* 0 backdrop / empty  */
  RGB(0,0,0),   /* 1 outline / mouth   */
  RGB(3,3,3),   /* 2 white (eyes)      */
  RGB(3,0,0),   /* 3 red (pupils)      */
  RGB(0,1,0),   /* 4 body shade        */
  RGB(0,2,0),   /* 5 body shade        */
  RGB(1,3,1),   /* 6 body shade        */
  RGB(0,2,1),   /* 7 body shade        */
  0,0,0,0,0,0,0,0
};

const unsigned char spritePalette[16] = {
  RGB(0,0,0),   /* 0 transparent */
  RGB(0,2,3),   /* 1 hull        */
  RGB(3,3,0),   /* 2 yellow      */
  RGB(3,3,3),   /* 3 cockpit     */
  0,0,0,0,0,0,0,0,0,0,0,0
};

/* ---------------- Boss shape: half-width per pixel row ---------------- */
const unsigned char bossHalfW[32] = {
   6, 9,11,13,15,16,17,18,
  19,20,20,21,21,22,22,22,
  23,23,23,22,22,21,21,20,
  19,18,17,15,13,11, 9, 6
};

/* ---------------- Player ship 16x16 (bit15 = leftmost pixel) ---------------- */
const unsigned int shipMask[16] = {
  0x0180,0x0180,0x03C0,0x03C0,0x07E0,0x07E0,0x0FF0,0x1FF8,
  0x3FFC,0x7FFE,0x7FFE,0xFFFF,0xFFFF,0xF99F,0xC183,0x8181
};
const unsigned int shipCore[16] = {
  0,0,0,0x0180,0x03C0,0x03C0,0x0180,0,
  0,0,0,0,0,0,0,0
};

/* ---------------- RAM buffers (filled at startup, display off) ---------------- */
static unsigned char bossGfx[4][BOSS_TILES * 32];   /* 4 frames x 768 bytes */
static unsigned char shipGfx[128];                  /* 4 tiles */
static unsigned int  bossMap[BOSS_TILES];

static unsigned char bossPixel(unsigned char x, unsigned char y, unsigned char f) {
  unsigned char dx = (x < 24) ? (unsigned char)(23 - x) : (unsigned char)(x - 24);
  unsigned char hw = bossHalfW[y];
  unsigned char open;
  if (dx >= hw) return 0;
  if (y == 0 || y == 31 || (unsigned char)(dx + 2) >= hw) return 1;
  /* eyes, pupils move with frame */
  if (y >= 9 && y <= 14 && dx >= 4 && dx <= 10) {
    if (y >= 11 && y <= 12 && dx >= (unsigned char)(5 + f) && dx <= (unsigned char)(6 + f))
      return 3;
    return 2;
  }
  /* mouth opens and closes */
  open = (f & 1) ? 4 : 2;
  if (dx < 9 && y >= 20 && y < (unsigned char)(20 + open))
    return (y == 20 && (dx & 1)) ? 2 : 1;
  /* body shading ripples with frame */
  return 4 + (((y >> 1) + f) & 3);
}

static void buildGraphics(void) {
  unsigned char f, ty, tx, r, b, c;
  unsigned char *out;
  unsigned char p0, p1, p2, p3;
  unsigned int row, t, h, v;

  for (f = 0; f < 4; f++) {
    for (ty = 0; ty < BOSS_H; ty++) {
      for (tx = 0; tx < BOSS_W; tx++) {
        out = &bossGfx[f][(ty * BOSS_W + tx) * 32];
        for (r = 0; r < 8; r++) {
          p0 = p1 = p2 = p3 = 0;
          for (b = 0; b < 8; b++) {
            c = bossPixel(tx * 8 + b, ty * 8 + r, f);
            if (c & 1) p0 |= (unsigned char)(0x80 >> b);
            if (c & 2) p1 |= (unsigned char)(0x80 >> b);
            if (c & 4) p2 |= (unsigned char)(0x80 >> b);
            if (c & 8) p3 |= (unsigned char)(0x80 >> b);
          }
          *out++ = p0; *out++ = p1; *out++ = p2; *out++ = p3;
        }
      }
    }
  }

  /* ship: tile order for tall sprites: leftTop, leftBottom, rightTop, rightBottom */
  for (h = 0; h < 2; h++) {
    for (v = 0; v < 2; v++) {
      out = &shipGfx[(h * 2 + v) * 32];
      for (r = 0; r < 8; r++) {
        row = v * 8 + r;
        p0 = p1 = 0;
        for (b = 0; b < 8; b++) {
          t = 15 - (h * 8 + b);
          if ((shipMask[row] >> t) & 1) {
            p0 |= (unsigned char)(0x80 >> b);
            if ((shipCore[row] >> t) & 1) p1 |= (unsigned char)(0x80 >> b);
          }
        }
        *out++ = p0; *out++ = p1; *out++ = 0; *out++ = 0;
      }
    }
  }

  for (t = 0; t < BOSS_TILES; t++) bossMap[t] = BOSS_FIRST + t;
}

void main(void) {
  unsigned char px = 120;
  unsigned char frame = 0;
  unsigned int keys;
  unsigned char *src;

  SMS_displayOff();
  SMS_init();
  SMS_displayOff();
  buildGraphics();

  SMS_setSpriteMode(SPRITEMODE_TALL);
  SMS_useFirstHalfTilesforSprites(0);   /* sprites use tiles 256+ */
  SMS_loadBGPalette(bgPalette);
  SMS_loadSpritePalette(spritePalette);
  SMS_setBackdropColor(0);

  SMS_VRAMmemset(0x0000, 0, 32);                       /* tile 0 blank */
  SMS_VRAMmemset(SMS_PNTAddress, 0, 32 * 24 * 2);      /* clear tilemap */
  SMS_loadTiles(bossGfx[0], BOSS_FIRST, BOSS_TILES * 32);
  SMS_loadTiles(shipGfx, 256, 128);
  SMS_loadTileMapArea(BOSS_TX, BOSS_TY, bossMap, BOSS_W, BOSS_H);

  SMS_initSprites();
  SMS_addSprite(px, PLAYER_Y, 0);
  SMS_addSprite(px + 8, PLAYER_Y, 2);
  SMS_copySpritestoSAT();
  SMS_displayOn();

  for (;;) {
    /* game logic first, outside VBlank */
    keys = SMS_getKeysStatus();
    if ((keys & PORT_A_KEY_LEFT) && px > 8) px -= 2;
    if ((keys & PORT_A_KEY_RIGHT) && px < 232) px += 2;

    SMS_initSprites();
    SMS_addSprite(px, PLAYER_Y, 0);
    SMS_addSprite(px + 8, PLAYER_Y, 2);

    frame = (frame + 1) & 3;
    src = bossGfx[frame];

    SMS_waitForVBlank();

    /* All VRAM writes happen right at the start of VBlank.
       768 bytes with unrolled OUTI (16 cycles/byte) = ~12300 cycles,
       which fits in the ~15900 cycles of NTSC VBlank. */
    SMS_copySpritestoSAT();
    UNSAFE_SMS_load4Tiles(src,       BOSS_FIRST);
    UNSAFE_SMS_load4Tiles(src + 128, BOSS_FIRST + 4);
    UNSAFE_SMS_load4Tiles(src + 256, BOSS_FIRST + 8);
    UNSAFE_SMS_load4Tiles(src + 384, BOSS_FIRST + 12);
    UNSAFE_SMS_load4Tiles(src + 512, BOSS_FIRST + 16);
    UNSAFE_SMS_load4Tiles(src + 640, BOSS_FIRST + 20);
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
