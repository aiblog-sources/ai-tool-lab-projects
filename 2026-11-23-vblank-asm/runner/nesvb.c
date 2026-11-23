/*
 * nesvb: runs an NES ROM headless on the FCEUmm libretro core and times every
 * PPU register write made inside the NMI handler against the end of vertical
 * blank.
 *
 * The core is the one from the 2026-11-17 nesprof project (FCEUmm with a
 * running cycle counter and a hook before every instruction). The hook sees
 * the PC and the opcode; for the store instructions that can reach a PPU
 * register (STA/STX/STY absolute, STA abs,X / abs,Y / (zp),Y / (zp,X)) it
 * works out the target address from the operand bytes in PRG ROM and the
 * index registers, and logs writes to $2000-$3FFF (mirrors folded to
 * $2000-$2007) and to $4014 (OAM DMA). A write lands on the last cycle of its
 * instruction; an OAM DMA keeps the bus 513 more cycles (the emulator counts
 * 512; the post uses the console's 513).
 *
 * Times are CPU cycles from the first instruction of the NMI handler (the
 * address NMI from the ld65 label file). The end of vertical blank is found
 * from the emulator itself: FCEUmm sets `scanline` to 0 one full scanline
 * (341 PPU dots) after the pre-render line starts, i.e. after vblank ends,
 * so vblank end = (first instruction seen with scanline 0) - 341/3 cycles.
 *
 *   nesvb core.so rom.nes NMI NMIEXIT out_prefix [--frames N] [--from F]
 *
 * Writes out_prefix.json (per-frame summary), out_prefix-writes.csv (every
 * logged write in frames >= F), out_prefix-vram.bin (2 KB nametable RAM,
 * 32 bytes palette, 256 bytes OAM, in that order) and out_prefix.ppm (the
 * last frame). MIT.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"

typedef struct { int32_t tcount; uint16_t PC; uint8_t A, X, Y, S, P, mooPI; uint8_t jammed; } X6502;

static X6502 *cpu;
static uint64_t *cyc;
static uint8_t *ram, *ntaram, *palram, *spram;
static int *scanline_p;
static uint8_t *rom; static long rom_size;
static uint16_t nmi_pc, exit_pc;
static FILE *wcsv;
static int cur_frame, from_frame;

#define MAXF 4096
typedef struct {
  int entered, exited, reentered;
  int64_t entry, exit_rel, last_write_rel, work_end_rel, vbl_end_rel;
  int writes, dma, late, rmw_ppu;
} frame_t;
static frame_t fr[MAXF];
static int in_nmi, cur_nmi_frame = -1, seen_scan0 = 1;
static int64_t entry_cyc;

static uint8_t prg(uint16_t a) {
  long off = 16 + (long)(a - 0x8000) % 0x8000;
  return off < rom_size ? rom[off] : 0xFF;
}
static uint8_t rd(uint16_t a) { return a < 0x2000 ? ram[a & 0x7FF] : a >= 0x8000 ? prg(a) : 0xFF; }

static void log_write(uint16_t addr, uint8_t val, int len) {
  frame_t *f = &fr[cur_nmi_frame];
  int64_t t = (int64_t)(*cyc - entry_cyc) + len;   /* the write is on the last cycle */
  int64_t end = t;
  if (addr == 0x4014) { end = t + 513; f->dma++; }
  f->writes++;
  if (t > f->last_write_rel) f->last_write_rel = t;
  if (end > f->work_end_rel) f->work_end_rel = end;
  if (wcsv && cur_nmi_frame >= from_frame)
    fprintf(wcsv, "%d,%lld,%04X,%02X,%04X\n", cur_nmi_frame, (long long)t, addr, val, cpu->PC);
}

static void hook(uint16_t pc, uint8_t op, uint8_t s) {
  (void)s;
  int sl = *scanline_p;
  if (sl == 0 && !seen_scan0) {
    seen_scan0 = 1;
    if (cur_nmi_frame >= 0 && cur_nmi_frame < MAXF && fr[cur_nmi_frame].entered && !fr[cur_nmi_frame].vbl_end_rel)
      fr[cur_nmi_frame].vbl_end_rel = (int64_t)(*cyc - entry_cyc) - 114;  /* 341 dots = 113.67 cycles */
  }
  if (pc == nmi_pc) {
    if (in_nmi && cur_nmi_frame >= 0) fr[cur_nmi_frame].reentered = 1;
    if (cur_frame < MAXF) {
      cur_nmi_frame = cur_frame;
      /* frames are counted per retro_run; one NMI per frame */
      frame_t *f = &fr[cur_nmi_frame];
      memset(f, 0, sizeof *f);
      f->entered = 1; f->entry = (int64_t)*cyc;
      entry_cyc = *cyc; in_nmi = 1; seen_scan0 = 0;
    }
    return;
  }
  if (!in_nmi || cur_nmi_frame < 0) return;
  if (pc == exit_pc) {
    fr[cur_nmi_frame].exited = 1;
    fr[cur_nmi_frame].exit_rel = (int64_t)(*cyc - entry_cyc);
    in_nmi = 0;
    return;
  }
  uint16_t a = 0; int len = 0; uint8_t v = 0;
  uint16_t ab = prg(pc + 1) | (prg(pc + 2) << 8);
  switch (op) {
    case 0x8D: a = ab; len = 4; v = cpu->A; break;
    case 0x8E: a = ab; len = 4; v = cpu->X; break;
    case 0x8C: a = ab; len = 4; v = cpu->Y; break;
    case 0x9D: a = ab + cpu->X; len = 5; v = cpu->A; break;
    case 0x99: a = ab + cpu->Y; len = 5; v = cpu->A; break;
    case 0x91: { uint8_t z = prg(pc + 1); a = (rd(z) | (rd((uint8_t)(z + 1)) << 8)) + cpu->Y; len = 6; v = cpu->A; break; }
    case 0x81: { uint8_t z = prg(pc + 1) + cpu->X; a = rd(z) | (rd((uint8_t)(z + 1)) << 8); len = 6; v = cpu->A; break; }
    /* read-modify-write on absolute addresses: flag if aimed at the PPU */
    case 0xEE: case 0xCE: case 0x0E: case 0x4E: case 0x2E: case 0x6E:
      if (ab >= 0x2000 && ab < 0x4000) fr[cur_nmi_frame].rmw_ppu++;
      return;
    default: return;
  }
  if (a >= 0x2000 && a < 0x4000) log_write(0x2000 | (a & 7), v, len);
  else if (a == 0x4014) log_write(a, v, len);
}

static bool env_cb(unsigned cmd, void *data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: *(const char **)data = "/tmp"; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE: *(bool *)data = true; return true;
    default: return false;
  }
}
static uint32_t fb[256 * 240]; static unsigned fbw, fbh;
static void video_cb(const void *d, unsigned w, unsigned h, size_t p) {
  if (!d || w > 256 || h > 240) return;
  fbw = w; fbh = h;
  for (unsigned y = 0; y < h; y++) memcpy(fb + y * w, (const uint8_t *)d + y * p, w * 4);
}
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t f) { (void)d; return f; }
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned p, unsigned d, unsigned i, unsigned id) { (void)p; (void)d; (void)i; (void)id; return 0; }

#define LOAD(name) do { *(void **)&p_##name = dlsym(core, #name); \
  if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; } } while (0)
#define SYM(var, name) do { var = dlsym(core, name); if (!var) { fprintf(stderr, "missing %s\n", name); return 1; } } while (0)

int main(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "usage: %s core.so rom.nes NMI NMIEXIT out_prefix [--frames N] [--from F]\n", argv[0]); return 2; }
  nmi_pc = (uint16_t)strtoul(argv[3], NULL, 16);
  exit_pc = (uint16_t)strtoul(argv[4], NULL, 16);
  const char *out = argv[5];
  int frames = 60;
  for (int i = 6; i < argc; i++) {
    if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--from") && i + 1 < argc) from_frame = atoi(argv[++i]);
  }
  if (frames > MAXF) frames = MAXF;
  FILE *rf = fopen(argv[2], "rb");
  if (!rf) { perror(argv[2]); return 1; }
  fseek(rf, 0, SEEK_END); rom_size = ftell(rf); fseek(rf, 0, SEEK_SET);
  rom = malloc(rom_size);
  if (fread(rom, 1, rom_size, rf) != (size_t)rom_size) return 1;
  fclose(rf);
  char path[1024];
  snprintf(path, sizeof path, "%s-writes.csv", out);
  wcsv = fopen(path, "w");
  if (wcsv) fprintf(wcsv, "frame,cycle,reg,value,pc\n");

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
  void *(*p_retro_get_memory_data)(unsigned);
  LOAD(retro_set_environment); LOAD(retro_set_video_refresh); LOAD(retro_set_audio_sample);
  LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
  LOAD(retro_init); LOAD(retro_load_game); LOAD(retro_run); LOAD(retro_get_memory_data);
  void (**hookp)(uint16_t, uint8_t, uint8_t);
  SYM(cyc, "nesprof_cycles"); SYM(hookp, "nesprof_hook"); SYM(cpu, "X");
  SYM(ntaram, "NTARAM"); SYM(palram, "PALRAM"); SYM(spram, "SPRAM"); SYM(scanline_p, "scanline");

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

  for (cur_frame = 0; cur_frame < frames; cur_frame++) p_retro_run();
  if (wcsv) fclose(wcsv);

  snprintf(path, sizeof path, "%s-vram.bin", out);
  FILE *vf = fopen(path, "wb");
  fwrite(ntaram, 1, 0x800, vf); fwrite(palram, 1, 0x20, vf); fwrite(spram, 1, 0x100, vf);
  fclose(vf);
  snprintf(path, sizeof path, "%s.ppm", out);
  FILE *pf = fopen(path, "wb");
  fprintf(pf, "P6\n%u %u\n255\n", fbw, fbh);
  for (unsigned i = 0; i < fbw * fbh; i++) { uint8_t px[3] = { fb[i] >> 16, fb[i] >> 8, fb[i] }; fwrite(px, 1, 3, pf); }
  fclose(pf);

  snprintf(path, sizeof path, "%s.json", out);
  FILE *jf = fopen(path, "w");
  fprintf(jf, "{\"frames\":[");
  for (int i = 0; i < frames; i++) {
    frame_t *f = &fr[i];
    fprintf(jf, "%s\n{\"f\":%d,\"entered\":%d,\"exited\":%d,\"reentered\":%d,\"exit\":%lld,\"last_write\":%lld,"
            "\"work_end\":%lld,\"vbl_end\":%lld,\"writes\":%d,\"dma\":%d,\"rmw_ppu\":%d}",
            i ? "," : "", i, f->entered, f->exited, f->reentered, (long long)f->exit_rel, (long long)f->last_write_rel,
            (long long)f->work_end_rel, (long long)f->vbl_end_rel, f->writes, f->dma, f->rmw_ppu);
  }
  fprintf(jf, "]}\n");
  fclose(jf);
  return 0;
}
