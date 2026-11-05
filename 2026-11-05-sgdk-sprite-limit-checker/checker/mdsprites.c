/*
 * mdsprites: a headless sprite-limit checker for Mega Drive / Genesis ROMs.
 *
 * It loads the Genesis Plus GX libretro core (built with HOOK_CPU=1, see
 * build.sh), runs a ROM for N frames without a window and, after each frame,
 * walks the sprite list the way the VDP does for every visible line:
 *   - follow the link chain from entry 0, reading Y, size and link from the
 *     VDP's internal cache and X and attributes from VRAM
 *   - stop listing at the per-line sprite limit (20 in H40, 16 in H32)
 *   - stop drawing at the per-line dot limit (320 in H40, 256 in H32)
 *   - apply sprite masking (X = 0)
 * Off-screen sprites use slots and dots like any other. For every line it
 * records which sprites were dropped or cut, and why.
 *
 * To check itself it can compare with the real picture: run once with
 * --nolimit --dump f (the core's "no sprite limit" option, frames saved to f),
 * then with --truth f. A line whose pixels differ really lost sprite pixels.
 *
 * Usage: mdsprites <core.so> <rom.bin> <symbol.txt> <frames> [options]
 *   --csv file       per-frame CSV
 *   --events file    one row per sprite that lost pixels in a frame
 *   --ram ADDR:name  add the work RAM word at ADDR to the CSV
 *   --input F-T:BTN  hold pad 1 buttons (U D L R A B C S) from frame F to T
 *   --shot N:file    write video frame N as a PPM image (repeatable)
 *   --idle a,b       idle functions (default VDP_waitVBlank,VDP_waitVSync)
 *   --nolimit        turn on the core's option that removes the limits
 *   --dump file      save every frame's pixels (with --nolimit)
 *   --truth file     compare every frame's pixels with a --dump file
 *
 * MIT license (this file). The emulator core it loads has its own license.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libretro.h"
/* keep the core's stdio-to-VFS macros (fopen -> rfopen ...) out of this file */
#define __LIBRETRO_SDK_FILE_STREAM_TRANSFORMS_H
#define INLINE static inline
#include "shared.h" /* Genesis Plus GX internals: sat, vram, satb, status, ... */

#define MCLK_PER_LINE 3420
#define MCLK_PER_68K 7
#define MAX_SYMS 20000
#define MAX_LINES 240
#define MAX_SAT 80
#define MAX_RAMW 8
#define MAX_SHOTS 16
#define MAX_PAD_RULES 64

enum { LOSS_NONE, LOSS_COUNT, LOSS_DOTS, LOSS_CUT };
static const char *loss_name[] = {"", "sprite_limit", "dot_limit", "cut_by_dot_limit"};

/* --- result of the walk for one frame ------------------------------------ */
typedef struct {
  int line_count[MAX_LINES];    /* sprites on the line, off-screen included */
  int line_dots[MAX_LINES];     /* their width in pixels */
  int line_off[MAX_LINES];      /* of those, entirely off-screen */
  uint8_t line_loss[MAX_LINES]; /* an on-screen sprite lost pixels here */
  uint8_t line_over[MAX_LINES]; /* a limit was hit here at all */
  int loss_lines[MAX_SAT];      /* per SAT entry: lines that lost pixels */
  int first_line[MAX_SAT];
  uint8_t reason[MAX_SAT];
  int chain;                    /* entries in the link chain */
  int masked_lines;
} walk_t;

static walk_t w;

static void note_loss(int line, int link, int why) {
  w.line_loss[line] = 1;
  if (!w.loss_lines[link]) w.first_line[link] = line;
  w.loss_lines[link]++;
  if (!w.reason[link]) w.reason[link] = (uint8_t)why;
}

static void walk_sprites(void) {
  const uint16_t *q = (const uint16_t *)sat;
  const uint16_t *p = (const uint16_t *)&vram[satb];
  int vw = bitmap.viewport.w, vh = bitmap.viewport.h;
  int max_count = vw >> 4;      /* 20 in H40, 16 in H32 */
  int max_dots = vw;            /* 320 in H40, 256 in H32 */
  int max_total = vw >> 2;      /* 80 in H40, 64 in H32 */
  int carry = 0;                /* masking state left by the previous line */
  memset(&w, 0, sizeof w);
  if (vh > MAX_LINES) vh = MAX_LINES;
  for (int s = 0; s < MAX_SAT; s++) w.first_line[s] = -1;

  { /* the chain itself, for the frame total */
    int l = 0, n = 0;
    do { n++; l = q[l * 4 + 1] & 0x7f; } while (l && l < max_total && n < max_total);
    w.chain = n;
  }

  for (int line = 0; line < vh; line++) {
    int vline = line + 0x80, link = 0, seen = 0, dots = 0, total = max_total;
    int masked = 0, ovr = carry, stopped = LOSS_NONE, off = 0;
    do {
      int y = q[link * 4] & 0x1ff;
      int size = q[link * 4 + 1] >> 8;
      int h = 8 + ((size & 3) << 3);
      if (vline >= y && vline - y < h) {
        int x = p[link * 4 + 3] & 0x1ff;
        int width = 8 + ((size & 0x0c) << 1);
        int sx = x - 0x80;
        int onscreen = sx + width > 0 && sx < vw;
        if (!stopped && seen == max_count) stopped = LOSS_COUNT;
        if (stopped) {
          /* over a limit: nothing further on this line is drawn */
          w.line_over[line] = 1;
          if (onscreen && !masked) note_loss(line, link, stopped);
        } else {
          seen++;
          if (x) ovr = 1;
          else if (ovr) { if (!masked) w.masked_lines++; masked = 1; }
          dots += width;
          if (!onscreen) off++;
          if (dots > max_dots) {
            /* this one is cut at the limit, the rest are gone */
            w.line_over[line] = 1;
            if (onscreen && !masked) note_loss(line, link, LOSS_CUT);
          }
          if (dots >= max_dots) stopped = LOSS_DOTS;
        }
      }
      link = q[link * 4 + 1] & 0x7f;
    } while (link && link < max_total && --total);
    w.line_count[line] = seen;
    w.line_dots[line] = dots;
    w.line_off[line] = off;
    carry = (dots >= vw) ? 1 : 0;
  }
}

/* --- symbols and the hook (busy cycles, overflow flag, late SAT writes) --- */
typedef struct { uint32_t addr; char name[96]; } sym_t;
static sym_t syms[MAX_SYMS];
static int nsyms;
static uint32_t idle_lo[16], idle_hi[16];
static int nidle;

static int cmp_sym(const void *a, const void *b) {
  uint32_t x = ((const sym_t *)a)->addr, y = ((const sym_t *)b)->addr;
  return x < y ? -1 : x > y;
}

static void load_symbols(const char *path, const char *idle_list) {
  FILE *f = fopen(path, "r");
  if (!f) { perror(path); exit(1); }
  char line[512];
  while (fgets(line, sizeof line, f) && nsyms < MAX_SYMS) {
    unsigned long addr; char type, name[256];
    if (sscanf(line, "%lx %c %255s", &addr, &type, name) != 3) continue;
    if ((type != 'T' && type != 't') || name[0] == '.') continue;
    syms[nsyms].addr = (uint32_t)addr;
    snprintf(syms[nsyms].name, sizeof syms[nsyms].name, "%s", name);
    nsyms++;
  }
  fclose(f);
  qsort(syms, nsyms, sizeof(sym_t), cmp_sym);
  char buf[512];
  snprintf(buf, sizeof buf, "%s", idle_list);
  for (char *t = strtok(buf, ","); t && nidle < 16; t = strtok(NULL, ","))
    for (int i = 0; i + 1 < nsyms; i++)
      if (!strcmp(syms[i].name, t)) {
        idle_lo[nidle] = syms[i].addr;
        idle_hi[nidle++] = syms[i + 1].addr;
        break;
      }
}

static int is_idle(uint32_t pc) {
  pc &= 0xffffff;
  for (int i = 0; i < nidle; i++) if (pc >= idle_lo[i] && pc < idle_hi[i]) return 1;
  return 0;
}

static int64_t last_cycles;
static uint32_t last_pc;
static int have_last;
static uint64_t busy_mclk;
static int flag_seen, sat_writes_active;

static void hook(hook_type_t type, int width, unsigned int address, unsigned int value) {
  (void)width; (void)value;
  if (type == HOOK_M68K_E) {
    int64_t now = m68k.cycles;
    if (have_last && now > last_cycles && !is_idle(last_pc)) busy_mclk += now - last_cycles;
    last_cycles = now;
    last_pc = address;
    have_last = 1;
    if (status & 0x40) flag_seen = 1;
    return;
  }
  /* The walk runs once per frame. Writes to the table during active
     display (raster tricks, or DMA that ran late) make it an estimate. */
  if (type == HOOK_VRAM_W && (reg[1] & 0x40) && v_counter < bitmap.viewport.h &&
      address >= satb && address < (unsigned)satb + 0x280)
    sat_writes_active++;
}

/* --- libretro frontend glue --------------------------------------------- */
static int opt_nolimit;
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
      if (opt_nolimit && v->key && !strcmp(v->key, "genesis_plus_gx_no_sprite_limit")) {
        v->value = "enabled";
        return true;
      }
      return false;
    }
    default:
      return false;
  }
}
static const void *last_fb;
static unsigned last_w, last_h;
static size_t last_pitch;
static void video_cb(const void *d, unsigned vw, unsigned vh, size_t pitch) {
  if (d) { last_fb = d; last_w = vw; last_h = vh; last_pitch = pitch; }
}
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t f) { (void)d; return f; }
static struct { int from, to; int16_t mask; } inputs[MAX_PAD_RULES];
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
  if (argc < 5) {
    fprintf(stderr, "usage: %s core.so rom.bin symbol.txt frames [options, see the source header]\n", argv[0]);
    return 2;
  }
  const char *core_path = argv[1], *rom_path = argv[2], *sym_path = argv[3];
  int frames = atoi(argv[4]);
  const char *idle_list = "VDP_waitVBlank,VDP_waitVSync";
  const char *csv_path = NULL, *ev_path = NULL, *dump_path = NULL, *truth_path = NULL;
  int shot_frame[MAX_SHOTS]; const char *shot_path[MAX_SHOTS]; int nshots = 0;
  uint32_t ram_addr[MAX_RAMW]; const char *ram_name[MAX_RAMW]; int nram = 0;
  for (int i = 5; i < argc; i++) {
    if (!strcmp(argv[i], "--idle") && i + 1 < argc) idle_list = argv[++i];
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else if (!strcmp(argv[i], "--events") && i + 1 < argc) ev_path = argv[++i];
    else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump_path = argv[++i];
    else if (!strcmp(argv[i], "--truth") && i + 1 < argc) truth_path = argv[++i];
    else if (!strcmp(argv[i], "--nolimit")) opt_nolimit = 1;
    else if (!strcmp(argv[i], "--input") && i + 1 < argc && ninputs < MAX_PAD_RULES) {
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
  load_symbols(sym_path, idle_list);

  p_retro_set_environment(env_cb);
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  p_retro_init();
  struct retro_game_info gi = { rom_path, rom, (size_t)rom_size, NULL };
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  p_set_cpu_hook(hook);

  FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
  FILE *ev = ev_path ? fopen(ev_path, "w") : NULL;
  FILE *dump = dump_path ? fopen(dump_path, "wb") : NULL;
  FILE *truth = truth_path ? fopen(truth_path, "rb") : NULL;
  if (truth_path && !truth) { perror(truth_path); return 1; }
  if (csv) {
    fprintf(csv, "frame,busy_cycles,load_pct,chain,max_count,max_dots,over_lines,loss_lines,sprites_lost,"
                 "off_slots_on_loss_lines,masked_lines,flag,late_sat_writes,real_lines,real_pixels,tp,fp,fn");
    for (int r = 0; r < nram; r++) fprintf(csv, ",%s", ram_name[r]);
    fprintf(csv, "\n");
  }
  if (ev) fprintf(ev, "frame,sat,tile,x,y,w,h,first_line,lines,reason\n");

  uint64_t tot_over = 0, tot_loss = 0, tp_all = 0, fp_all = 0, fn_all = 0, real_all = 0;
  int frames_loss = 0, frames_late = 0, frames_flag = 0, worst_frame = -1, worst_lost = 0, fn_first = -1;
  uint16_t *fbuf = malloc(320 * 240 * 2);
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  for (int fno = 0; fno < frames; fno++) {
    busy_mclk = 0; flag_seen = 0; sat_writes_active = 0;
    cur_frame = fno;
    p_retro_run();
    /* a core frame runs VBlank first, then the visible lines, so the table is
       now the one those lines used, unless it was written during them */
    walk_sprites();
    int64_t frame_mclk = (int64_t)lines_per_frame * MCLK_PER_LINE;
    last_cycles -= frame_mclk; /* the core rebases m68k.cycles each frame */

    int vh = (int)last_h < MAX_LINES ? (int)last_h : MAX_LINES;
    int over = 0, loss = 0, off_on_loss = 0, max_count = 0, max_dots = 0, lost = 0;
    for (int l = 0; l < vh; l++) {
      over += w.line_over[l];
      loss += w.line_loss[l];
      if (w.line_loss[l]) off_on_loss += w.line_off[l];
      if (w.line_count[l] > max_count) max_count = w.line_count[l];
      if (w.line_dots[l] > max_dots) max_dots = w.line_dots[l];
    }
    for (int s = 0; s < MAX_SAT; s++) if (w.loss_lines[s]) lost++;

    int real_lines = 0, real_px = 0, tp = 0, fp = 0, fn = 0;
    if (dump) {
      for (unsigned y = 0; y < last_h; y++)
        fwrite((const uint8_t *)last_fb + y * last_pitch, 2, last_w, dump);
    }
    if (truth) {
      for (unsigned y = 0; y < last_h; y++) {
        const uint16_t *row = (const uint16_t *)((const uint8_t *)last_fb + y * last_pitch);
        if (fread(fbuf, 2, last_w, truth) != last_w) { fprintf(stderr, "truth file too short\n"); return 1; }
        int d = 0;
        for (unsigned x = 0; x < last_w; x++) if (row[x] != fbuf[x]) d++;
        real_px += d;
        if (d) real_lines++;
        int pred = y < MAX_LINES && w.line_loss[y];
        if (pred && d) tp++;
        else if (pred) fp++;
        else if (d) { fn++; if (fn_first < 0) fn_first = fno; }
      }
    }

    if (loss) frames_loss++;
    if (sat_writes_active) frames_late++;
    if (flag_seen) frames_flag++;
    tot_over += over; tot_loss += loss; tp_all += tp; fp_all += fp; fn_all += fn; real_all += real_lines;
    if (lost > worst_lost) { worst_lost = lost; worst_frame = fno; }

    if (csv) {
      fprintf(csv, "%d,%llu,%.1f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", fno,
              (unsigned long long)(busy_mclk / MCLK_PER_68K), 100.0 * busy_mclk / frame_mclk, w.chain, max_count,
              max_dots, over, loss, lost, off_on_loss, w.masked_lines, flag_seen, sat_writes_active, real_lines,
              real_px, tp, fp, fn);
      for (int r = 0; r < nram; r++) {
        /* the core keeps work RAM byte-swapped on little-endian hosts */
        uint32_t a = ram_addr[r] & 0xffff;
        fprintf(csv, ",%u", *(uint16_t *)(work_ram + (a & ~1u)));
      }
      fprintf(csv, "\n");
    }
    if (ev) {
      const uint16_t *q = (const uint16_t *)sat;
      const uint16_t *p = (const uint16_t *)&vram[satb];
      for (int s = 0; s < MAX_SAT; s++) {
        if (!w.loss_lines[s]) continue;
        int size = q[s * 4 + 1] >> 8;
        fprintf(ev, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%s\n", fno, s, p[s * 4 + 2] & 0x7ff, (p[s * 4 + 3] & 0x1ff) - 0x80,
                (q[s * 4] & 0x1ff) - 0x80, 8 + ((size & 0x0c) << 1), 8 + ((size & 3) << 3), w.first_line[s],
                w.loss_lines[s], loss_name[w.reason[s]]);
      }
    }
    for (int s = 0; s < nshots; s++) if (shot_frame[s] == fno) write_ppm(shot_path[s]);
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
  if (csv) fclose(csv);
  if (ev) fclose(ev);
  if (dump) fclose(dump);
  if (truth) fclose(truth);

  printf("frames %d, wall %.2f s (%.0f frames/s)%s\n", frames, secs, frames / secs, opt_nolimit ? ", limits off" : "");
  printf("frames with lost sprite pixels: %d; lines over a limit: %llu; lines with on-screen loss: %llu\n", frames_loss,
         (unsigned long long)tot_over, (unsigned long long)tot_loss);
  printf("most sprites lost in one frame: %d (frame %d)\n", worst_lost, worst_frame);
  printf("frames with the VDP overflow flag set: %d\n", frames_flag);
  printf("frames with sprite table writes during active display: %d\n", frames_late);
  if (truth)
    printf("against the picture: %llu lines really differ; predicted and real %llu, predicted only %llu, real only %llu (first real-only frame %d)\n",
           (unsigned long long)real_all, (unsigned long long)tp_all, (unsigned long long)fp_all,
           (unsigned long long)fn_all, fn_first);
  return 0;
}
