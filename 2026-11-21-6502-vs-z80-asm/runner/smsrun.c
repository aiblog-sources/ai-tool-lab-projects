/*
 * smsrun: runs one Z80 routine against a file of test vectors on the Genesis
 * Plus GX libretro core (Master System mode), headless, and writes what came
 * out of every call.
 *
 * No emulator patch: like smsvdp (2026-11-19), it wraps the core's exported
 * z80_writeport. The harness ROM loops `out ($DE),a` / `call routine` /
 * `out ($DF),a`. On the $DE write, smsrun loads the next vector into
 * A F B C D E H L, sets IX and IY to junk, fills $C000-$C00F and the buffer
 * $C100-$C13F, and notes the cycle count. On the $DF write it records the
 * registers, those memory areas, the cycles of CALL + routine + RET (the
 * core stamps a port write at the end of its OUT, so the second OUT's 11
 * cycles are taken off) and whether anything else changed:
 *   status bit 1  timeout: no return within --limit cycles (the core is reset)
 *   status bit 2  SP differs after the call
 *   status bit 4  RAM outside $C000-$C00F, the buffer and the stack area ($D000-$DFEF) changed
 *                 ($C010-$CFFF outside the buffer is filled with a per-vector pattern before each call)
 *
 *   smsrun core.so harness.sms in.bin out.bin [--limit N]
 * The stack area was $DF00-$DFEF in the first run; a counting sort that put its
 * 256-byte table on the stack (allowed by the prompt) showed that was too tight.
 * Records: in = 88 bytes (regs[8] = A F B C D E H L, args[16], buf[64]);
 * out = 96 bytes (the same 88 after the call, then cycles u32 little-endian,
 * status, first stray address hi/lo, 0). MIT.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"
#define __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H
#define INLINE static inline
#include "shared.h" /* Genesis Plus GX internals: Z80, work_ram, z80_writeport, lines_per_frame */

#define INREC 88
#define OUTREC 96
#define MCLK_PER_Z80 15
#define MCLK_PER_LINE 3420
#define ARGS 0x0000   /* offsets into work_ram, which is mirrored at $C000 */
#define BUF 0x0100
#define BUFLEN 64

static uint8_t *vin; static long nvec, cur;
static FILE *outf;
static int in_call, need_reset, done;
static int64_t frame_base, start;
static uint64_t limit = 2000000;
static uint8_t snap[0x2000];
static uint16_t sp_before;
static long n_timeout;

static void (*orig_w)(unsigned int, unsigned char);

static int64_t now(void) { return frame_base + (int64_t)Z80.cycles; }

static int allowed(unsigned a) {
  return a < 0x10 || (a >= BUF && a < BUF + BUFLEN) || (a >= 0x1000 && a < 0x1FF0);
}

static void finish(uint32_t cycles, uint8_t status) {
  uint8_t r[OUTREC] = {0};
  r[0] = Z80.af.b.h; r[1] = Z80.af.b.l; r[2] = Z80.bc.b.h; r[3] = Z80.bc.b.l;
  r[4] = Z80.de.b.h; r[5] = Z80.de.b.l; r[6] = Z80.hl.b.h; r[7] = Z80.hl.b.l;
  memcpy(r + 8, work_ram + ARGS, 16);
  memcpy(r + 24, work_ram + BUF, BUFLEN);
  if (Z80.sp.w.l != sp_before) status |= 2;
  for (unsigned a = 0; a < 0x2000; a++)
    if (!allowed(a) && work_ram[a] != snap[a]) { status |= 4; unsigned z = 0xC000 + a; r[93] = z >> 8; r[94] = z & 0xFF; break; }
  memcpy(r + 88, &cycles, 4);
  r[92] = status;
  fwrite(r, 1, OUTREC, outf);
  in_call = 0; cur++;
}

static void port_w(unsigned int port, unsigned char data) {
  if ((port & 0xFF) == 0xDE) {
    if (done || need_reset) return;
    if (cur >= nvec) { done = 1; return; }
    const uint8_t *v = vin + cur * INREC;
    Z80.af.b.h = v[0]; Z80.af.b.l = v[1]; Z80.bc.b.h = v[2]; Z80.bc.b.l = v[3];
    Z80.de.b.h = v[4]; Z80.de.b.l = v[5]; Z80.hl.b.h = v[6]; Z80.hl.b.l = v[7];
    Z80.ix.w.l = 0x5A5A; Z80.iy.w.l = 0xA5A5;
    memcpy(work_ram + ARGS, v + 8, 16);
    memcpy(work_ram + BUF, v + 24, BUFLEN);
    /* fill the RAM the routine must not touch with a pattern that changes
       per vector, so a stray write shows up even when it stores the same value */
    uint8_t fill = (uint8_t)(cur * 37 + 0x5A);
    for (unsigned a = 0; a < 0x1000; a++) if (!allowed(a)) work_ram[a] = fill;
    memcpy(snap, work_ram, 0x2000);
    sp_before = Z80.sp.w.l;
    start = now(); in_call = 1;
    return;
  }
  if ((port & 0xFF) == 0xDF) {
    if (in_call && !need_reset) finish((uint32_t)((now() - start) / MCLK_PER_Z80 - 11), 0);
    return;
  }
  orig_w(port, data);
}

static bool env_cb(unsigned cmd, void *data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)data = "/tmp"; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)data = true; return true;
    default: return false;
  }
}
static void video_cb(const void *d, unsigned w, unsigned h, size_t p) { (void)d; (void)w; (void)h; (void)p; }
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t f) { (void)d; return f; }
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned p, unsigned d, unsigned i, unsigned id) { (void)p; (void)d; (void)i; (void)id; return 0; }

#define LOAD(name) do { *(void **)&p_##name = dlsym(core, #name); \
  if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; } } while (0)

static uint8_t *slurp(const char *path, long *size) {
  FILE *f = fopen(path, "rb");
  if (!f) { perror(path); exit(1); }
  fseek(f, 0, SEEK_END); *size = ftell(f); fseek(f, 0, SEEK_SET);
  uint8_t *b = malloc(*size ? *size : 1);
  if (fread(b, 1, *size, f) != (size_t)*size) { fprintf(stderr, "short read %s\n", path); exit(1); }
  fclose(f);
  return b;
}

int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "usage: %s core.so rom.sms in.bin out.bin [--limit N]\n", argv[0]); return 2; }
  for (int i = 5; i < argc; i++)
    if (!strcmp(argv[i], "--limit") && i + 1 < argc) limit = strtoull(argv[++i], NULL, 10);
  long rom_size, in_size;
  uint8_t *rom = slurp(argv[2], &rom_size);
  vin = slurp(argv[3], &in_size);
  nvec = in_size / INREC;
  outf = fopen(argv[4], "wb");
  if (!outf) { perror(argv[4]); return 1; }

  void *core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!core) { fprintf(stderr, "%s\n", dlerror()); return 1; }
  void (*p_retro_set_environment)(retro_environment_t);
  void (*p_retro_set_video_refresh)(retro_video_refresh_t);
  void (*p_retro_set_audio_sample)(retro_audio_sample_t);
  void (*p_retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
  void (*p_retro_set_input_poll)(retro_input_poll_t);
  void (*p_retro_set_input_state)(retro_input_state_t);
  void (*p_retro_init)(void);
  bool (*p_retro_load_game)(const struct retro_game_info *);
  void (*p_retro_run)(void);
  void (*p_retro_reset)(void);
  LOAD(retro_set_environment); LOAD(retro_set_video_refresh); LOAD(retro_set_audio_sample);
  LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
  LOAD(retro_init); LOAD(retro_load_game); LOAD(retro_run); LOAD(retro_reset);

  p_retro_set_environment(env_cb);
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  p_retro_init();
  struct retro_game_info gi = {argv[2], rom, (size_t)rom_size, NULL};
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  if (system_hw != SYSTEM_SMS && system_hw != SYSTEM_SMS2) {
    fprintf(stderr, "not running as a Master System (system_hw 0x%x); name the ROM .sms\n", system_hw);
    return 1;
  }
  orig_w = z80_writeport; z80_writeport = port_w;

  long frames = 0, max_frames = 2000000;
  while (!done && frames < max_frames) {
    p_retro_run(); frames++;
    frame_base += (int64_t)lines_per_frame * MCLK_PER_LINE;
    if (in_call && !need_reset && (uint64_t)((now() - start) / MCLK_PER_Z80) > limit) {
      finish((uint32_t)((now() - start) / MCLK_PER_Z80), 1); n_timeout++; need_reset = 1;
    }
    if (need_reset) {
      p_retro_reset();
      orig_w = z80_writeport == port_w ? orig_w : z80_writeport; z80_writeport = port_w;
      need_reset = 0; in_call = 0;
    }
  }
  fclose(outf);
  fprintf(stderr, "smsrun: %ld of %ld vectors, %ld timeouts, %ld frames\n", cur, nvec, n_timeout, frames);
  return cur == nvec ? 0 : 1;
}
