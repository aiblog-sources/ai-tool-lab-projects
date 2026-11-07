/*
 * mddma: a headless DMA budget checker for Mega Drive / Genesis ROMs.
 *
 * It loads the Genesis Plus GX libretro core (built with HOOK_CPU=1, see
 * build.sh), runs a ROM for N frames without a window and, for every video
 * frame, records:
 *   - what SGDK's DMA queue held when DMA_flushQueue() was entered (bytes,
 *     entries, the entry where the per-frame limit was hit), read from SGDK's
 *     own variables through symbol.txt
 *   - the bytes the VDP really received by DMA, split into VBlank and active
 *     display, with the first and last scanline of any DMA during display
 *   - where those bytes went: tiles, plane maps, sprite table (SAT), hscroll,
 *     CRAM, VSRAM (from the VDP's own table addresses at that moment)
 *   - CPU load and dropped frames (no new game loop started at that VBlank),
 *     the same way mdprof counts them, because a 68000 DMA stalls the CPU
 *
 * Usage: mddma <core.so> <rom.bin> <symbol.txt> <frames> [options]
 *   --pal            run as a 50 Hz PAL console (default NTSC)
 *   --idle a,b,c     idle functions (default VDP_waitVBlank,VDP_waitVSync)
 *   --csv file       per-frame CSV
 *   --from N         leave frames before N out of the summary
 *   --input F-T:BTN  hold pad 1 buttons (U D L R A B C S) from frame F to T
 *   --shot N:file    write the video frame N as a PPM image (repeatable)
 *   --ram ADDR:name  add a 16-bit work RAM word (hex address) to the CSV
 *
 * MIT license (this file). The emulator core it loads has its own license.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libretro.h"
/* keep the core's stdio-to-VFS macros (fopen -> rfopen ...) out of this file */
#define __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H
#define INLINE static inline
#include "shared.h" /* Genesis Plus GX internals: m68k, v_counter, reg, satb ... */

#define MCLK_PER_LINE 3420
#define MCLK_PER_68K 7
#define MAX_SYMS 20000
#define MAX_SHOTS 32
#define MAX_RAMW 8
#define TOP_N 8

enum { D_TILES, D_PLANE, D_SAT, D_HSCROLL, D_CRAM, D_VSRAM, D_N };
static const char *dest_name[D_N] = {"tiles", "plane", "sat", "hscroll", "cram", "vsram"};

typedef struct { uint32_t addr; char name[256]; char type; } sym_t;
static sym_t syms[MAX_SYMS];
static int nsyms;
static uint8_t *rom_idle;          /* ROM address / 2 -> 1 if inside an idle function */
static uint32_t rom_words;

static uint32_t a_flush, a_qsize, a_qindex, a_qlimit, a_qmax; /* SGDK addresses, 0 = not found */

typedef struct {
  uint64_t busy_mclk;
  int loop_start;
  int flushes;
  uint32_t queued, qentries, qlimit_hit, qmax;
  uint32_t sent_vblank, sent_active;
  int first_active, last_active;
  uint32_t dest_active[D_N], dest_all[D_N];
  int sat_first_line, cram_first_line;   /* first display line with a SAT / CRAM DMA write */
  int first_dma_line;                    /* line of the frame's first DMA write */
  int carry;                             /* a DMA was still running when the frame ended */
  uint32_t cpu_vdp;                      /* bytes the CPU wrote itself (not DMA) */
  uint16_t ramw[MAX_RAMW];
} frame_t;

static frame_t cur;
static int64_t last_cycles;
static uint32_t last_pc;
static int have_last, prev_idle;
static int pal;

static const void *last_fb;
static unsigned last_w, last_h;
static size_t last_pitch;

/* --- symbols ------------------------------------------------------------ */
static int cmp_sym(const void *a, const void *b) {
  uint32_t x = ((const sym_t *)a)->addr, y = ((const sym_t *)b)->addr;
  return x < y ? -1 : x > y;
}

static uint32_t find_sym(const char *name) {
  for (int i = 0; i < nsyms; i++) if (!strcmp(syms[i].name, name)) return syms[i].addr;
  return 0;
}

static void load_symbols(const char *path, uint32_t rom_size, const char *idle_list) {
  FILE *f = fopen(path, "r");
  if (!f) { perror(path); exit(1); }
  char line[512];
  while (fgets(line, sizeof line, f) && nsyms < MAX_SYMS - 1) {
    unsigned long addr; char type; char name[256];
    if (sscanf(line, "%lx %c %255s", &addr, &type, name) != 3) continue;
    if (name[0] == '.' || (name[0] == 'L' && name[1] >= '0' && name[1] <= '9')) continue;
    syms[nsyms].addr = (uint32_t)addr;
    syms[nsyms].type = type;
    snprintf(syms[nsyms].name, sizeof syms[nsyms].name, "%s", name);
    nsyms++;
  }
  fclose(f);
  /* SGDK's DMA queue state: static variables in dma.c, listed by nm as 'b' */
  /* DMA_flushQueue is often inlined by LTO; its assembly helper flushQueue is
     not, and it runs before the queue counters are cleared */
  a_flush = find_sym("DMA_flushQueue");
  if (!a_flush) a_flush = find_sym("flushQueue");
  a_qsize = find_sym("queueTransferSize");
  a_qindex = find_sym("queueIndex");
  a_qlimit = find_sym("queueIndexLimit");
  a_qmax = find_sym("maxTransferPerFrame");
  if (!a_flush || !a_qsize) fprintf(stderr, "note: SGDK DMA queue symbols not found, queue columns stay 0\n");

  /* idle functions: mark their ROM range (code symbols only, sorted) */
  sym_t *code = malloc(sizeof(sym_t) * nsyms);
  int nc = 0;
  for (int i = 0; i < nsyms; i++)
    if ((syms[i].type == 'T' || syms[i].type == 't') && syms[i].addr < rom_size) code[nc++] = syms[i];
  qsort(code, nc, sizeof(sym_t), cmp_sym);
  rom_words = rom_size / 2;
  rom_idle = calloc(rom_words, 1);
  char buf[512];
  snprintf(buf, sizeof buf, "%s", idle_list);
  for (char *t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
    int found = 0;
    for (int i = 0; i < nc; i++) {
      if (strcmp(code[i].name, t)) continue;
      uint32_t end = rom_size;
      for (int j = i + 1; j < nc; j++) if (code[j].addr > code[i].addr) { end = code[j].addr; break; }
      for (uint32_t a = code[i].addr; a < end; a += 2) rom_idle[a / 2] = 1;
      found = 1;
    }
    if (!found) fprintf(stderr, "note: idle function %s not in symbols\n", t);
  }
  free(code);
}

static int is_idle_pc(uint32_t pc) {
  pc &= 0xffffff;
  return pc / 2 < rom_words && rom_idle[pc / 2];
}

static uint16_t ram16(uint32_t a) {
  /* GPGX keeps work RAM byte-swapped on little-endian hosts, so a native
     16-bit read of an even address gives the 68000's value */
  return *(uint16_t *)(work_ram + ((a & 0xffff) & ~1u));
}

/* --- where a VRAM write lands -------------------------------------------- */
static int plane_bytes(void) {
  static const int sz[4] = {32, 64, 32, 128};
  return sz[reg[16] & 3] * sz[(reg[16] >> 4) & 3] * 2;
}

static int classify_vram(unsigned addr) {
  addr &= 0xfffe;
  int h40 = reg[12] & 1;
  unsigned sat_len = h40 ? 0x280 : 0x200;
  if (addr >= satb && addr < satb + sat_len) return D_SAT;
  if (addr >= hscb && addr < hscb + 0x400u) return D_HSCROLL;
  unsigned pb = (unsigned)plane_bytes();
  if (addr >= ntab && addr < ntab + pb) return D_PLANE;
  if (addr >= ntbb && addr < ntbb + pb) return D_PLANE;
  unsigned wb = h40 ? 0x1000 : 0x800;
  if (addr >= ntwb && addr < ntwb + wb) return D_PLANE;
  return D_TILES;
}

/* --- the hook ----------------------------------------------------------- */
static void hook(hook_type_t type, int width, unsigned int address, unsigned int value) {
  (void)value;
  if (type == HOOK_M68K_E) {
    int64_t now = m68k.cycles;
    if (have_last) {
      int64_t d = now - last_cycles;
      if (d > 0 && !is_idle_pc(last_pc)) cur.busy_mclk += d;
    }
    last_cycles = now;
    last_pc = address;
    have_last = 1;
    int idle_now = is_idle_pc(address);
    if (!idle_now && prev_idle) cur.loop_start = 1;
    prev_idle = idle_now;
    if (a_flush && (address & 0xffffff) == a_flush) {
      /* the queue as the game left it, before SGDK trims or sends it */
      cur.flushes++;
      cur.queued += ram16(a_qsize);
      if (a_qindex) cur.qentries += ram16(a_qindex);
      if (a_qlimit && ram16(a_qlimit)) cur.qlimit_hit = ram16(a_qlimit);
      if (a_qmax) cur.qmax = ram16(a_qmax);
    }
    return;
  }
  if (type == HOOK_VRAM_W || type == HOOK_CRAM_W || type == HOOK_VSRAM_W) {
    /* A 68k-bus DMA stalls the CPU: the core sets m68k.cycles to the DMA end
       before it runs the transfer, so writes made while m68k.cycles has not
       passed dma_endCycles come from DMA. CPU writes come later. */
    int is_dma = (dma_type < 2) && ((uint32_t)m68k.cycles <= dma_endCycles) && dma_endCycles;
    unsigned bytes = width ? (unsigned)width : 2;
    if (!is_dma) { cur.cpu_vdp += bytes; return; }
    int d = type == HOOK_CRAM_W ? D_CRAM : type == HOOK_VSRAM_W ? D_VSRAM : classify_vram(address);
    cur.dest_all[d] += bytes;
    if (cur.first_dma_line < 0) cur.first_dma_line = v_counter;
    int display_on = reg[1] & 0x40;
    if (display_on && v_counter < bitmap.viewport.h) {
      cur.sent_active += bytes;
      cur.dest_active[d] += bytes;
      if (cur.first_active < 0) cur.first_active = v_counter;
      cur.last_active = v_counter;
      if (d == D_SAT && cur.sat_first_line < 0) cur.sat_first_line = v_counter;
      if (d == D_CRAM && cur.cram_first_line < 0) cur.cram_first_line = v_counter;
    } else {
      cur.sent_vblank += bytes;
    }
  }
}

/* --- libretro frontend glue ---------------------------------------------- */
static bool env_cb(unsigned cmd, void *data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char **)data = "/tmp";
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_RGB565;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
      return true;
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
#define MAX_PAD_INPUTS 64
static struct { int from, to; int16_t mask; } inputs[MAX_PAD_INPUTS];
static int ninputs, cur_frame;
static int16_t parse_buttons(const char *b) {
  int16_t m = 0;
  for (; *b; b++) switch (*b) {
    case 'U': m |= 1 << RETRO_DEVICE_ID_JOYPAD_UP; break;
    case 'D': m |= 1 << RETRO_DEVICE_ID_JOYPAD_DOWN; break;
    case 'L': m |= 1 << RETRO_DEVICE_ID_JOYPAD_LEFT; break;
    case 'R': m |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
    case 'A': m |= 1 << RETRO_DEVICE_ID_JOYPAD_Y; break;
    case 'B': m |= 1 << RETRO_DEVICE_ID_JOYPAD_B; break;
    case 'C': m |= 1 << RETRO_DEVICE_ID_JOYPAD_A; break;
    case 'S': m |= 1 << RETRO_DEVICE_ID_JOYPAD_START; break;
  }
  return m;
}
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned p, unsigned dev, unsigned idx, unsigned id) {
  (void)dev; (void)idx;
  if (p != 0 || id != RETRO_DEVICE_ID_JOYPAD_MASK) return 0;
  int16_t m = 0;
  for (int i = 0; i < ninputs; i++)
    if (cur_frame >= inputs[i].from && cur_frame <= inputs[i].to) m |= inputs[i].mask;
  return m;
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

typedef struct { int frame; uint32_t active; frame_t f; } worst_t;

int main(int argc, char **argv) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s core.so rom.bin symbol.txt frames [--pal] [--csv f] [--from N] [--shot N:f] [--ram ADDR:name]\n", argv[0]);
    return 2;
  }
  const char *core_path = argv[1], *rom_path = argv[2], *sym_path = argv[3];
  int frames = atoi(argv[4]);
  const char *idle_list = "VDP_waitVBlank,VDP_waitVSync";
  const char *csv_path = NULL;
  int from = 0;
  int shot_frame[MAX_SHOTS]; const char *shot_path[MAX_SHOTS]; int nshots = 0;
  uint32_t ram_addr[MAX_RAMW]; const char *ram_name[MAX_RAMW]; int nram = 0;
  for (int i = 5; i < argc; i++) {
    if (!strcmp(argv[i], "--idle") && i + 1 < argc) idle_list = argv[++i];
    else if (!strcmp(argv[i], "--pal")) pal = 1;
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else if (!strcmp(argv[i], "--from") && i + 1 < argc) from = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--input") && i + 1 < argc && ninputs < MAX_PAD_INPUTS) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c || sscanf(a, "%d-%d", &inputs[ninputs].from, &inputs[ninputs].to) != 2) return 2;
      inputs[ninputs++].mask = parse_buttons(c + 1);
    } else if (!strcmp(argv[i], "--shot") && i + 1 < argc && nshots < MAX_SHOTS) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c) return 2;
      *c = 0; shot_frame[nshots] = atoi(a); shot_path[nshots++] = c + 1;
    } else if (!strcmp(argv[i], "--ram") && i + 1 < argc && nram < MAX_RAMW) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c) return 2;
      *c = 0; ram_addr[nram] = (uint32_t)strtoul(a, NULL, 16); ram_name[nram++] = c + 1;
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
  void (*p_set_cpu_hook)(void (*)(hook_type_t, int, unsigned int, unsigned int));
  LOAD(retro_set_environment); LOAD(retro_set_video_refresh); LOAD(retro_set_audio_sample);
  LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
  LOAD(retro_init); LOAD(retro_load_game); LOAD(retro_run); LOAD(set_cpu_hook);

  FILE *rf = fopen(rom_path, "rb");
  if (!rf) { perror(rom_path); return 1; }
  fseek(rf, 0, SEEK_END); long rom_size = ftell(rf); fseek(rf, 0, SEEK_SET);
  void *rom = malloc(rom_size);
  if (fread(rom, 1, rom_size, rf) != (size_t)rom_size) return 1;
  fclose(rf);
  load_symbols(sym_path, (uint32_t)rom_size, idle_list);

  p_retro_set_environment(env_cb);
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  p_retro_init();
  struct retro_game_info gi = {rom_path, rom, (size_t)rom_size, NULL};
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  p_set_cpu_hook(hook);

  FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
  if (csv) {
    fprintf(csv, "frame,lines,load_pct,dropped,flushes,queued,queue_entries,limit_entry,max_transfer,"
                 "sent_vblank,sent_active,first_dma_line,first_active_line,last_active_line,sat_line,cram_line,carry,cpu_vdp_bytes");
    for (int d = 0; d < D_N; d++) fprintf(csv, ",%s_active", dest_name[d]);
    for (int d = 0; d < D_N; d++) fprintf(csv, ",%s_all", dest_name[d]);
    for (int r = 0; r < nram; r++) fprintf(csv, ",%s", ram_name[r]);
    fprintf(csv, "\n");
  }

  worst_t worst[TOP_N];
  for (int k = 0; k < TOP_N; k++) worst[k].frame = -1;
  int n_carry = 0;
  int n_frames = 0, n_spill = 0, n_drop = 0, n_short = 0, n_sat = 0, n_cram = 0, seen_loop = 0;
  uint64_t short_bytes = 0, sum_busy = 0, sum_frame = 0;
  uint32_t max_queued = 0, max_vblank_full = 0;
  int max_queued_frame = -1;

  for (int fno = 0; fno < frames; fno++) {
    memset(&cur, 0, sizeof cur);
    cur.first_active = cur.last_active = cur.sat_first_line = cur.cram_first_line = cur.first_dma_line = -1;
    cur_frame = fno;
    p_retro_run();
    cur.carry = dma_length != 0;
    int64_t frame_mclk = (int64_t)lines_per_frame * MCLK_PER_LINE;
    last_cycles -= frame_mclk;
    for (int r = 0; r < nram; r++) cur.ramw[r] = ram16(ram_addr[r]);

    if (cur.loop_start) seen_loop = 1;
    int dropped = seen_loop && !cur.loop_start;
    uint32_t sent = cur.sent_vblank + cur.sent_active;
    double load = 100.0 * cur.busy_mclk / frame_mclk;
    if (fno >= from) {
      n_frames++;
      sum_busy += cur.busy_mclk; sum_frame += frame_mclk;
      if (dropped) n_drop++;
      if (cur.sent_active) n_spill++;
      if (cur.carry) n_carry++;
      if (cur.sat_first_line >= 0) n_sat++;
      if (cur.cram_first_line >= 0) n_cram++;
      if (cur.flushes && cur.queued > sent) { n_short++; short_bytes += cur.queued - sent; }
      if (cur.queued > max_queued) { max_queued = cur.queued; max_queued_frame = fno; }
      /* DMA still running when the display started: VBlank was used in full */
      if (cur.sent_active && cur.first_active == 0 && cur.sent_vblank > max_vblank_full)
        max_vblank_full = cur.sent_vblank;
      for (int k = 0; k < TOP_N; k++) {
        if (worst[k].frame < 0 || cur.sent_active > worst[k].active) {
          if (!cur.sent_active) break;
          memmove(&worst[k + 1], &worst[k], sizeof(worst_t) * (TOP_N - k - 1));
          worst[k].frame = fno; worst[k].active = cur.sent_active; worst[k].f = cur;
          break;
        }
      }
    }
    if (csv) {
      fprintf(csv, "%d,%d,%.1f,%d,%d,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%u", fno, lines_per_frame, load, dropped,
              cur.flushes, cur.queued, cur.qentries, cur.qlimit_hit, cur.qmax, cur.sent_vblank, cur.sent_active,
              cur.first_dma_line, cur.first_active, cur.last_active, cur.sat_first_line, cur.cram_first_line,
              cur.carry, cur.cpu_vdp);
      for (int d = 0; d < D_N; d++) fprintf(csv, ",%u", cur.dest_active[d]);
      for (int d = 0; d < D_N; d++) fprintf(csv, ",%u", cur.dest_all[d]);
      for (int r = 0; r < nram; r++) fprintf(csv, ",%u", cur.ramw[r]);
      fprintf(csv, "\n");
    }
    for (int s = 0; s < nshots; s++) if (shot_frame[s] == fno) write_ppm(shot_path[s]);
  }
  if (csv) fclose(csv);

  printf("%s, %d lines per frame, %d frames checked (from frame %d)\n", pal ? "PAL 50 Hz" : "NTSC 60 Hz",
         lines_per_frame, n_frames, from);
  printf("mean CPU load %.1f%%, dropped frames %d\n", sum_frame ? 100.0 * sum_busy / sum_frame : 0.0, n_drop);
  printf("largest SGDK queue: %u bytes (frame %d)\n", max_queued, max_queued_frame);
  if (max_vblank_full) printf("VBlank used in full: up to %u bytes went out before line 0\n", max_vblank_full);
  printf("frames with DMA during active display: %d\n", n_spill);
  printf("frames that ended with a DMA still running (it finishes in the next frame): %d\n", n_carry);
  printf("frames where SAT (sprite table) DMA landed during display: %d\n", n_sat);
  printf("frames where CRAM DMA landed during display: %d\n", n_cram);
  printf("frames where the VDP got fewer bytes than SGDK queued (counting bytes that spill into the next frame as missing): %d (%llu bytes)\n", n_short,
         (unsigned long long)short_bytes);
  if (worst[0].frame >= 0) printf("\nframes with the most DMA during display:\n");
  for (int k = 0; k < TOP_N && worst[k].frame >= 0; k++) {
    frame_t *f = &worst[k].f;
    printf("  frame %d: queued %u, sent %u in VBlank + %u during lines %d-%d", worst[k].frame, f->queued,
           f->sent_vblank, f->sent_active, f->first_active, f->last_active);
    for (int d = 0; d < D_N; d++) if (f->dest_active[d]) printf(", %s %u", dest_name[d], f->dest_active[d]);
    if (f->sat_first_line >= 0) printf("; SAT from line %d", f->sat_first_line);
    printf("\n");
  }
  return 0;
}
