/*
 * smsvb: runs a Master System ROM headless on the Genesis Plus GX libretro
 * core and times every VDP port write made inside the VBlank interrupt
 * handler against the end of vertical blank.
 *
 * No emulator patch: like smsvdp (2026-11-19) it wraps the core's exported
 * z80_writeport. The harness ISR writes port $DE as its first instruction and
 * port $DF just before EI/RETI; those two writes mark the handler. The core
 * starts each SMS frame at line 192 (the first line after the active
 * display), stamps a port write at the end of its OUT, and counts Z80.cycles
 * in master clocks (15 per Z80 cycle, 3420 per line). So the active display
 * starts again (lines_per_frame - 192) * 228 Z80 cycles into the frame.
 *
 * For each frame: when the handler started, when it ended, the last VDP write,
 * the writes that landed during active display (display enabled and line <
 * 192) and how many of those came less than 26 cycles after the previous data
 * access (the figure from SMS Power testing that smsvdp used).
 *
 *   smsvb core.so rom.sms out_prefix [--frames N] [--from F]
 *
 * Writes out_prefix.json, out_prefix-writes.csv, out_prefix-vram.bin (16 KB
 * VRAM then the core's 128-byte CRAM copy) and out_prefix.ppm (last frame). MIT.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libretro.h"
#define __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H
#define INLINE static inline
#include "shared.h" /* Genesis Plus GX internals: Z80, reg, vram, cram, lines_per_frame, bitmap */

#define MCLK_PER_Z80 15
#define MAXF 4096

typedef struct {
  int entered, exited, reentered;
  int64_t entry, exit, last_write;
  int writes, data, active, fast26;
} frame_t;
static frame_t fr[MAXF];
static int cur_frame, from_frame, in_isr;
static int64_t last_data = -1000000;
static FILE *wcsv;
static void (*orig_w)(unsigned int, unsigned char);

static int64_t cyc(void) { return (int64_t)Z80.cycles / MCLK_PER_Z80; }

static int isr_frame;
static void port_w(unsigned int port, unsigned char data) {
  int p = port & 0xFF;
  int64_t abs_t = (int64_t)cur_frame * lines_per_frame * 228 + cyc();
  if (p == 0xDE) {
    if (in_isr) fr[isr_frame].reentered = 1;
    isr_frame = cur_frame < MAXF ? cur_frame : MAXF - 1;
    fr[isr_frame].entered = 1; fr[isr_frame].entry = cyc(); in_isr = 1;
  } else if (in_isr) {
    frame_t *f = &fr[isr_frame];
    /* times relative to the start of the frame the handler started in */
    int64_t t = abs_t - (int64_t)isr_frame * lines_per_frame * 228;
    if (p == 0xDF) { f->exited = 1; f->exit = t; in_isr = 0; }
    else if ((p & 0xC0) == 0x80) {
      int is_data = (p & 1) == 0;
      int line = (192 + (int)(cyc() / 228)) % lines_per_frame;
      int active = (reg[1] & 0x40) && line < 192;
      int gap = (int)(abs_t - last_data);
      f->writes++;
      f->last_write = t;
      if (is_data) {
        f->data++;
        if (active) { f->active++; if (gap < 26) f->fast26++; }
        last_data = abs_t;
      }
      if (wcsv && isr_frame >= from_frame)
        fprintf(wcsv, "%d,%lld,%s,%02X,%d,%d,%d\n", isr_frame, (long long)t, is_data ? "data" : "ctrl", data, line,
                active, is_data ? gap : -1);
    }
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
static uint16_t fb[256 * 240]; static unsigned fbw, fbh;
static void video_cb(const void *d, unsigned w, unsigned h, size_t p) {
  if (!d || w > 256 || h > 240) return;
  fbw = w; fbh = h;
  for (unsigned y = 0; y < h; y++) memcpy(fb + y * w, (const uint8_t *)d + y * p, w * 2);
}
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t f) { (void)d; return f; }
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned p, unsigned d, unsigned i, unsigned id) { (void)p; (void)d; (void)i; (void)id; return 0; }

#define LOAD(name) do { *(void **)&p_##name = dlsym(core, #name); \
  if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; } } while (0)

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: %s core.so rom.sms out_prefix [--frames N] [--from F]\n", argv[0]); return 2; }
  const char *out = argv[3];
  int frames = 60;
  for (int i = 4; i < argc; i++) {
    if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--from") && i + 1 < argc) from_frame = atoi(argv[++i]);
  }
  if (frames > MAXF) frames = MAXF;
  char path[1024];
  snprintf(path, sizeof path, "%s-writes.csv", out);
  wcsv = fopen(path, "w");
  if (wcsv) fprintf(wcsv, "frame,cycle,port,value,line,active,gap\n");

  void *core = dlopen(argv[1], RTLD_NOW | RTLD_GLOBAL);
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
  LOAD(retro_set_environment); LOAD(retro_set_video_refresh); LOAD(retro_set_audio_sample);
  LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
  LOAD(retro_init); LOAD(retro_load_game); LOAD(retro_run);

  FILE *rf = fopen(argv[2], "rb");
  if (!rf) { perror(argv[2]); return 1; }
  fseek(rf, 0, SEEK_END); long rom_size = ftell(rf); fseek(rf, 0, SEEK_SET);
  void *rom = malloc(rom_size);
  if (fread(rom, 1, rom_size, rf) != (size_t)rom_size) return 1;
  fclose(rf);

  p_retro_set_environment(env_cb);
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  p_retro_init();
  struct retro_game_info gi = {argv[2], rom, (size_t)rom_size, NULL};
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  if (system_hw != SYSTEM_SMS && system_hw != SYSTEM_SMS2) { fprintf(stderr, "not a Master System run\n"); return 1; }
  orig_w = z80_writeport; z80_writeport = port_w;

  for (cur_frame = 0; cur_frame < frames; cur_frame++) p_retro_run();
  if (wcsv) fclose(wcsv);

  snprintf(path, sizeof path, "%s-vram.bin", out);
  FILE *vf = fopen(path, "wb");
  fwrite(vram, 1, 0x4000, vf); fwrite(cram, 1, 0x80, vf);
  fclose(vf);
  snprintf(path, sizeof path, "%s.ppm", out);
  FILE *pf = fopen(path, "wb");
  fprintf(pf, "P6\n%u %u\n255\n", fbw, fbh);
  for (unsigned i = 0; i < fbw * fbh; i++) {
    uint16_t c = fb[i];
    uint8_t px[3] = { ((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31 };
    fwrite(px, 1, 3, pf);
  }
  fclose(pf);

  int vbl_end = (lines_per_frame - 192) * 228;
  snprintf(path, sizeof path, "%s.json", out);
  FILE *jf = fopen(path, "w");
  fprintf(jf, "{\"lines_per_frame\":%d,\"vbl_end\":%d,\"frames\":[", lines_per_frame, vbl_end);
  for (int i = 0; i < frames; i++) {
    frame_t *f = &fr[i];
    fprintf(jf, "%s\n{\"f\":%d,\"entered\":%d,\"exited\":%d,\"reentered\":%d,\"entry\":%lld,\"exit\":%lld,"
            "\"last_write\":%lld,\"writes\":%d,\"data\":%d,\"active\":%d,\"fast26\":%d}",
            i ? "," : "", i, f->entered, f->exited, f->reentered, (long long)f->entry, (long long)f->exit,
            (long long)f->last_write, f->writes, f->data, f->active, f->fast26);
  }
  fprintf(jf, "]}\n");
  fclose(jf);
  return 0;
}
