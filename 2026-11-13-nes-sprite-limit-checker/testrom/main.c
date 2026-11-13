/* nessprites test ROM (MIT). Shows one sprite layout for 120 frames, then
 * the next. TAG holds the layout number of the table being built, and the NMI
 * copies it to SHOWN_TAG ($07F5) when that table reaches the PPU. Background is blank, so
 * every lost sprite pixel shows in the picture.
 *
 *  0 eight      8 sprites on one row                   (at the limit)
 *  1 nine       9 on one row                           (one over)
 *  2 twelve     12 on one row, fixed order             (the same 4 lost every frame)
 *  3 rotate     12 on one row, OAM order rotated each frame (flicker)
 *  4 tall       10 x 8x16 sprites on one row
 *  5 zeroed     unused entries left at Y = 0 (cleared OAM), 8 real sprites near the top
 *  6 flag_miss  8 on a row plus a 10th: lost, but the hardware flag stays clear
 *  7 flag_false 8 on a row, nothing lost, but the hardware flag is set
 *  8 spread     24 sprites in 3 rows of 8, 16 lines apart (nothing lost)
 *  9 rotate5    like 3, but the starting slot moves by 5 each frame, not by 1
 */
#include "nes.h"

#define FRAMES_PER_LAYOUT 120
#define NLAYOUTS 10
#define ROW_Y 100

static const unsigned char palette[32] = {
  0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
  0x0F, 0x16, 0x27, 0x30,  0x0F, 0x12, 0x22, 0x30,  0x0F, 0x1A, 0x2A, 0x30,  0x0F, 0x14, 0x24, 0x30,
};

static unsigned char i, n, layout, t, rot;

static void put(unsigned char k, unsigned char y, unsigned char tile, unsigned char attr, unsigned char x) {
  OAM_Y(k) = y; OAM_TILE(k) = tile; OAM_ATTR(k) = attr; OAM_X(k) = x;
}

static void row(unsigned char first, unsigned char count, unsigned char y, unsigned char tile) {
  for (i = 0; i < count; ++i) put(first + i, y, tile, i & 3, 16 + i * 18);
}

void main(void) {
  ppu_setup(palette);
  for (;;) {
    TAG = layout;
    oam_hide_all();
    ppu_ctrl = 0x80;
    switch (layout) {
      case 0: row(0, 8, ROW_Y, 0x01); break;
      case 1: row(0, 9, ROW_Y, 0x01); break;
      case 2: row(0, 12, ROW_Y, 0x04); break;
      case 3:
        /* the same 12 sprites, written starting at a different OAM slot each frame */
        for (i = 0; i < 12; ++i) {
          n = i + rot; if (n >= 12) n -= 12;
          put(n, ROW_Y, 0x04, i & 3, 16 + i * 18);
        }
        if (++rot == 12) rot = 0;
        break;
      case 4:
        ppu_ctrl = 0x80 | CTRL_SPR_8X16;
        for (i = 0; i < 10; ++i) put(i, ROW_Y, 0x10, i & 3, 16 + i * 22);
        break;
      case 5:
        /* what a cleared OAM looks like: Y = 0, tile 0 (blank), X = 0 */
        for (i = 0; i < 56; ++i) put(i, 0, 0, 0, 0);
        for (i = 0; i < 8; ++i) put(56 + i, 3, 0x02, i & 3, 40 + i * 20);
        break;
      case 6:
        row(0, 8, ROW_Y, 0x01);
        put(8, 200, 0x01, 0, 100);            /* not on the row */
        put(9, ROW_Y, 0x05, 1, 16 + 8 * 18);  /* on the row; its tile number $05 is not "in range" */
        break;
      case 7:
        row(0, 8, ROW_Y, 0x01);
        put(8, 200, 0x01, 0, 100);
        put(9, 200, ROW_Y + 3, 1, 140);       /* tile number 103 reads as a Y on the row */
        break;
      case 9:
        for (i = 0; i < 12; ++i) {
          n = i + rot; if (n >= 12) n -= 12;
          put(n, ROW_Y, 0x04, i & 3, 16 + i * 18);
        }
        rot += 5; if (rot >= 12) rot -= 12;
        break;
      case 8:
        row(0, 8, ROW_Y - 16, 0x0B);
        row(8, 8, ROW_Y, 0x0B);
        row(16, 8, ROW_Y + 16, 0x0B);
        break;
    }
    oam_ready();
    wait_frame();
    if (++t == FRAMES_PER_LAYOUT) {
      t = 0;
      if (++layout == NLAYOUTS) layout = 0;
    }
  }
}
