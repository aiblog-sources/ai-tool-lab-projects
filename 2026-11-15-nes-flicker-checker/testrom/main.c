/* nesflicker test ROM (MIT). Ten layouts of 16x16 objects (2x2 sprites of
 * tile $04), 150 frames each. Every frame the ROM writes a truth table for the
 * OAM table it just built, so the tracker (which only sees OAM) can be scored:
 *
 *   $0500 + 64 * (TAG & 1):  [0] object count n (max 16)
 *                            [1 + i]  bit 0 = object i is in this OAM table,
 *                                     bit 1 = object i exists (alive, on screen)
 *                            [17 + i] x    [33 + i] y   (top left, OAM Y)
 *
 * Between layouts the ROM shows 20 frames with no sprites.
 * TAG = layout * 2 + parity; the NMI copies it to SHOWN_TAG ($07F5) when the
 * table reaches the PPU, so the checker knows which truth buffer was shown.
 *
 *  0 calm     4 objects on separate rows, nothing hidden
 *  1 row6     6 objects on one row (12 sprites per line), fixed OAM order: the PPU drops the last 2 objects
 *  2 row6rot  the same 6, OAM start rotated by one object each frame: the PPU drops a different 2 each frame
 *  3 alt2     8 objects on one row, the game draws the even ones, then the odd ones (each hidden every other frame)
 *  4 alt3     12 objects on one row, the game draws one in three each frame (hidden 2 frames of 3)
 *  5 budget   6 objects on one row, the game draws 4 per frame, round robin (hidden 2 frames of 6)
 *  6 random   8 objects on separate rows, each skipped with probability 1/4 per frame
 *  7 spawn    nothing hidden, but objects die and respawn elsewhere, and 4 bullets fly at 4 px per frame
 *  8 blink    3 objects plus one that blinks on purpose, 4 frames on, 4 off (an invincibility blink)
 *  9 fast     8 objects on one row moving 3 px per frame, game draws even and odd on alternate frames
 */
#include "nes.h"

#define FRAMES_PER_LAYOUT 150
#define BLANK_FRAMES 20   /* empty OAM between layouts, longer than the tracker's 8-frame gap */
#define NLAYOUTS 10
#define MAXOBJ 16
#define TRUTH ((unsigned char *)0x0500)   /* free RAM: BSS ends near $03A0, the C stack starts at $0600 */

static const unsigned char palette[32] = {
  0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,  0x0F, 0x00, 0x10, 0x30,
  0x0F, 0x16, 0x27, 0x30,  0x0F, 0x12, 0x22, 0x30,  0x0F, 0x1A, 0x2A, 0x30,  0x0F, 0x14, 0x24, 0x30,
};

static unsigned char ox[MAXOBJ], oy[MAXOBJ], alive[MAXOBJ], timer[MAXOBJ], shown[MAXOBJ];
static signed char vx[MAXOBJ], vy[MAXOBJ];
static unsigned char n, i, k, layout, t, slot, start, parity;
static unsigned char *tb;
static unsigned char seed = 0x5A;

/* 8-bit LCG (period 256); use its high bits. The runtime's rnd8() repeats every
   17 calls, and a 16-bit LFSR stepped 8 times per call cost lag frames here. */
static unsigned char rnd(void) {
  seed = seed * 5 + 17;
  return seed;
}

static void put_obj(unsigned char o) {
  unsigned char x = ox[o], y = oy[o], a = o & 3;
  OAM_Y(slot) = y;     OAM_TILE(slot) = 0x04; OAM_ATTR(slot) = a; OAM_X(slot) = x;     ++slot;
  OAM_Y(slot) = y;     OAM_TILE(slot) = 0x04; OAM_ATTR(slot) = a; OAM_X(slot) = x + 8; ++slot;
  OAM_Y(slot) = y + 8; OAM_TILE(slot) = 0x04; OAM_ATTR(slot) = a; OAM_X(slot) = x;     ++slot;
  OAM_Y(slot) = y + 8; OAM_TILE(slot) = 0x04; OAM_ATTR(slot) = a; OAM_X(slot) = x + 8; ++slot;
  shown[o] = 1;
}

/* objects on one row, 20 px apart, moving together and bouncing */
static void setup_row(unsigned char count, unsigned char y, signed char speed) {
  n = count;
  for (i = 0; i < n; ++i) { ox[i] = 8 + i * 20; oy[i] = y; vx[i] = speed; vy[i] = 0; alive[i] = 1; }
}

static void setup(void) {
  for (i = 0; i < MAXOBJ; ++i) { alive[i] = 0; timer[i] = 0; vx[i] = 0; vy[i] = 0; }
  switch (layout) {
    case 0: n = 4; for (i = 0; i < 4; ++i) { ox[i] = 20 + i * 50; oy[i] = 40 + i * 40; vx[i] = (i & 1) ? -1 : 1; alive[i] = 1; } break;
    case 1: case 2: case 5: setup_row(6, 100, 1); break;
    case 3: setup_row(8, 100, 1); break;
    case 4: setup_row(12, 100, 0); break;   /* 12 x 20 px fill the row; they stay put */
    case 6: n = 8; for (i = 0; i < 8; ++i) { ox[i] = 10 + i * 28; oy[i] = 24 + i * 24; vx[i] = (i & 1) ? -1 : 1; alive[i] = 1; } break;
    case 7:
      n = 10;
      for (i = 0; i < 6; ++i) { ox[i] = 20 + i * 36; oy[i] = 30 + (i & 1) * 50; vx[i] = (i & 1) ? -1 : 1; alive[i] = 1; timer[i] = 20 + i * 17; }
      for (i = 6; i < 10; ++i) { ox[i] = 40 + (i - 6) * 50; oy[i] = 200 - (i - 6) * 40; vy[i] = -4; alive[i] = 1; }
      break;
    case 8: n = 4; for (i = 0; i < 4; ++i) { ox[i] = 30 + i * 50; oy[i] = 60 + i * 30; vx[i] = 1; alive[i] = 1; } break;
    case 9: setup_row(8, 100, 3); break;
  }
}

static void move(void) {
  for (i = 0; i < n; ++i) {
    if (!alive[i]) {
      /* layout 7: dead objects come back 30 frames later somewhere else */
      if (timer[i] && --timer[i] == 0) { alive[i] = 1; ox[i] = 16 + (rnd() & 0x7F); oy[i] = 30 + (rnd() & 0x7F); timer[i] = 40 + (rnd() & 0x3F); }
      continue;
    }
    ox[i] += vx[i]; oy[i] += vy[i];
    if (vx[i] > 0 && ox[i] >= 232) vx[i] = -vx[i];
    if (vx[i] < 0 && ox[i] <= 8) vx[i] = -vx[i];
    if (layout == 7) {
      if (i >= 6 && oy[i] < 12) oy[i] = 208;                       /* bullets wrap bottom to top */
      if (i < 6 && --timer[i] == 0) { alive[i] = 0; timer[i] = 30; } /* enemy dies */
    }
  }
}

void main(void) {
  ppu_ctrl = 0x80;
  ppu_setup(palette);
  setup();
  for (;;) {
    parity ^= 1;
    TAG = (layout << 1) | parity;
    move();
    oam_hide_all();
    slot = 0;
    for (i = 0; i < MAXOBJ; ++i) shown[i] = 0;
    if (t >= FRAMES_PER_LAYOUT) n = 0;       /* blank gap: no objects */
    else switch (layout) {
      case 0: case 1: case 6: case 7: case 8:
        for (i = 0; i < n; ++i) {
          if (!alive[i]) continue;
          if (layout == 6 && (rnd() & 0xC0) == 0) continue;
          if (layout == 8 && i == 3 && (t & 4)) continue;
          put_obj(i);
        }
        break;
      case 2:
        for (k = 0; k < n; ++k) { i = k + start; if (i >= n) i -= n; put_obj(i); }
        if (++start == n) start = 0;
        break;
      case 3: case 9:
        for (i = parity; i < n; i += 2) put_obj(i);
        break;
      case 4:
        for (i = start; i < n; i += 3) put_obj(i);
        if (++start == 3) start = 0;
        break;
      case 5:
        for (k = 0; k < 4; ++k) { i = k + start; if (i >= n) i -= n; put_obj(i); }
        if (++start == n) start = 0;
        break;
    }
    tb = TRUTH + (parity << 6);
    tb[0] = n;
    for (i = 0; i < n; ++i) { tb[1 + i] = shown[i] | (alive[i] << 1); tb[17 + i] = ox[i]; tb[33 + i] = oy[i]; }
    oam_ready();
    wait_frame();
    if (++t == FRAMES_PER_LAYOUT + BLANK_FRAMES) { t = 0; start = 0; if (++layout == NLAYOUTS) layout = 0; setup(); }
  }
}
