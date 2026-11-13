/*
 * nessprites: a headless sprites-per-scanline checker for NES ROMs.
 *
 * It loads a libretro NES core, runs a ROM for N frames without a window and,
 * after each frame, evaluates OAM the way the PPU does for every line:
 *   - entries 0..63 in order; an entry is "in range" on evaluation line L if
 *     L - Y is 0..7 (0..15 in 8x16 mode), and it is drawn on line L + 1
 *   - the first 8 in range are drawn, the rest are lost on that line
 *   - the sprite overflow flag ($2002 bit 5) as the real PPU sets it, with
 *     its hardware bug (after 8 hits it also steps through the other bytes
 *     of each entry and compares those as if they were Y), next to the flag
 *     an exact implementation would set
 * A lost sprite counts as visible loss only if its pattern row on that line
 * has an opaque pixel on screen (CHR read through the core; left-column mask
 * and the right edge applied). Lost rows that are all transparent are counted
 * apart: they still used a slot.
 *
 * With FCEUmm it reads the PPU's own OAM (SPRAM), PPUCTRL and the 256x240
 * picture (XBuf) from the core. To check itself it compares with the real
 * picture: run once with --nolimit --dump f (the core's "no sprite limit"
 * option, frames saved to f), then with --truth f. A line whose pixels differ
 * really lost sprite pixels.
 *
 * With other cores (Mesen) it reads only work RAM, so it can log what the ROM
 * itself saw: --flagaddr is a RAM byte where the ROM stores $2002 bit 5 in its
 * NMI. That is how the flag model is checked against an emulator that
 * implements the hardware bug.
 *
 * Usage: nessprites <core.so> <rom.nes> <frames> [options]
 *   --csv file       per-frame CSV
 *   --events file    one row per sprite that lost pixels in a frame
 *   --ram ADDR:name  add the work RAM byte at ADDR (hex) to the CSV
 *   --flagaddr ADDR  RAM byte holding the overflow flag the ROM read (hex)
 *   --input F-T:BTN  hold pad 1 buttons (U D L R A B S=start E=select) from frame F to T
 *   --shot N:file    write video frame N as a PPM image (repeatable)
 *   --nolimit        turn on the core's option that removes the 8-per-line limit
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

#define LINES 240
#define MAX_SHOTS 16
#define MAX_PAD_RULES 64
#define MAX_RAMB 8

/* --- the walk ------------------------------------------------------------- */
typedef struct {
  int line_count[LINES + 1];   /* sprites in range, by drawn line (1..240) */
  uint8_t line_loss[LINES];    /* a visible sprite lost pixels on this drawn line */
  int loss_lines[64];          /* per OAM entry: drawn lines it lost with visible pixels */
  int clear_lines;             /* lost rows that were transparent anyway */
  int first_line[64];
  int over_lines;              /* evaluation lines with more than 8 in range */
  int flag_exact;              /* any line had more than 8 */
  int flag_hw;                 /* the real PPU's flag, bug included */
  int hw_first_line;           /* evaluation line that set it */
} walk_t;
static walk_t w;

static int in_range(int line, uint8_t y, int h) { return (unsigned)(line - y) < (unsigned)h; }

/* FCEUmm's CHR mapping: 8 pointers to 1 KB pages, offset by the page base */
static uint8_t **vpage;

/* does sprite row `row` of this entry put an opaque pixel on screen? */
static int row_visible(const uint8_t *e, int row, int tall, int ctrl, int show_left) {
  if (!vpage) return 1;
  int tile = e[1], attr = e[2], x = e[3];
  int h = tall ? 16 : 8;
  if (attr & 0x80) row = h - 1 - row;
  unsigned a;
  if (tall) a = ((tile & 1) << 12) + ((tile & 0xFE) << 4) + (row & 7) + ((row & 8) << 1);
  else a = (tile << 4) + ((ctrl & 0x08) ? 0x1000 : 0) + row;
  const uint8_t *c = &vpage[a >> 10][a];
  int bits = c[0] | c[8];
  for (int col = 0; col < 8; col++) {
    int bit = (attr & 0x40) ? col : 7 - col;   /* horizontal flip */
    int sx = x + col;
    if (sx > 255 || (sx < 8 && !show_left)) continue;
    if (bits & (1 << bit)) return 1;
  }
  return 0;
}

static void walk(const uint8_t *oam, int tall, int ctrl, int show_left) {
  int h = tall ? 16 : 8;
  memset(&w, 0, sizeof w);
  w.hw_first_line = -1;
  for (int s = 0; s < 64; s++) w.first_line[s] = -1;
  for (int line = 0; line < LINES; line++) {
    int found = 0, n;
    /* what is drawn and what is lost */
    for (n = 0; n < 64; n++) {
      if (!in_range(line, oam[n * 4], h)) continue;
      if (found < 8) { found++; continue; }
      found++;
      int drawn = line + 1;
      if (drawn >= LINES) continue;
      if (!row_visible(&oam[n * 4], line - oam[n * 4], tall, ctrl, show_left)) { w.clear_lines++; continue; }
      w.line_loss[drawn] = 1;
      if (!w.loss_lines[n]) w.first_line[n] = drawn;
      w.loss_lines[n]++;
    }
    if (line + 1 <= LINES) w.line_count[line + 1] = found;
    if (found > 8) { w.over_lines++; w.flag_exact = 1; }

    /* the PPU's own evaluation, with the overflow bug (NESdev wiki,
       "PPU sprite evaluation"): after 8 hits it keeps reading, but steps
       n and m together on every miss, so it compares tile, attribute and
       X bytes as if they were Y */
    found = 0;
    for (n = 0; n < 64 && found < 8; n++)
      if (in_range(line, oam[n * 4], h)) found++;
    if (found == 8) {
      int m = 0;
      while (n < 64) {
        if (in_range(line, oam[n * 4 + m], h)) {
          if (!w.flag_hw) w.hw_first_line = line;
          w.flag_hw = 1;
          break;
        }
        n++;
        m = (m + 1) & 3;
      }
    }
  }
}

/* --- libretro frontend glue --------------------------------------------- */
static int opt_nolimit;
static enum retro_pixel_format pixfmt = RETRO_PIXEL_FORMAT_0RGB1555;
static bool env_cb(unsigned cmd, void *data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char **)data = "/tmp";
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
      enum retro_pixel_format f = *(enum retro_pixel_format *)data;
      if (f == RETRO_PIXEL_FORMAT_RGB565 || f == RETRO_PIXEL_FORMAT_XRGB8888) { pixfmt = f; return true; }
      return false;
    }
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
      return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      *(bool *)data = true;
      return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
      struct retro_variable *v = data;
      if (!v->key) return false;
      if (!strcmp(v->key, "fceumm_nospritelimit") || !strcmp(v->key, "mesen_nospritelimit")) {
        v->value = opt_nolimit ? "enabled" : "disabled";
        return true;
      }
      /* full 256x240 picture, no cropping */
      if (!strncmp(v->key, "fceumm_overscan_", 16) || !strncmp(v->key, "mesen_overscan_", 15)) {
        v->value = "0";
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
    case 'A': m |= 1 << RETRO_DEVICE_ID_JOYPAD_A; break;
    case 'B': m |= 1 << RETRO_DEVICE_ID_JOYPAD_B; break;
    case 'S': m |= 1 << RETRO_DEVICE_ID_JOYPAD_START; break;
    case 'E': m |= 1 << RETRO_DEVICE_ID_JOYPAD_SELECT; break;
  }
  return m;
}
static void input_poll_cb(void) {}
static int16_t input_state_cb(unsigned p, unsigned dev, unsigned idx, unsigned id) {
  (void)idx;
  if (p != 0 || dev != RETRO_DEVICE_JOYPAD) return 0;
  int16_t m = 0;
  for (int i = 0; i < ninputs; i++)
    if (cur_frame >= inputs[i].from && cur_frame <= inputs[i].to) m |= inputs[i].mask;
  if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return m;
  return (m >> id) & 1;
}

static void write_ppm(const char *path) {
  if (!last_fb) return;
  FILE *f = fopen(path, "wb");
  if (!f) { perror(path); return; }
  fprintf(f, "P6\n%u %u\n255\n", last_w, last_h);
  for (unsigned y = 0; y < last_h; y++) {
    const uint8_t *row = (const uint8_t *)last_fb + y * last_pitch;
    for (unsigned x = 0; x < last_w; x++) {
      unsigned char rgb[3];
      if (pixfmt == RETRO_PIXEL_FORMAT_XRGB8888) {
        uint32_t c = ((const uint32_t *)row)[x];
        rgb[0] = (c >> 16) & 255; rgb[1] = (c >> 8) & 255; rgb[2] = c & 255;
      } else {
        uint16_t c = ((const uint16_t *)row)[x];
        rgb[0] = ((c >> 11) & 31) * 255 / 31; rgb[1] = ((c >> 5) & 63) * 255 / 63; rgb[2] = (c & 31) * 255 / 31;
      }
      fwrite(rgb, 1, 3, f);
    }
  }
  fclose(f);
}

#define LOAD(name) do { *(void **)&p_##name = dlsym(core, #name); \
  if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; } } while (0)

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s core.so rom.nes frames [options, see the source header]\n", argv[0]);
    return 2;
  }
  const char *core_path = argv[1], *rom_path = argv[2];
  int frames = atoi(argv[3]);
  const char *csv_path = NULL, *ev_path = NULL, *dump_path = NULL, *truth_path = NULL;
  int shot_frame[MAX_SHOTS]; const char *shot_path[MAX_SHOTS]; int nshots = 0;
  unsigned ram_addr[MAX_RAMB]; const char *ram_name[MAX_RAMB]; int nram = 0;
  int flag_addr = -1;
  for (int i = 4; i < argc; i++) {
    if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else if (!strcmp(argv[i], "--events") && i + 1 < argc) ev_path = argv[++i];
    else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump_path = argv[++i];
    else if (!strcmp(argv[i], "--truth") && i + 1 < argc) truth_path = argv[++i];
    else if (!strcmp(argv[i], "--nolimit")) opt_nolimit = 1;
    else if (!strcmp(argv[i], "--flagaddr") && i + 1 < argc) flag_addr = (int)strtoul(argv[++i], NULL, 16);
    else if (!strcmp(argv[i], "--input") && i + 1 < argc && ninputs < MAX_PAD_RULES) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c || sscanf(a, "%d-%d", &inputs[ninputs].from, &inputs[ninputs].to) != 2) return 2;
      inputs[ninputs++].mask = parse_buttons(c + 1);
    } else if (!strcmp(argv[i], "--shot") && i + 1 < argc && nshots < MAX_SHOTS) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c) return 2;
      *c = 0; shot_frame[nshots] = atoi(a); shot_path[nshots++] = c + 1;
    } else if (!strcmp(argv[i], "--ram") && i + 1 < argc && nram < MAX_RAMB) {
      char *a = argv[++i], *c = strchr(a, ':');
      if (!c) return 2;
      *c = 0; ram_addr[nram] = (unsigned)strtoul(a, NULL, 16) & 0x7ff; ram_name[nram++] = c + 1;
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
  void *(*p_retro_get_memory_data)(unsigned);
  void (*p_retro_set_controller_port_device)(unsigned, unsigned);
  LOAD(retro_set_environment); LOAD(retro_set_video_refresh); LOAD(retro_set_audio_sample);
  LOAD(retro_set_audio_sample_batch); LOAD(retro_set_input_poll); LOAD(retro_set_input_state);
  LOAD(retro_init); LOAD(retro_load_game); LOAD(retro_run); LOAD(retro_get_memory_data);
  LOAD(retro_set_controller_port_device);

  /* FCEUmm internals (exported because build.sh links without the version script).
     VPage is uint8_t *VPage[8]: dlsym returns the address of the array itself. */
  const uint8_t *spram = dlsym(core, "SPRAM");
  const uint8_t *ppu = dlsym(core, "PPU");
  uint8_t **xbuf = dlsym(core, "XBuf");
  vpage = dlsym(core, "VPage");
  int full = spram && ppu && xbuf && vpage;
  if (!full && (csv_path || ev_path || truth_path || dump_path))
    fprintf(stderr, "note: core does not export SPRAM/PPU/XBuf; only RAM columns and the ROM's flag are logged\n");

  FILE *rf = fopen(rom_path, "rb");
  if (!rf) { perror(rom_path); return 1; }
  fseek(rf, 0, SEEK_END); long rom_size = ftell(rf); fseek(rf, 0, SEEK_SET);
  void *rom = malloc(rom_size);
  if (fread(rom, 1, rom_size, rf) != (size_t)rom_size) return 1;
  fclose(rf);

  int trace = getenv("NESSPRITES_TRACE") != NULL;
#define TRACE(m) do { if (trace) { fprintf(stderr, "trace: %s\n", m); fflush(stderr); } } while (0)
  TRACE("set_environment");
  p_retro_set_environment(env_cb);
  TRACE("init");
  p_retro_init();
  /* callbacks after init: Mesen creates the objects that hold them there */
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  TRACE("load_game");
  struct retro_game_info gi = { rom_path, rom, (size_t)rom_size, NULL };
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  TRACE("controller");
  p_retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
  TRACE("memory");
  const uint8_t *ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
  if (!ram) { fprintf(stderr, "core gives no system RAM\n"); return 1; }

  FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
  FILE *ev = ev_path ? fopen(ev_path, "w") : NULL;
  FILE *dump = dump_path ? fopen(dump_path, "wb") : NULL;
  FILE *truth = truth_path ? fopen(truth_path, "rb") : NULL;
  if (truth_path && !truth) { perror(truth_path); return 1; }
  if (csv) {
    fprintf(csv, "frame,tall,max_on_line,over_lines,loss_lines,clear_lost_rows,sprites_lost,flag_exact,flag_hw,flag_hw_line,flag_rom,"
                 "real_lines,real_pixels,tp,fp,fn");
    for (int r = 0; r < nram; r++) fprintf(csv, ",%s", ram_name[r]);
    fprintf(csv, "\n");
  }
  if (ev) fprintf(ev, "frame,oam,tile,x,y,first_line,lines\n");

  uint64_t tot_clear = 0, tot_over = 0, tot_loss = 0, tp_all = 0, fp_all = 0, fn_all = 0, real_all = 0;
  int frames_loss = 0, frames_exact = 0, frames_hw = 0, frames_rom = 0, worst_frame = -1, worst_lost = 0, fn_first = -1;
  int hw_vs_exact_differ = 0;
  uint8_t prev[256 * LINES];
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  for (int fno = 0; fno < frames; fno++) {
    cur_frame = fno;
    if (fno < 2) TRACE("run");
    p_retro_run();
    /* FCEUmm runs VBlank (NMI, OAM DMA) first, then the 240 visible lines,
       so SPRAM is now the table those lines used, unless the ROM wrote it
       during them */
    int tall = 0, lost = 0, loss = 0, maxc = 0;
    if (full) {
      tall = (ppu[0] & 0x20) != 0;
      walk(spram, tall, ppu[0], (ppu[1] & 0x04) != 0);
      for (int l = 0; l < LINES; l++) { loss += w.line_loss[l]; if (w.line_count[l] > maxc) maxc = w.line_count[l]; }
      for (int s = 0; s < 64; s++) if (w.loss_lines[s]) lost++;
    }
    int flag_rom = flag_addr >= 0 ? (ram[flag_addr] & 0x20) != 0 : -1;

    int real_lines = 0, real_px = 0, tp = 0, fp = 0, fn = 0;
    if (full && dump) fwrite(*xbuf, 1, 256 * LINES, dump);
    if (full && truth) {
      if (fread(prev, 1, 256 * LINES, truth) != 256 * LINES) { fprintf(stderr, "truth file too short\n"); return 1; }
      for (int y = 0; y < LINES; y++) {
        int d = 0;
        for (int x = 0; x < 256; x++) if ((*xbuf)[y * 256 + x] != prev[y * 256 + x]) d++;
        real_px += d;
        if (d) real_lines++;
        if (w.line_loss[y] && d) tp++;
        else if (w.line_loss[y]) fp++;
        else if (d) { fn++; if (fn_first < 0) fn_first = fno; }
      }
    }

    if (loss) frames_loss++;
    if (w.flag_exact) frames_exact++;
    if (w.flag_hw) frames_hw++;
    if (w.flag_hw != w.flag_exact) hw_vs_exact_differ++;
    if (flag_rom == 1) frames_rom++;
    tot_clear += w.clear_lines;
    tot_over += w.over_lines; tot_loss += loss; tp_all += tp; fp_all += fp; fn_all += fn; real_all += real_lines;
    if (lost > worst_lost) { worst_lost = lost; worst_frame = fno; }

    if (csv) {
      fprintf(csv, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", fno, tall, maxc, w.over_lines, loss, w.clear_lines, lost,
              w.flag_exact, w.flag_hw, w.hw_first_line, flag_rom, real_lines, real_px, tp, fp, fn);
      for (int r = 0; r < nram; r++) fprintf(csv, ",%u", ram[ram_addr[r]]);
      fprintf(csv, "\n");
    }
    if (ev && full)
      for (int s = 0; s < 64; s++)
        if (w.loss_lines[s])
          fprintf(ev, "%d,%d,%d,%d,%d,%d,%d\n", fno, s, spram[s * 4 + 1], spram[s * 4 + 3], spram[s * 4],
                  w.first_line[s], w.loss_lines[s]);
    for (int s = 0; s < nshots; s++) if (shot_frame[s] == fno) write_ppm(shot_path[s]);
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
  if (csv) fclose(csv);
  if (ev) fclose(ev);
  if (dump) fclose(dump);
  if (truth) fclose(truth);

  printf("frames %d, wall %.2f s (%.0f frames/s)%s\n", frames, secs, frames / secs, opt_nolimit ? ", limit off" : "");
  if (full) {
    printf("frames with lost sprite pixels: %d; evaluation lines over 8: %llu; drawn lines with visible loss: %llu\n",
           frames_loss, (unsigned long long)tot_over, (unsigned long long)tot_loss);
    printf("lost sprite rows that were transparent anyway: %llu\n", (unsigned long long)tot_clear);
    printf("most sprites lost in one frame: %d (frame %d)\n", worst_lost, worst_frame);
    printf("frames over the limit: %d; frames the real PPU would flag (bug model): %d; frames where they disagree: %d\n",
           frames_exact, frames_hw, hw_vs_exact_differ);
  }
  if (flag_addr >= 0) printf("frames where the ROM read the overflow flag set: %d\n", frames_rom);
  if (truth)
    printf("against the picture: %llu lines really differ; predicted and real %llu, predicted only %llu, real only %llu (first real-only frame %d)\n",
           (unsigned long long)real_all, (unsigned long long)tp_all, (unsigned long long)fp_all,
           (unsigned long long)fn_all, fn_first);
  return 0;
}
