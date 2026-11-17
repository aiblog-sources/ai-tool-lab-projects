/* cc65 cost ROM (MIT): the same job written several ways, one function per
 * way, each called once per game loop. nesprof's inclusive cycles per call,
 * divided by 64, give the cost per sprite (or per value) of each way.
 * Compiled like the scenes: cl65 -t nes -Oirs, with nes.h / nes.c.
 */
#include "nes.h"

static unsigned char xs[64], ys[64], tiles[64], vals[64], out[64];
static int ixs[64], iys[64];

/* 1. the boss scene's helper: int coordinates, OAM_*(i) macros */
static void put_int(unsigned char i, int x, int y, unsigned char tile, unsigned char attr) {
  OAM_Y(i) = (unsigned char)(y - 1);
  OAM_TILE(i) = tile;
  OAM_ATTR(i) = attr;
  OAM_X(i) = (unsigned char)x;
}
void fill_int_args(void) {
  unsigned char i;
  for (i = 0; i < 64; ++i) put_int(i, ixs[i], iys[i], tiles[i], 0);
}

/* 2. the same helper with unsigned char arguments */
static void put_u8(unsigned char i, unsigned char x, unsigned char y, unsigned char tile, unsigned char attr) {
  OAM_Y(i) = y - 1;
  OAM_TILE(i) = tile;
  OAM_ATTR(i) = attr;
  OAM_X(i) = x;
}
void fill_u8_args(void) {
  unsigned char i;
  for (i = 0; i < 64; ++i) put_u8(i, xs[i], ys[i], tiles[i], 0);
}

/* 3. no helper: OAM_*(i) macros inline in the loop */
void fill_macros_inline(void) {
  unsigned char i;
  for (i = 0; i < 64; ++i) {
    OAM_Y(i) = ys[i] - 1;
    OAM_TILE(i) = tiles[i];
    OAM_ATTR(i) = 0;
    OAM_X(i) = xs[i];
  }
}

/* 3b. the same macros with the offset cast back to a byte (a possible nes.h fix) */
#define OAMB_Y(i)    OAM[(unsigned char)((i) << 2)]
#define OAMB_TILE(i) OAM[(unsigned char)(((i) << 2) + 1)]
#define OAMB_ATTR(i) OAM[(unsigned char)(((i) << 2) + 2)]
#define OAMB_X(i)    OAM[(unsigned char)(((i) << 2) + 3)]
void fill_macros_cast(void) {
  unsigned char i;
  for (i = 0; i < 64; ++i) {
    OAMB_Y(i) = ys[i] - 1;
    OAMB_TILE(i) = tiles[i];
    OAMB_ATTR(i) = 0;
    OAMB_X(i) = xs[i];
  }
}

/* 4. inline, with an 8-bit OAM offset instead of (i) << 2 */
void fill_byte_offset(void) {
  unsigned char i, o = 0;
  for (i = 0; i < 64; ++i) {
    OAM[o] = ys[i] - 1;
    OAM[o + 1] = tiles[i];
    OAM[o + 2] = 0;
    OAM[o + 3] = xs[i];
    o += 4;
  }
}

/* 5. as 4, with static (global) loop variables instead of locals */
static unsigned char si, so;
void fill_static_vars(void) {
  so = 0;
  for (si = 0; si < 64; ++si) {
    OAM[so] = ys[si] - 1;
    OAM[(unsigned char)(so + 1)] = tiles[si];
    OAM[(unsigned char)(so + 2)] = 0;
    OAM[(unsigned char)(so + 3)] = xs[si];
    so += 4;
  }
}

/* arithmetic on 64 values */
void div10_u8(void)   { unsigned char i; for (i = 0; i < 64; ++i) out[i] = vals[i] / 10; }
void mod8_u8(void)    { unsigned char i; for (i = 0; i < 64; ++i) out[i] = vals[i] % 8; }
void and7_u8(void)    { unsigned char i; for (i = 0; i < 64; ++i) out[i] = vals[i] & 7; }
void mul12_u8(void)   { unsigned char i; for (i = 0; i < 64; ++i) out[i] = vals[i] * 12; }
void shift12_u8(void) { unsigned char i; for (i = 0; i < 64; ++i) out[i] = (vals[i] << 3) + (vals[i] << 2); }
void copy_u8(void)    { unsigned char i; for (i = 0; i < 64; ++i) out[i] = vals[i]; }

void main(void) {
  unsigned char i, n;
  for (i = 0; i < 64; ++i) {
    xs[i] = i * 3; ys[i] = 16 + i; tiles[i] = i & 7; vals[i] = rnd8();
    ixs[i] = xs[i]; iys[i] = ys[i];
  }
  ppu_setup((const unsigned char *)"\x0f\x10\x20\x30\x0f\x10\x20\x30\x0f\x10\x20\x30\x0f\x10\x20\x30"
                                   "\x0f\x16\x27\x30\x0f\x16\x27\x30\x0f\x16\x27\x30\x0f\x16\x27\x30");
  for (n = 0; n < 40; ++n) {   /* 40 loops, then only wait, so the totals hold whole calls */
    fill_int_args();
    fill_u8_args();
    fill_macros_inline();
    fill_macros_cast();
    fill_byte_offset();
    fill_static_vars();
    div10_u8(); mod8_u8(); and7_u8(); mul12_u8(); shift12_u8(); copy_u8();
    oam_ready();
    wait_frame();
  }
  for (;;) wait_frame();
}
