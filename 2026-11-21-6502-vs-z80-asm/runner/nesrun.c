/*
 * nesrun: runs one 6502 routine against a file of test vectors on the FCEUmm
 * libretro core (NES), headless, and writes what came out of every call.
 *
 * The core is the one from the 2026-11-17 nesprof project (FCEUmm with a
 * running cycle counter and a hook called before every instruction). The
 * harness ROM is a loop: `call: JSR routine` / `after: JMP call`. When the
 * CPU reaches `call`, nesrun loads the next vector into A, X, Y, P, zero page
 * $00-$0F and the buffer $0300-$033F, and notes the cycle counter. When it
 * reaches `after`, it records the registers, those memory areas, the cycles
 * the call took (JSR + routine + RTS) and whether anything else changed:
 *   status bit 1  timeout: no return within --limit cycles (the core is reset)
 *   status bit 2  stack pointer differs after the call
 *   status bit 4  RAM outside $00-$0F, the stack page and the buffer changed
 *                 (that RAM is filled with a per-vector pattern before each call)
 *
 *   nesrun core.so harness.nes CALL AFTER in.bin out.bin [--limit N]
 * CALL and AFTER are hex addresses (from the ld65 label file).
 * Records: in = 88 bytes (regs[8] = A X Y P . . . ., args[16], buf[64]);
 * out = 96 bytes (the same 88 after the call, with regs[4] = S, then cycles
 * u32 little-endian, status, first stray address hi/lo, 0). MIT.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

#define INREC 88
#define OUTREC 96
#define BUF 0x300
#define BUFLEN 64

typedef struct { int32_t tcount; uint16_t PC; uint8_t A, X, Y, S, P, mooPI; uint8_t jammed; } X6502;

static X6502 *cpu;
static uint64_t *cyc;
static uint8_t *ram;
static uint16_t call_pc, after_pc;
static uint8_t *vin; static long nvec, cur;
static FILE *outf;
static int in_call, need_reset, done;
static uint64_t start, limit = 2000000;
static uint8_t snap[0x800], s_before;
static long n_timeout;

static int allowed(unsigned a) {
  return a < 0x10 || (a >= 0x100 && a < 0x200) || (a >= BUF && a < BUF + BUFLEN);
}

static void finish(uint32_t cycles, uint8_t status) {
  uint8_t r[OUTREC] = {0};
  r[0] = cpu->A; r[1] = cpu->X; r[2] = cpu->Y; r[3] = cpu->P; r[4] = cpu->S;
  memcpy(r + 8, ram, 16);
  memcpy(r + 24, ram + BUF, BUFLEN);
  if (cpu->S != s_before) status |= 2;
  for (unsigned a = 0; a < 0x800; a++)
    if (!allowed(a) && ram[a] != snap[a]) { status |= 4; r[93] = a >> 8; r[94] = a & 0xFF; break; }
  memcpy(r + 88, &cycles, 4);
  r[92] = status;
  fwrite(r, 1, OUTREC, outf);
  in_call = 0; cur++;
}

static void hook(uint16_t pc, uint8_t op, uint8_t s) {
  (void)op; (void)s;
  if (done || need_reset) return;
  if (in_call && pc == after_pc) { finish((uint32_t)(*cyc - start), 0); return; }
  if (in_call && *cyc - start > limit) {
    /* a timeout: keep only the registers and memory as they are now */
    finish((uint32_t)(*cyc - start), 1); need_reset = 1; n_timeout++; return;
  }
  if (!in_call && pc == call_pc) {
    if (cur >= nvec) { done = 1; return; }
    const uint8_t *v = vin + cur * INREC;
    cpu->A = v[0]; cpu->X = v[1]; cpu->Y = v[2];
    cpu->P = (v[3] & 0xC3) | 0x24;   /* N V Z C from the vector; I and the unused bit set; D clear */
    memcpy(ram, v + 8, 16);
    memcpy(ram + BUF, v + 24, BUFLEN);
    /* fill the RAM the routine must not touch with a pattern that changes
       per vector, so a stray write shows up even when it stores the same value */
    uint8_t fill = (uint8_t)(cur * 37 + 0x5A);
    for (unsigned a = 0x10; a < 0x100; a++) ram[a] = fill;
    for (unsigned a = 0x200; a < 0x800; a++) if (!allowed(a)) ram[a] = fill;
    memcpy(snap, ram, 0x800);
    s_before = cpu->S;
    start = *cyc; in_call = 1;
  }
}

static bool env_cb(unsigned cmd, void *data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)data = "/tmp"; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
      enum retro_pixel_format f = *(enum retro_pixel_format *)data;
      return f == RETRO_PIXEL_FORMAT_RGB565 || f == RETRO_PIXEL_FORMAT_XRGB8888;
    }
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
  if (argc < 7) { fprintf(stderr, "usage: %s core.so rom.nes CALL AFTER in.bin out.bin [--limit N]\n", argv[0]); return 2; }
  call_pc = (uint16_t)strtoul(argv[3], NULL, 16);
  after_pc = (uint16_t)strtoul(argv[4], NULL, 16);
  for (int i = 7; i < argc; i++)
    if (!strcmp(argv[i], "--limit") && i + 1 < argc) limit = strtoull(argv[++i], NULL, 10);
  long rom_size, in_size;
  uint8_t *rom = slurp(argv[2], &rom_size);
  vin = slurp(argv[5], &in_size);
  nvec = in_size / INREC;
  outf = fopen(argv[6], "wb");
  if (!outf) { perror(argv[6]); return 1; }

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
  void *(*p_retro_get_memory_data)(unsigned);
  LOAD(retro_set_environment); LOAD(retro_set_video_refresh); LOAD(retro_set_audio_sample);
  LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
  LOAD(retro_init); LOAD(retro_load_game); LOAD(retro_run); LOAD(retro_reset); LOAD(retro_get_memory_data);
  cyc = dlsym(core, "nesprof_cycles");
  void (**hookp)(uint16_t, uint8_t, uint8_t) = dlsym(core, "nesprof_hook");
  cpu = dlsym(core, "X");
  if (!cyc || !hookp || !cpu) { fprintf(stderr, "core was not built with fceumm-nesprof.patch (or X is not exported)\n"); return 1; }

  p_retro_set_environment(env_cb);
  p_retro_init();
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  struct retro_game_info gi = { argv[2], rom, (size_t)rom_size, NULL };
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
  *hookp = hook;

  long frames = 0, max_frames = 2000000;
  while (!done && frames < max_frames) {
    p_retro_run(); frames++;
    if (need_reset) { p_retro_reset(); need_reset = 0; in_call = 0; }
  }
  fclose(outf);
  fprintf(stderr, "nesrun: %ld of %ld vectors, %ld timeouts, %ld frames\n", cur, nvec, n_timeout, frames);
  return cur == nvec ? 0 : 1;
}
