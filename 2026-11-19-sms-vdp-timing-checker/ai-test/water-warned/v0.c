#include "SMSlib.h"

/* ------------------------------------------------------------------ */
/* Tile data (4bpp planar: 4 bytes per row, 32 bytes per tile)         */
/* ------------------------------------------------------------------ */

/* Water row: colour 1 base, colour 2 where bit of mask m is set.
   colour 1 = plane0 only, colour 2 = plane1 only. */
#define W(m)  (unsigned char)(~(m) & 0xFF), (unsigned char)((m) & 0xFF), 0, 0

/* Sprite row: colour 1 where p0, colour 2 where p1 (3 if both). */
#define S(p0,p1)  (unsigned char)(p0), (unsigned char)(p1), 0, 0

/* 4 animation frames of the water tile. Two wave lines moving in
   opposite directions, each frame shifted by one pixel. */
const unsigned char water_frames[4][32] = {
  { W(0x00), W(0x66), W(0x00), W(0x00), W(0x00), W(0x66), W(0x00), W(0x00) },
  { W(0x00), W(0x33), W(0x00), W(0x00), W(0x00), W(0xCC), W(0x00), W(0x00) },
  { W(0x00), W(0x99), W(0x00), W(0x00), W(0x00), W(0x99), W(0x00), W(0x00) },
  { W(0x00), W(0xCC), W(0x00), W(0x00), W(0x00), W(0x33), W(0x00), W(0x00) }
};

/* Boat: 4 tiles in order TL, TR, BL, BR */
const unsigned char boat_tiles[128] = {
  /* top-left: mast (colour 1) + sail (colour 2) */
  S(0x01,0x00), S(0x01,0x02), S(0x01,0x06), S(0x01,0x0E),
  S(0x01,0x1E), S(0x01,0x3E), S(0x01,0x7E), S(0x01,0xFE),
  /* top-right: sail mirrored */
  S(0x00,0x00), S(0x00,0x40), S(0x00,0x60), S(0x00,0x70),
  S(0x00,0x78), S(0x00,0x7C), S(0x00,0x7E), S(0x00,0x7F),
  /* bottom-left: hull */
  S(0xFF,0x00), S(0xFF,0x00), S(0x7F,0x00), S(0x3F,0x00),
  S(0x00,0x00), S(0x00,0x00), S(0x00,0x00), S(0x00,0x00),
  /* bottom-right: hull */
  S(0xFF,0x00), S(0xFF,0x00), S(0xFE,0x00), S(0xFC,0x00),
  S(0x00,0x00), S(0x00,0x00), S(0x00,0x00), S(0x00,0x00)
};

/* ------------------------------------------------------------------ */
/* Palettes (16 entries each)                                          */
/* ------------------------------------------------------------------ */
const unsigned char bg_palette[16] = {
  RGB(0,0,2), RGB(0,1,3), RGB(1,2,3), RGB(3,3,3),
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const unsigned char sprite_palette[16] = {
  RGB(0,0,0), RGB(2,1,0), RGB(3,3,3), RGB(3,0,0),
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Bobbing offsets (triangle-ish wave) */
const unsigned char bob_table[16] = {
  0, 1, 1, 2, 3, 3, 4, 4, 4, 4, 3, 3, 2, 1, 1, 0
};

#define WATER_TILE  0
#define BOAT_TILE   4
#define BOAT_X      120
#define BOAT_Y      88

static void draw_boat(unsigned char y)
{
  SMS_initSprites();
  SMS_addSprite(BOAT_X,     y,     BOAT_TILE);
  SMS_addSprite(BOAT_X + 8, y,     BOAT_TILE + 1);
  SMS_addSprite(BOAT_X,     y + 8, BOAT_TILE + 2);
  SMS_addSprite(BOAT_X + 8, y + 8, BOAT_TILE + 3);
}

void main(void)
{
  unsigned int frame = 0;
  unsigned char water_idx = 0;

  /* All VRAM setup is done with the display off, so it's safe on hardware */
  SMS_displayOff();
  SMS_waitForVBlank();

  SMS_useFirstHalfTilesforSprites(1);
  SMS_setSpriteMode(SPRITEMODE_NORMAL);

  SMS_loadBGPalette(bg_palette);
  SMS_loadSpritePalette(sprite_palette);
  SMS_setBackdropColor(0);

  SMS_loadTiles(water_frames[0], WATER_TILE, 32);
  SMS_loadTiles(boat_tiles, BOAT_TILE, 128);

  /* Fill the whole name table (32x28 entries = 1792 bytes) with the water tile */
  SMS_VRAMmemsetW(XYtoADDR(0, 0), WATER_TILE, 32 * 28 * 2);

  draw_boat(BOAT_Y);
  SMS_copySpritestoSAT();

  SMS_displayOn();

  for (;;) {
    SMS_waitForVBlank();

    /* All VRAM writes happen right after VBlank starts */
    SMS_copySpritestoSAT();

    if ((frame & 7) == 0) {
      water_idx = (water_idx + 1) & 3;
      SMS_loadTiles(water_frames[water_idx], WATER_TILE, 32);
    }

    frame++;

    /* Prepare next frame's sprites (RAM only) */
    draw_boat(BOAT_Y + bob_table[(frame >> 2) & 15]);
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
