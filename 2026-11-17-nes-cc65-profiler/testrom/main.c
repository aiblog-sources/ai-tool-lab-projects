/* nesprof validation ROM (MIT).
 * For LOOPS game loops, loop k calls burn400() SINGLES[k % 16] times and
 * burn_pair() PAIRS[k % 16] times, then waits for the next frame. After that
 * it only waits. The loops with the most calls take longer than a frame
 * (29,780 cycles), so the NMI comes before the loop ends and the ROM's lag
 * counter ($07F6) goes up: the profiler must see the same slow loops.
 * tools/validate.py computes the exact cycle counts from the same tables.
 */
#include "nes.h"

void burn400(void);
void burn_pair(void);

#define LOOPS 64
static const unsigned char SINGLES[16] = {0, 10, 20, 40, 60, 70, 72, 80, 5, 0, 90, 30, 1, 2, 64, 100};
static const unsigned char PAIRS[16]   = {0,  1,  2,  4,  6,  0,  3,  0, 30, 35, 0, 10, 0, 1, 2, 20};

static const unsigned char palette[32] = {
  0x0F, 0x10, 0x20, 0x30, 0x0F, 0x10, 0x20, 0x30, 0x0F, 0x10, 0x20, 0x30, 0x0F, 0x10, 0x20, 0x30,
  0x0F, 0x16, 0x27, 0x30, 0x0F, 0x16, 0x27, 0x30, 0x0F, 0x16, 0x27, 0x30, 0x0F, 0x16, 0x27, 0x30
};

void main(void) {
  static unsigned char k, i;
  ppu_setup(palette);
  oam_hide_all();
  LAG = 0;
  for (k = 0; k < LOOPS; ++k) {
    for (i = SINGLES[k & 15]; i; --i) burn400();
    for (i = PAIRS[k & 15]; i; --i) burn_pair();
    DEBUG0 = k + 1;           /* loops finished */
    oam_ready();
    wait_frame();
  }
  for (;;) { oam_ready(); wait_frame(); }
}
