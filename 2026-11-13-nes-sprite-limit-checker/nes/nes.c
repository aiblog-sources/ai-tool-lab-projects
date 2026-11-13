/* MIT. See nes.h. */
#include "nes.h"

#define PPUCTRL   (*(volatile unsigned char *)0x2000)
#define PPUMASK   (*(volatile unsigned char *)0x2001)
#define PPUSTATUS (*(volatile unsigned char *)0x2002)
#define PPUADDR   (*(volatile unsigned char *)0x2006)
#define PPUDATA   (*(volatile unsigned char *)0x2007)
#define JOY1      (*(volatile unsigned char *)0x4016)

void wait_frame(void) {
  unsigned char n = NMI_COUNT;
  while (NMI_COUNT == n) {}
}

void oam_ready(void) {
  OAM_READY = 1;
}

unsigned char pad_read(void) {
  unsigned char i, v = 0;
  JOY1 = 1;
  JOY1 = 0;
  for (i = 0; i < 8; ++i) v = (v << 1) | (JOY1 & 1);
  return v;
}

void ppu_setup(const unsigned char *pal) {
  unsigned int i;
  PPUCTRL = 0;
  PPUMASK = 0;
  (void)PPUSTATUS;
  PPUADDR = 0x3F; PPUADDR = 0x00;
  for (i = 0; i < 32; ++i) PPUDATA = pal[i];
  PPUADDR = 0x20; PPUADDR = 0x00;
  for (i = 0; i < 0x800; ++i) PPUDATA = 0;
  (void)PPUSTATUS;
  PPUADDR = 0; PPUADDR = 0;
  PPUCTRL = ppu_ctrl;  /* NMI back on; the NMI turns rendering on with ppu_mask */
  wait_frame();
}

void oam_hide_all(void) {
  /* all four bytes, not just Y: the PPU's overflow check also reads tile,
     attribute and X bytes of entries after the eighth hit as if they were Y */
  unsigned char i = 0;
  do { OAM[i] = 0xFF; } while (++i);
}

static unsigned char seed = 0x5A;
unsigned char rnd8(void) {
  seed ^= seed << 3;
  seed ^= seed >> 5;
  seed ^= seed << 1;
  return seed;
}
