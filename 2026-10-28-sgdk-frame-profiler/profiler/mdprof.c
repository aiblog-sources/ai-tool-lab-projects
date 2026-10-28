/*
 * mdprof: a headless frame-budget profiler for Mega Drive / Genesis ROMs.
 *
 * It loads the Genesis Plus GX libretro core (built with HOOK_CPU=1, see
 * build.sh), runs a ROM for N frames without a window, and uses the core's
 * CPU hook to charge every 68000 instruction's cycles to the function that
 * ran it (from an SGDK symbol.txt). Per video frame it records:
 *   - busy cycles: everything except the idle (wait-for-VBlank) functions
 *   - the scanline where the game went from its own code into the idle function
 *   - whether a new game loop started (the CPU left the idle function)
 *   - DMA bytes written to VRAM/CRAM/VSRAM, split into VBlank and active display
 * A video frame in which no new game loop started is a dropped frame (the
 * slowdown you see). The busiest frames are listed with the functions that
 * used the time.
 *
 * Usage: mdprof <core.so> <rom.bin> <symbol.txt> <frames> [options]
 *   --idle a,b,c     idle functions (default VDP_waitVBlank,VDP_waitVSync)
 *   --csv file       per-frame CSV
 *   --from N         leave frames before N (boot) out of the summary
 *   --nohook         run without the hook (to time the emulator alone)
 *   --input F-T:BTN  hold pad 1 buttons (U D L R A B C S) from frame F to T
 *   --json file      summary JSON (whole-run profile, worst frames)
 *   --shot N:file    write the video frame N as a PPM image (repeatable)
 *   --ram ADDR:file  dump 16-bit big-endian work RAM word at ADDR per frame
 *                    into the CSV (repeatable via --ram, ADDR from symbol.txt)
 *
 * MIT license (this file). The emulator core it loads has its own license.
 */
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libretro.h"
/* keep the core's stdio-to-VFS macros (fopen -> rfopen ...) out of this file */
#define __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H
#define INLINE static inline
#include "shared.h" /* Genesis Plus GX internals: m68k, v_counter, reg, ... */

#define MCLK_PER_LINE 3420
#define MCLK_PER_68K 7
#define MAX_SYMS 20000
#define MAX_SHOTS 16
#define MAX_RAMW 8
#define TOP_N 6

typedef struct { uint32_t addr; char name[96]; } sym_t;

static sym_t syms[MAX_SYMS];
static int nsyms;
static int32_t *rom_map;            /* ROM address / 2 -> symbol index */
static uint32_t rom_map_words;
static int idle_sym[16], nidle;

/* run-wide and per-frame counters (master clocks) */
static uint64_t *fn_total, *fn_frame;
static int64_t last_cycles;
static uint32_t last_pc;
static int have_last;

typedef struct {
  uint64_t frame_mclk, busy_mclk, idle_mclk;
  int idle_line;                    /* first scanline in idle fn, -1 = never */
  uint32_t dma_vblank, dma_active;  /* bytes */
  uint32_t cpu_vram;                /* bytes written by the CPU, not DMA */
  int last_dma_line;
  int loop_start;                   /* a game loop started in this frame */
  uint16_t ramw[MAX_RAMW];
} frame_t;

static frame_t cur;
static int in_idle_seen, prev_idle;

/* video */
static const void *last_fb;
static unsigned last_w, last_h;
static size_t last_pitch;

/* --- symbols ------------------------------------------------------------ */
static int cmp_sym(const void *a, const void *b) {
  uint32_t x = ((const sym_t *)a)->addr, y = ((const sym_t *)b)->addr;
  if (x != y) return x < y ? -1 : 1;
  return a < b ? -1 : a > b; /* keep file order for aliases */
}

static void load_symbols(const char *path, uint32_t rom_size) {
  FILE *f = fopen(path, "r");
  if (!f) { perror(path); exit(1); }
  char line[512];
  while (fgets(line, sizeof line, f) && nsyms < MAX_SYMS - 1) {
    unsigned long addr; char type; char name[256];
    if (sscanf(line, "%lx %c %255s", &addr, &type, name) != 3) continue;
    if (type != 'T' && type != 't') continue;
    if (addr >= rom_size) continue;
    /* local labels in asm files (.fq_loop in SGDK's DMA flush, L1..L6 in
       libgcc's division) count as part of the function they sit in */
    if (name[0] == '.' || (name[0] == 'L' && name[1] >= '0' && name[1] <= '9')) continue;
    syms[nsyms].addr = (uint32_t)addr;
    snprintf(syms[nsyms].name, sizeof syms[nsyms].name, "%s", name);
    nsyms++;
  }
  fclose(f);
  qsort(syms, nsyms, sizeof(sym_t), cmp_sym);
  /* aliases (__divsi3 and __divsi3_internal): keep the first name only */
  int k = 0;
  for (int i = 0; i < nsyms; i++)
    if (k == 0 || syms[i].addr != syms[k - 1].addr) syms[k++] = syms[i];
  nsyms = k;
  /* index nsyms = "[no symbol]" (vectors, header), nsyms+1 = "[ram code]" */
  rom_map_words = rom_size / 2;
  rom_map = malloc(sizeof(int32_t) * rom_map_words);
  int s = -1;
  for (uint32_t w = 0; w < rom_map_words; w++) {
    while (s + 1 < nsyms && syms[s + 1].addr <= w * 2) s++;
    rom_map[w] = s < 0 ? nsyms : s;
  }
}

static int pc_to_sym(uint32_t pc) {
  pc &= 0xffffff;
  if (pc / 2 < rom_map_words) return rom_map[pc / 2];
  return nsyms + 1;
}

static const char *sym_name(int i) {
  if (i == nsyms) return "[no symbol]";
  if (i == nsyms + 1) return "[ram code]";
  return syms[i].name;
}

static int is_idle(int s) {
  for (int i = 0; i < nidle; i++) if (idle_sym[i] == s) return 1;
  return 0;
}

/* --- the hook ----------------------------------------------------------- */
static void hook(hook_type_t type, int width, unsigned int address, unsigned int value) {
  (void)value; (void)address;
  if (type == HOOK_M68K_E) {
    int64_t now = m68k.cycles;
    if (have_last) {
      int64_t d = now - last_cycles;
      if (d > 0) {
        int s = pc_to_sym(last_pc);
        fn_total[s] += d;
        fn_frame[s] += d;
        if (is_idle(s)) cur.idle_mclk += d; else cur.busy_mclk += d;
      }
    }
    last_cycles = now;
    last_pc = address;
    have_last = 1;
    /* the frame's work ends when the CPU moves from game code into the idle
       function; a frame starts at VBlank, often already inside it */
    int idle_now = is_idle(pc_to_sym(address));
    if (idle_now && !prev_idle && !in_idle_seen) {
      in_idle_seen = 1;
      cur.idle_line = v_counter;
    }
    /* and a new game loop starts when the CPU leaves the idle function */
    if (!idle_now && prev_idle) cur.loop_start = 1;
    prev_idle = idle_now;
    return;
  }
  if (type == HOOK_VRAM_W || type == HOOK_CRAM_W || type == HOOK_VSRAM_W) {
    /* A 68k-bus DMA stalls the CPU: the core sets m68k.cycles to the DMA end
       before it runs the transfer, so writes made while m68k.cycles has not
       passed dma_endCycles come from DMA. CPU writes come later. */
    int is_dma = (dma_type < 2) && ((uint32_t)m68k.cycles <= dma_endCycles) && dma_endCycles;
    unsigned bytes = width ? (unsigned)width : 2;
    if (is_dma) {
      int display_on = reg[1] & 0x40;
      if (display_on && v_counter < bitmap.viewport.h) cur.dma_active += bytes;
      else cur.dma_vblank += bytes;
      cur.last_dma_line = v_counter;
    } else {
      cur.cpu_vram += bytes;
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
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS: /* pad state as one bitmask */
      return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool *)data = true;
      return true;
    default:
      return false;
  }
}
static void video_cb(const void *d, unsigned w, unsigned h, size_t pitch) {
  if (d) { last_fb = d; last_w = w; last_h = h; last_pitch = pitch; }
}
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t f) { (void)d; return f; }
/* scripted pad 1: --input FROM-TO:BUTTONS, buttons U D L R A B C S */
#define MAX_INPUTS 64
static struct { int from, to; int16_t mask; } inputs[MAX_INPUTS];
static int ninputs, cur_frame;
static int16_t parse_buttons(const char *b) {
  int16_t m = 0;
  for (; *b; b++) switch (*b) {
    case 'U': m |= 1 << RETRO_DEVICE_ID_JOYPAD_UP; break;
    case 'D': m |= 1 << RETRO_DEVICE_ID_JOYPAD_DOWN; break;
    case 'L': m |= 1 << RETRO_DEVICE_ID_JOYPAD_LEFT; break;
    case 'R': m |= 1 << RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
    case 'A': m |= 1 << RETRO_DEVICE_ID_JOYPAD_Y; break;   /* the core maps Y to MD A */
    case 'B': m |= 1 << RETRO_DEVICE_ID_JOYPAD_B; break;
    case 'C': m |= 1 << RETRO_DEVICE_ID_JOYPAD_A; break;   /* and A to MD C */
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
      unsigned char rgb[3] = {
        (unsigned char)(((c >> 11) & 31) * 255 / 31),
        (unsigned char)(((c >> 5) & 63) * 255 / 63),
        (unsigned char)((c & 31) * 255 / 31)};
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
}

#define LOAD(name) do { *(void **)&p_##name = dlsym(core, #name); \
  if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; } } while (0)

typedef struct { int idx; uint64_t v; } kv_t;
static int cmp_kv(const void *a, const void *b) {
  uint64_t x = ((const kv_t *)a)->v, y = ((const kv_t *)b)->v;
  return x < y ? 1 : x > y ? -1 : 0;
}

static void top_fns(uint64_t *arr, kv_t *out, int n, int skip_idle) {
  /* simple partial selection, nsyms is small */
  for (int k = 0; k < n; k++) { out[k].idx = -1; out[k].v = 0; }
  for (int i = 0; i < nsyms + 2; i++) {
    if (!arr[i] || (skip_idle && is_idle(i))) continue;
    for (int k = 0; k < n; k++) {
      if (arr[i] > out[k].v) {
        memmove(&out[k + 1], &out[k], sizeof(kv_t) * (n - k - 1));
        out[k].idx = i; out[k].v = arr[i];
        break;
      }
    }
  }
}

int main(int argc, char **argv) {
  if (argc < 5) {
    fprintf(stderr, "usage: %s core.so rom.bin symbol.txt frames [--idle a,b] [--csv f] [--json f] [--shot N:f] [--ram ADDR:name]\n", argv[0]);
    return 2;
  }
  const char *core_path = argv[1], *rom_path = argv[2], *sym_path = argv[3];
  int frames = atoi(argv[4]);
  const char *idle_list = "VDP_waitVBlank,VDP_waitVSync";
  const char *csv_path = NULL, *json_path = NULL;
  int from = 0, nohook = 0;
  int shot_frame[MAX_SHOTS]; const char *shot_path[MAX_SHOTS]; int nshots = 0;
  uint32_t ram_addr[MAX_RAMW]; const char *ram_name[MAX_RAMW]; int nram = 0;
  for (int i = 5; i < argc; i++) {
    if (!strcmp(argv[i], "--idle") && i + 1 < argc) idle_list = argv[++i];
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else if (!strcmp(argv[i], "--from") && i + 1 < argc) from = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--nohook")) nohook = 1;
    else if (!strcmp(argv[i], "--input") && i + 1 < argc && ninputs < MAX_INPUTS) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c || sscanf(a, "%d-%d", &inputs[ninputs].from, &inputs[ninputs].to) != 2) return 2;
      inputs[ninputs++].mask = parse_buttons(c + 1);
    }
    else if (!strcmp(argv[i], "--json") && i + 1 < argc) json_path = argv[++i];
    else if (!strcmp(argv[i], "--shot") && i + 1 < argc && nshots < MAX_SHOTS) {
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

  load_symbols(sym_path, (uint32_t)rom_size);
  fn_total = calloc(nsyms + 2, sizeof(uint64_t));
  fn_frame = calloc(nsyms + 2, sizeof(uint64_t));
  {
    char buf[512]; snprintf(buf, sizeof buf, "%s", idle_list);
    for (char *t = strtok(buf, ","); t && nidle < 16; t = strtok(NULL, ",")) {
      int found = 0;
      for (int i = 0; i < nsyms; i++)
        if (!strcmp(syms[i].name, t)) { idle_sym[nidle++] = i; found = 1; break; }
      if (!found) fprintf(stderr, "note: idle function %s not in symbols\n", t);
    }
  }

  p_retro_set_environment(env_cb);
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  p_retro_init();
  struct retro_game_info gi = { rom_path, rom, (size_t)rom_size, NULL };
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  if (!nohook) p_set_cpu_hook(hook);

  FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
  if (csv) {
    fprintf(csv, "frame,frame_cycles,busy_cycles,load_pct,idle_line,loop_start,dropped,dma_vblank_bytes,dma_active_bytes,last_dma_line,cpu_vdp_bytes");
    for (int r = 0; r < nram; r++) fprintf(csv, ",%s", ram_name[r]);
    fprintf(csv, "\n");
  }

  /* worst frames: keep the TOP_N frames by busy cycles with their top functions */
  typedef struct { int frame; uint64_t busy; int idle_line; kv_t fns[TOP_N]; } worst_t;
  worst_t worst[TOP_N]; memset(worst, 0, sizeof worst);
  for (int k = 0; k < TOP_N; k++) worst[k].frame = -1;

  int missed = 0, dma_spill_frames = 0, seen_loop = 0;
  uint64_t sum_busy = 0, sum_frame = 0, max_dma = 0;
  int max_dma_frame = -1;
  uint8_t *ram = work_ram;
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  for (int fno = 0; fno < frames; fno++) {
    memset(&cur, 0, sizeof cur);
    cur.idle_line = -1; cur.last_dma_line = -1;
    in_idle_seen = 0;
    memset(fn_frame, 0, sizeof(uint64_t) * (nsyms + 2));

    cur_frame = fno;
    p_retro_run();

    /* the core subtracts one frame of master clocks from m68k.cycles at the
       end of each frame; shift our reference by the same amount */
    int64_t frame_mclk = (int64_t)lines_per_frame * MCLK_PER_LINE;
    last_cycles -= frame_mclk;
    cur.frame_mclk = frame_mclk;

    for (int r = 0; r < nram; r++) {
      uint32_t a = ram_addr[r] & 0xffff;
      /* GPGX keeps work RAM byte-swapped on little-endian hosts */
      cur.ramw[r] = *(uint16_t *)(ram + (a & ~1u));
    }

    double load = 100.0 * cur.busy_mclk / frame_mclk;
    /* dropped: no new game loop started at this VBlank, so the screen shows the
       previous frame again. Counted once the first loop has started (boot). */
    if (cur.loop_start) seen_loop = 1;
    int is_missed = nidle > 0 && seen_loop && !cur.loop_start;
    int counted = fno >= from;  /* --from N: leave boot frames out of the summary */
    if (counted) {
      if (is_missed) missed++;
      if (cur.dma_active) dma_spill_frames++;
      uint32_t dma_all = cur.dma_vblank + cur.dma_active;
      if (dma_all > max_dma) { max_dma = dma_all; max_dma_frame = fno; }
      sum_busy += cur.busy_mclk; sum_frame += frame_mclk;
    } else {
      /* keep the run-wide profile to the counted frames too */
      for (int i = 0; i < nsyms + 2; i++) fn_total[i] -= fn_frame[i];
    }

    if (csv) {
      fprintf(csv, "%d,%llu,%llu,%.1f,%d,%d,%d,%u,%u,%d,%u", fno,
              (unsigned long long)(frame_mclk / MCLK_PER_68K),
              (unsigned long long)(cur.busy_mclk / MCLK_PER_68K), load, cur.idle_line,
              cur.loop_start, is_missed, cur.dma_vblank, cur.dma_active, cur.last_dma_line, cur.cpu_vram);
      for (int r = 0; r < nram; r++) fprintf(csv, ",%u", cur.ramw[r]);
      fprintf(csv, "\n");
    }

    for (int k = 0; k < TOP_N && counted; k++) {
      if (worst[k].frame < 0 || cur.busy_mclk > worst[k].busy) {
        memmove(&worst[k + 1], &worst[k], sizeof(worst_t) * (TOP_N - k - 1));
        worst[k].frame = fno; worst[k].busy = cur.busy_mclk; worst[k].idle_line = cur.idle_line;
        top_fns(fn_frame, worst[k].fns, TOP_N, 1);
        break;
      }
    }
    for (int s = 0; s < nshots; s++) if (shot_frame[s] == fno) write_ppm(shot_path[s]);
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
  if (csv) fclose(csv);

  kv_t top[40];
  top_fns(fn_total, top, 40, 0);
  uint64_t all = 0;
  for (int i = 0; i < nsyms + 2; i++) all += fn_total[i];

  printf("frames %d (summary from frame %d), wall %.2f s (%.0f frames/s), symbols %d%s\n", frames, from, secs,
         frames / secs, nsyms, nohook ? ", hook off" : "");
  if (nohook) return 0;
  printf("mean CPU load %.1f%% (busy cycles / frame cycles)\n", 100.0 * sum_busy / sum_frame);
  printf("dropped frames (no new game loop started at that VBlank; idle = %s): %d\n", idle_list, missed);
  printf("frames with DMA during active display: %d; max DMA %llu bytes in frame %d\n",
         dma_spill_frames, (unsigned long long)max_dma, max_dma_frame);
  printf("\nwhole run, cycles by function:\n");
  for (int k = 0; k < 12 && top[k].idx >= 0; k++)
    printf("  %5.1f%%  %s\n", 100.0 * top[k].v / all, sym_name(top[k].idx));
  printf("\nbusiest frames:\n");
  for (int k = 0; k < TOP_N && worst[k].frame >= 0; k++) {
    printf("  frame %d: %llu busy cycles (%.0f%% of a frame), idle entered at line %d\n", worst[k].frame,
           (unsigned long long)(worst[k].busy / MCLK_PER_68K),
           100.0 * worst[k].busy / ((double)lines_per_frame * MCLK_PER_LINE), worst[k].idle_line);
    for (int j = 0; j < 4 && worst[k].fns[j].idx >= 0; j++)
      printf("      %7llu  %s\n", (unsigned long long)(worst[k].fns[j].v / MCLK_PER_68K),
             sym_name(worst[k].fns[j].idx));
  }

  if (json_path) {
    FILE *j = fopen(json_path, "w");
    fprintf(j, "{\"frames\":%d,\"from\":%d,\"wall_s\":%.3f,\"mean_load_pct\":%.2f,\"dropped_frames\":%d,"
               "\"dma_active_frames\":%d,\"max_dma_bytes\":%llu,\"max_dma_frame\":%d,\"profile\":[",
            frames, from, secs, 100.0 * sum_busy / sum_frame, missed, dma_spill_frames,
            (unsigned long long)max_dma, max_dma_frame);
    for (int k = 0; k < 40 && top[k].idx >= 0; k++)
      fprintf(j, "%s{\"fn\":\"%s\",\"cycles\":%llu,\"pct\":%.2f}", k ? "," : "", sym_name(top[k].idx),
              (unsigned long long)(top[k].v / MCLK_PER_68K), 100.0 * top[k].v / all);
    fprintf(j, "],\"busiest\":[");
    for (int k = 0; k < TOP_N && worst[k].frame >= 0; k++) {
      fprintf(j, "%s{\"frame\":%d,\"busy_cycles\":%llu,\"idle_line\":%d,\"fns\":[", k ? "," : "",
              worst[k].frame, (unsigned long long)(worst[k].busy / MCLK_PER_68K), worst[k].idle_line);
      for (int q = 0; q < TOP_N && worst[k].fns[q].idx >= 0; q++)
        fprintf(j, "%s{\"fn\":\"%s\",\"cycles\":%llu}", q ? "," : "", sym_name(worst[k].fns[q].idx),
                (unsigned long long)(worst[k].fns[q].v / MCLK_PER_68K));
      fprintf(j, "]}");
    }
    fprintf(j, "]}\n");
    fclose(j);
  }
  return 0;
}
