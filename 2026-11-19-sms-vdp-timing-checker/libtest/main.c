/*
 * SMSlib during active display: one VRAM/CRAM function per frame, called when
 * the V counter reaches line 64, display on. test_id (read by smsvdp --ram at
 * the end of the frame) says which function ran in that frame.
 * MIT license (this file). SMSlib is devkitSMS by sverx (see its license).
 */
#include "SMSlib.h"

#define NTESTS 24
volatile unsigned char test_id;

static const unsigned char data[256] = {
  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
};
static const unsigned char pal16[16] = {0x00, 0x3f, 0x30, 0x0c, 0x03, 0x3c, 0x33, 0x0f,
                                        0x15, 0x2a, 0x10, 0x04, 0x01, 0x14, 0x11, 0x05};
static unsigned char ram_buf[64];

static void wait_line(unsigned char l) {
  while (SMS_getVCount() != l) {}
}

static void run_test(unsigned char t) {
  unsigned char i;
  switch (t) {
    case 0: SMS_loadTiles(data, 256, 128); break;                    /* SMS_VRAMmemcpy, 4 tiles */
    case 1: SMS_VRAMmemcpy(0x2000, data, 128); break;
    case 2: SMS_VRAMmemcpy_brief(0x2000, data, 128); break;
    case 3: SMS_VRAMmemset(0x2000, 0x00, 128); break;
    case 4: SMS_VRAMmemsetW(0x2000, 0x0000, 128); break;
    case 5: SMS_loadTileMap(0, 0, data, 64); break;                  /* 32 entries */
    case 6: SMS_loadTileMapArea(0, 0, data, 8, 4); break;            /* 32 entries */
    case 7: SMS_loadTileMapColumn(0, 0, data, 24); break;
    case 8: for (i = 0; i < 32; i++) SMS_setTileatXY(i, 0, i); break;
    case 9: SMS_setNextTileatXY(0, 0); for (i = 0; i < 32; i++) SMS_setTile(i); break;
    case 10: SMS_loadBGPalette(pal16); break;
    case 11: SMS_loadSpritePalette(pal16); break;
    case 12: for (i = 0; i < 16; i++) SMS_setBGPaletteColor(i, pal16[i]); break;
    case 13: SMS_zeroBGPalette(); break;
    case 14: SMS_loadBGPaletteHalfBrightness(pal16); break;
    case 15: SMS_load1bppTiles(data, 256, 32, 0, 1); break;          /* 4 tiles from 1bpp */
    case 16: SMS_load2bppTiles(data, 256, 64); break;                /* 4 tiles from 2bpp */
    case 17: SMS_setNextTileatXY(0, 0); SMS_print((const unsigned char *)"HELLO WORLD 0123456789 ABCDEFGHI"); break;
    case 18: SMS_copySpritestoSAT(); break;
    case 19: UNSAFE_SMS_copySpritestoSAT(); break;
    case 20: UNSAFE_SMS_VRAMmemcpy128(0x2000, data); break;
    case 21: UNSAFE_SMS_VRAMmemcpy(0x2000, data, 128); break;
    case 22: SMS_readVRAM(ram_buf, 0x2000, 64); break;
    case 23: for (i = 0; i < 32; i++) { SMS_setAddr(0x2000 + i); SMS_setTile(i); } break;
  }
}

void main(void) {
  unsigned char t = 0, i;
  SMS_autoSetUpTextRenderer();
  SMS_initSprites();
  for (i = 0; i < 16; i++) SMS_addSprite(i * 12, 100, i);
  SMS_displayOn();
  for (;;) {
    SMS_waitForVBlank();
    wait_line(64);
    test_id = t;
    run_test(t);
    t = (t + 1) % NTESTS;
  }
}

SMS_EMBED_SEGA_ROM_HEADER(9999, 0);
SMS_EMBED_SDSC_HEADER_AUTO_DATE(1, 0, "aitoollabs", "SMSlib timing", "SMSlib VRAM calls during display");
