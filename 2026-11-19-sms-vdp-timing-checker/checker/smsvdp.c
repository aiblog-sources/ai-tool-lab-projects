/*
 * smsvdp: a headless checker for Master System VDP writes that come too fast
 * during active display.
 *
 * On a real Master System the VDP only lets the CPU into video memory between
 * its own reads while it draws the picture. Data port writes (VRAM or CRAM)
 * that come closer together than that are lost or land wrong. Most emulators
 * accept them anyway, so code that looks fine in an emulator can corrupt tiles
 * or colors on the console. Community figures for the minimum gap during
 * active display are 22.8 Z80 cycles (Maxim, SMS Power), 26 (sverx's 2014 test)
 * and 29 (the official Mark III manual, quoted by Bumbershoot Software).
 * During VBlank, in the border, and with the display off there is no limit.
 *
 * smsvdp loads the Genesis Plus GX libretro core, runs an .sms ROM for N
 * frames with no window, and wraps the core's Z80 port handlers. For every
 * VDP data port access it records the Z80 cycles since the previous one, the
 * scanline, whether the display was on, the destination (VRAM or CRAM) and the
 * instruction address. It reports the accesses made during active display
 * with a gap under 23, 26 and 29 cycles, per frame and per instruction.
 *
 * Usage: smsvdp <core.so> <rom.sms> <frames> [options]
 *   --sym file       SDCC .noi (DEF name 0xADDR) or "ADDR name" symbol file
 *   --min N          gap that counts as too fast in the headline (default 26)
 *   --from N         leave frames before N out of the summary
 *   --csv file       per-frame CSV
 *   --sites file     per-instruction CSV
 *   --events file    every data port access during active display (CSV)
 *   --shot N:file    write video frame N as a PPM image (repeatable)
 *   --ram ADDR:name  add a Z80 RAM byte (hex address, read at frame end) to the CSV
 *   --pal            run as a 50 Hz PAL console (default NTSC)
 *
 * Timestamps: the core charges an instruction's cycles before running it, so
 * a port write is stamped at the end of its instruction. Every Z80 OUT form
 * ends with its I/O cycle, so gaps between writes are exact; OTIR's extra 5
 * cycles per repeat land after its write, as on the real CPU.
 *
 * MIT license (this file). The emulator core it loads has its own license.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"
#define __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H
#define INLINE static inline
#include "shared.h" /* Genesis Plus GX internals: Z80, reg, bitmap, lines_per_frame */

#define MCLK_PER_LINE 3420
#define MCLK_PER_Z80 15
#define MAX_SYMS 8000
#define MAX_SHOTS 32
#define HIST_MAX 64

/* --- symbols -------------------------------------------------------------- */
typedef struct { uint32_t addr; char name[96]; } sym_t;
static sym_t syms[MAX_SYMS];
static int nsyms;

static int cmp_sym(const void *a, const void *b) {
  uint32_t x = ((const sym_t *)a)->addr, y = ((const sym_t *)b)->addr;
  return x < y ? -1 : x > y;
}

static void load_symbols(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) { perror(path); exit(1); }
  char line[512], name[256];
  unsigned long addr;
  while (fgets(line, sizeof line, f) && nsyms < MAX_SYMS) {
    if (sscanf(line, "DEF %255s 0x%lx", name, &addr) != 2 &&
        sscanf(line, "%lx %255s", &addr, name) != 2) continue;
    /* code only (ROM page 0-2), no linker section markers */
    if (addr >= 0xC000 || !strncmp(name, "l__", 3) || !strncmp(name, "s__", 3) || name[0] == '.') continue;
    syms[nsyms].addr = (uint32_t)addr;
    snprintf(syms[nsyms].name, sizeof syms[nsyms].name, "%s", name);
    nsyms++;
  }
  fclose(f);
  qsort(syms, nsyms, sizeof(sym_t), cmp_sym);
}

static const char *sym_for(uint32_t pc, uint32_t *off) {
  const sym_t *best = NULL;
  for (int i = 0; i < nsyms && syms[i].addr <= pc; i++) best = &syms[i];
  if (!best) return NULL;
  *off = pc - best->addr;
  return best->name;
}

/* --- state ---------------------------------------------------------------- */
typedef struct {
  uint32_t writes, active, fast23, fast26, fast29, fastmin, cram_active;
  uint32_t reads_active, reads_fast;
  int first_fast_line, last_fast_line;
  int blank_lines;  /* visible lines (0..h-1) blanked by switching the display off and on in this frame */
  int screen_off;   /* the display was off at the start and at the end of this frame */
} frame_t;

typedef struct {
  uint32_t active, fast23, fast26, fast29, fastmin, all;
  int min_gap, first_frame, last_frame, min_line, max_line;
  uint8_t dest;  /* 1 VRAM, 2 CRAM */
} site_t;

static frame_t cur;
static site_t sites[0x10000];
static uint64_t hist[HIST_MAX + 2];  /* gaps of active-display writes, last bin = more */
static int64_t frame_base, last_data = -1000000, last_ctrl = -1000000;
static int min_gap_cfg = 26, from_frame, cur_frame;
static int pal;
static FILE *events;

/* our own copy of the VDP's port state: latch, code, address */
static int latch_second, vcode;
static unsigned vaddr, latch_low;

static int cmp_site(const void *a, const void *b) {
  uint32_t x = sites[*(const int *)a].fastmin, y = sites[*(const int *)b].fastmin;
  return x > y ? -1 : x < y;
}

static void (*orig_w)(unsigned int, unsigned char);
static unsigned char (*orig_r)(unsigned int);

static int line_now(int *active) {
  /* the core starts each SMS frame at line viewport.h (start of VBlank), then
     the bottom/top border and blanking, then active lines 0..h-1 */
  int h = bitmap.viewport.h;
  int k = (int)(Z80.cycles / MCLK_PER_LINE);
  int line = (h + k) % lines_per_frame;
  *active = (reg[1] & 0x40) && line < h;
  return line;
}

static void data_access(int is_write) {
  int64_t now = frame_base + (int64_t)Z80.cycles;
  int gap = (int)((now - last_data) / MCLK_PER_Z80);
  int gap_ctrl = (int)((now - last_ctrl) / MCLK_PER_Z80);
  last_data = now;
  latch_second = 0;
  int active;
  int line = line_now(&active);
  int cram = is_write && vcode == 3;
  vaddr = (vaddr + 1) & 0x3FFF;
  if (!is_write) {
    /* a read needs the same gap after the previous data access or the
       address setup on the control port */
    int g = gap < gap_ctrl ? gap : gap_ctrl;
    if (active && cur_frame >= from_frame) {
      cur.reads_active++;
      if (g < min_gap_cfg) cur.reads_fast++;
    }
    return;
  }
  cur.writes++;
  uint32_t pc = (Z80.pc.w.l - 2) & 0xFFFF;  /* every OUT to the VDP is a 2-byte instruction */
  site_t *s = &sites[pc];
  if (cur_frame >= from_frame) s->all++;
  if (!active || cur_frame < from_frame) return;
  cur.active++;
  if (cram) cur.cram_active++;
  hist[gap > HIST_MAX ? HIST_MAX + 1 : gap]++;
  if (events) fprintf(events, "%d,%d,%d,%04X,%s,%04X\n", cur_frame, line, gap, pc, cram ? "cram" : "vram", (vaddr - 1) & 0x3FFF);
  if (!s->active) { s->min_gap = gap; s->first_frame = cur_frame; s->min_line = line; s->max_line = line; }
  s->active++;
  s->last_frame = cur_frame;
  if (gap < s->min_gap) s->min_gap = gap;
  if (line < s->min_line) s->min_line = line;
  if (line > s->max_line) s->max_line = line;
  s->dest |= cram ? 2 : 1;
  if (gap < 23) { cur.fast23++; s->fast23++; }
  if (gap < 26) { cur.fast26++; s->fast26++; }
  if (gap < 29) { cur.fast29++; s->fast29++; }
  if (gap < min_gap_cfg) {
    cur.fastmin++; s->fastmin++;
    if (cur.first_fast_line < 0) cur.first_fast_line = line;
    cur.last_fast_line = line;
  }
}

/* display switched off and on mid-frame: VDP access is then free, but the
   lines in between are blank on screen. Line indexes count from the start of
   the core's frame (k = 0 is the first VBlank line). */
static int disp_on, off_k, off_frame = -1;

static void blank_add(int k0, int k1) {
  int a0 = lines_per_frame - bitmap.viewport.h;
  if (k0 < a0) k0 = a0;
  if (k1 > lines_per_frame) k1 = lines_per_frame;
  if (k1 > k0) cur.blank_lines += k1 - k0;
}

static void display_check(void) {
  int on = (reg[1] & 0x40) != 0;
  if (on == disp_on) return;
  if (!on) { off_k = (int)(Z80.cycles / MCLK_PER_LINE); off_frame = cur_frame; }   /* line it went off */
  /* off and back on in the same frame tears the picture; an off period that
     spans frames is a black screen (loading, transitions) and is not counted */
  else if (off_frame == cur_frame)
    blank_add(off_k, (int)((Z80.cycles + MCLK_PER_LINE - 1) / MCLK_PER_LINE));      /* through the line it came back */
  disp_on = on;
}

static void port_w(unsigned int port, unsigned char data) {
  switch (port & 0xC1) {
    case 0x80: data_access(1); break;
    case 0x81:
      last_ctrl = frame_base + (int64_t)Z80.cycles;
      if (!latch_second) { latch_low = data; latch_second = 1; }
      else {
        latch_second = 0;
        vcode = data >> 6;
        vaddr = ((data & 0x3F) << 8) | latch_low;
        if (vcode == 0) vaddr = (vaddr + 1) & 0x3FFF;  /* read setup prefetches one byte */
      }
      break;
  }
  orig_w(port, data);
  if ((port & 0xC1) == 0x81 && !latch_second) display_check();
}

static unsigned char port_r(unsigned int port) {
  switch (port & 0xC1) {
    case 0x80: data_access(0); break;
    case 0x81: latch_second = 0; break;
  }
  return orig_r(port);
}

/* --- libretro frontend glue ------------------------------------------------ */
static const void *last_fb;
static unsigned last_w, last_h;
static size_t last_pitch;

static bool env_cb(unsigned cmd, void *data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char **)data = "/tmp";
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool *)data = true;
      return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
      struct retro_variable *v = data;
      if (pal && v->key && !strcmp(v->key, "genesis_plus_gx_region_detect")) { v->value = "pal"; return true; }
      v->value = NULL;
      return false;
    }
    default:
      return false;
  }
}
static void video_cb(const void *d, unsigned w, unsigned h, size_t pitch) {
  if (d) { last_fb = d; last_w = w; last_h = h; last_pitch = pitch; }
}
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t f) { (void)d; return f; }
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned p, unsigned dev, unsigned idx, unsigned id) {
  (void)p; (void)dev; (void)idx; (void)id;
  return 0;
}

static void write_ppm(const char *path) {
  if (!last_fb) return;
  FILE *f = fopen(path, "wb");
  if (!f) { perror(path); return; }
  fprintf(f, "P6\n%u %u\n255\n", last_w, last_h);
  for (unsigned y = 0; y < last_h; y++) {
    const uint16_t *row = (const uint16_t *)((const uint8_t *)last_fb + y * last_pitch);
    for (unsigned x = 0; x < last_w; x++) {
      uint16_t c = row[x];
      unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31),
                              (unsigned char)(((c >> 5) & 63) * 255 / 63),
                              (unsigned char)((c & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
}

#define LOAD(name) do { *(void **)&p_##name = dlsym(core, #name); \
  if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; } } while (0)

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s core.so rom.sms frames [--sym f] [--min N] [--from N] [--csv f] [--sites f] [--events f] [--shot N:f] [--pal]\n", argv[0]);
    return 2;
  }
  const char *core_path = argv[1], *rom_path = argv[2];
  int frames = atoi(argv[3]);
  const char *csv_path = NULL, *sites_path = NULL, *events_path = NULL;
  int shot_frame[MAX_SHOTS]; const char *shot_path[MAX_SHOTS]; int nshots = 0;
  unsigned ram_addr[4]; const char *ram_name[4]; int nram = 0;
  for (int i = 4; i < argc; i++) {
    if (!strcmp(argv[i], "--sym") && i + 1 < argc) load_symbols(argv[++i]);
    else if (!strcmp(argv[i], "--min") && i + 1 < argc) min_gap_cfg = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--from") && i + 1 < argc) from_frame = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else if (!strcmp(argv[i], "--sites") && i + 1 < argc) sites_path = argv[++i];
    else if (!strcmp(argv[i], "--events") && i + 1 < argc) events_path = argv[++i];
    else if (!strcmp(argv[i], "--pal")) pal = 1;
    else if (!strcmp(argv[i], "--ram") && i + 1 < argc && nram < 4) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c) return 2;
      *c = 0; ram_addr[nram] = (unsigned)strtoul(a, NULL, 16); ram_name[nram++] = c + 1;
    }
    else if (!strcmp(argv[i], "--shot") && i + 1 < argc && nshots < MAX_SHOTS) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c) return 2;
      *c = 0; shot_frame[nshots] = atoi(a); shot_path[nshots++] = c + 1;
    } else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
  }

  void *core = dlopen(core_path, RTLD_NOW | RTLD_LOCAL);
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

  FILE *rf = fopen(rom_path, "rb");
  if (!rf) { perror(rom_path); return 1; }
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
  struct retro_game_info gi = {rom_path, rom, (size_t)rom_size, NULL};
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  if (system_hw != SYSTEM_SMS && system_hw != SYSTEM_SMS2) {
    fprintf(stderr, "not running as a Master System (system_hw 0x%x); name the ROM .sms\n", system_hw);
    return 1;
  }
  /* wrap the core's port handlers (set once in gen_init, exported because
     the core is built without a version script) */
  disp_on = (reg[1] & 0x40) != 0; off_k = 0;
  orig_w = z80_writeport; z80_writeport = port_w;
  orig_r = z80_readport;  z80_readport = port_r;

  FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
  if (csv) fprintf(csv, "frame,data_writes,active_writes,cram_active,fast_lt23,fast_lt26,fast_lt29,fast_min,first_fast_line,last_fast_line,reads_active,reads_fast,blank_lines,screen_off");
  if (csv) for (int r = 0; r < nram; r++) fprintf(csv, ",%s", ram_name[r]);
  if (csv) fprintf(csv, "\n");
  if (events_path) {
    events = fopen(events_path, "w");
    if (events) fprintf(events, "frame,line,gap,pc,dest,addr\n");
  }

  uint64_t t_writes = 0, t_active = 0, t23 = 0, t26 = 0, t29 = 0, tmin = 0, t_reads = 0, t_reads_fast = 0;
  int frames_fast = 0, n = 0, worst_frame = -1, frames_blank = 0, max_blank = 0, frames_off = 0;
  uint64_t t_blank = 0;
  uint32_t worst = 0;
  for (int fno = 0; fno < frames; fno++) {
    memset(&cur, 0, sizeof cur);
    cur.first_fast_line = cur.last_fast_line = -1;
    cur_frame = fno;
    p_retro_run();
    cur.screen_off = !disp_on && off_frame < cur_frame && !(reg[1] & 0x40);
    /* a display still off at the end of the frame it went off in: blank to the end */
    if (!disp_on && off_frame == cur_frame) blank_add(off_k, lines_per_frame);
    frame_base += (int64_t)lines_per_frame * MCLK_PER_LINE;
    if (fno >= from_frame) {
      n++;
      t_writes += cur.writes; t_active += cur.active;
      t23 += cur.fast23; t26 += cur.fast26; t29 += cur.fast29; tmin += cur.fastmin;
      t_reads += cur.reads_active; t_reads_fast += cur.reads_fast;
      if (cur.fastmin) frames_fast++;
      /* a frame with the display off throughout is a black screen (loading,
         transitions); only a switch during the visible lines tears the picture */
      if (cur.screen_off) frames_off++;
      if (cur.blank_lines) {
        frames_blank++; t_blank += cur.blank_lines;
        if (cur.blank_lines > max_blank) max_blank = cur.blank_lines;
      }
      if (cur.fastmin > worst) { worst = cur.fastmin; worst_frame = fno; }
    }
    if (csv) fprintf(csv, "%d,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u,%d,%d", fno, cur.writes, cur.active, cur.cram_active,
                     cur.fast23, cur.fast26, cur.fast29, cur.fastmin, cur.first_fast_line, cur.last_fast_line,
                     cur.reads_active, cur.reads_fast, cur.blank_lines, cur.screen_off);
    if (csv) {
      for (int r = 0; r < nram; r++) fprintf(csv, ",%u", work_ram[ram_addr[r] & 0x1FFF]);
      fprintf(csv, "\n");
    }
    for (int s = 0; s < nshots; s++) if (shot_frame[s] == fno) write_ppm(shot_path[s]);
  }
  if (csv) fclose(csv);
  if (events) fclose(events);

  printf("%s, %d lines per frame, %d frames checked (from frame %d)\n", pal ? "PAL 50 Hz" : "NTSC 60 Hz",
         lines_per_frame, n, from_frame);
  printf("VDP data writes: %llu, during active display: %llu\n", (unsigned long long)t_writes, (unsigned long long)t_active);
  printf("active-display writes closer than 23 / 26 / 29 cycles to the previous access: %llu / %llu / %llu\n",
         (unsigned long long)t23, (unsigned long long)t26, (unsigned long long)t29);
  printf("frames with a write under %d cycles: %d", min_gap_cfg, frames_fast);
  if (worst_frame >= 0) printf(" (worst: frame %d, %u writes)", worst_frame, worst);
  printf("\n");
  if (frames_blank) printf("frames that switched the display off and on during visible lines: %d (blank lines per such frame: mean %.1f, max %d of %d)\n",
                          frames_blank, (double)t_blank / frames_blank, max_blank, bitmap.viewport.h);
  if (frames_off) printf("frames with the display off the whole time (black screen): %d\n", frames_off);
  if (t_reads) printf("VRAM reads during active display: %llu, under %d cycles: %llu\n", (unsigned long long)t_reads,
                      min_gap_cfg, (unsigned long long)t_reads_fast);

  /* instructions, most violations first */
  static int order[0x10000];
  int no = 0;
  for (int pc = 0; pc < 0x10000; pc++) if (sites[pc].fastmin) order[no++] = pc;
  qsort(order, no, sizeof(int), cmp_site);
  if (t_active && nsyms) {
    /* the same per function (unrolled code has one instruction per byte) */
    static uint32_t f_fast[MAX_SYMS + 1], f_act[MAX_SYMS + 1], f23[MAX_SYMS + 1], f26[MAX_SYMS + 1], f29[MAX_SYMS + 1];
    static int f_min[MAX_SYMS + 1];
    for (int i = 0; i <= nsyms; i++) f_min[i] = 1 << 30;
    for (int pc = 0; pc < 0x10000; pc++) {
      site_t *s = &sites[pc];
      if (!s->active) continue;
      int k = nsyms;
      for (int i = 0; i < nsyms && syms[i].addr <= (uint32_t)pc; i++) k = i;
      f_fast[k] += s->fastmin; f_act[k] += s->active;
      f23[k] += s->fast23; f26[k] += s->fast26; f29[k] += s->fast29;
      if (s->min_gap < f_min[k]) f_min[k] = s->min_gap;
    }
    printf("\nfunctions with active-display writes (too fast under 23 / 26 / 29 cycles):\n");
    for (int i = 0; i <= nsyms; i++)
      if (f_act[i]) printf("  %-32s %8u active writes, %u / %u / %u too fast, min gap %d\n",
                           i < nsyms ? syms[i].name : "(no symbol)", f_act[i], f23[i], f26[i], f29[i], f_min[i]);
  }
  if (no) printf("\ninstructions that wrote too fast during active display (gap < %d):\n", min_gap_cfg);
  for (int i = 0; i < no && i < 12; i++) {
    site_t *s = &sites[order[i]];
    uint32_t off = 0;
    const char *nm = sym_for(order[i], &off);
    printf("  %04X", order[i]);
    if (nm) printf(" %s+0x%X", nm, off);
    printf(": %u of %u active writes too fast, min gap %d, %s, lines %d-%d, frames %d-%d\n", s->fastmin, s->active,
           s->min_gap, s->dest == 3 ? "VRAM+CRAM" : s->dest == 2 ? "CRAM" : "VRAM", s->min_line, s->max_line,
           s->first_frame, s->last_frame);
  }
  if (sites_path) {
    FILE *sf = fopen(sites_path, "w");
    if (sf) {
      fprintf(sf, "pc,symbol,offset,writes,active_writes,fast_lt23,fast_lt26,fast_lt29,min_gap,min_line,max_line,first_frame,last_frame,dest\n");
      for (int pc = 0; pc < 0x10000; pc++) {
        site_t *s = &sites[pc];
        if (!s->all) continue;
        uint32_t off = 0;
        const char *nm = sym_for(pc, &off);
        fprintf(sf, "%04X,%s,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%s\n", pc, nm ? nm : "", off, s->all, s->active,
                s->fast23, s->fast26, s->fast29, s->active ? s->min_gap : -1, s->active ? s->min_line : -1,
                s->active ? s->max_line : -1, s->active ? s->first_frame : -1, s->active ? s->last_frame : -1,
                s->dest == 3 ? "both" : s->dest == 2 ? "cram" : s->dest == 1 ? "vram" : "");
      }
      fclose(sf);
    }
  }
  printf("\ngap histogram of active-display writes (Z80 cycles: count)\n ");
  int col = 0;
  for (int g = 0; g <= HIST_MAX + 1; g++) {
    if (!hist[g]) continue;
    if (g == HIST_MAX + 1) printf(" >%d: %llu", HIST_MAX, (unsigned long long)hist[g]);
    else printf(" %d: %llu", g, (unsigned long long)hist[g]);
    if (++col % 8 == 0) printf("\n ");
  }
  printf("\n");
  return tmin ? 3 : frames_blank ? 4 : 0;
}
