/*
 * nesprof: a headless per-function CPU profiler for cc65 NES programs.
 *
 * It loads the FCEUmm libretro core built with checker/fceumm-nesprof.patch
 * (a running cycle counter plus a hook called before every 6502 instruction),
 * runs a ROM for N frames without a window, and charges the cycles between
 * one instruction and the next to the function that holds the first one.
 * Function ranges come from tools/symbols.py (the ld65 debug file).
 *
 *   self       cycles spent in a function's own instructions (cc65 runtime
 *              helpers such as pushax or tosmulax are functions of their own)
 *   inclusive  self plus everything it called (a shadow call stack follows
 *              JSR, RTS, RTI and NMI entry; JMP-based tail calls stay with
 *              the caller). Idle cycles are not passed up to callers, and
 *              the NMI handler's cycles are not charged to the code it
 *              interrupted.
 *   idle       cycles inside the --idle functions (default _wait_frame) and
 *              what they call, outside the NMI handler: waiting for the frame
 *   busy       every other cycle, NMI handler included
 *   game loop  from one exit of the idle function to the next; a loop that
 *              saw two or more NMIs missed a frame (the game ran slow there)
 *
 * The cycles a JSR takes belong to the caller; the 7 cycles of NMI entry
 * belong to the NMI handler. OAM DMA ($4014) stalls are charged to the
 * instruction that wrote $4014 (FCEUmm counts 512 cycles; hardware takes
 * 513 or 514). Only NROM-style ROMs without bank switching are mapped.
 *
 *   nesprof core.so rom.nes frames --sym rom.sym [options]
 *   --sym file        ranges from tools/symbols.py (required)
 *   --idle name       a wait-for-frame function (repeatable; default _wait_frame)
 *   --from F          leave frames before F out of the totals (startup)
 *   --csv file        per frame: cycles, busy, idle, nmi, loops ended, hottest function
 *   --funcs file      per function: self, inclusive, calls, per frame, share of busy
 *   --loops file      per game loop: busy cycles, NMIs seen, three hottest functions
 *   --loopfns file    per game loop and function: self cycles (nonzero only)
 *   --fnframes file   per frame and function: self, inclusive, calls (nonzero only)
 *   --pcs file        cycles per instruction address, and the same with the cycles of cc65
 *                     runtime helpers (names without a leading _) moved to the C call
 *                     site that led to them (tools/lines.py maps both to C lines)
 *   --spikes N file   the N game loops with the most busy cycles (max 10): per loop, the
 *                     cycles of every address with helpers moved to the C call site
 *                     (tools/lines.py --spikes turns them into the hottest lines per loop)
 *   --oam file        per frame, the PPU's OAM (SPRAM) as 512 hex digits
 *   --input F-T:BTN   hold pad 1 buttons (U D L R A B S=start E=select) from frame F to T
 *   --ram ADDR:name   add a work RAM byte (hex address) to the --csv file
 *   --top N           functions in the printed summary (default 12)
 * MIT.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libretro.h"

#define MAXF 1024
#define MAXDEPTH 64
#define MAX_PAD_RULES 32
#define MAX_RAMB 16
#define MAX_IDLE 8

static int fn_at[0x10000];               /* address -> function index, -1 = unmapped */
static char *fn_name[MAXF];
static unsigned fn_start[MAXF];          /* first address of the function's first range */
static int nfn, unmapped;
static int is_idle[MAXF];
static uint64_t self_c[MAXF], incl_c[MAXF], calls[MAXF];
static uint64_t f_self[MAXF], f_incl[MAXF], f_calls[MAXF];   /* this frame */
static uint64_t l_self[MAXF];                                /* this game loop */
static unsigned mark[MAXF], gen;
static uint64_t pc_c[0x10000];           /* cycles per instruction address (--pcs) */
static uint64_t line_c[0x10000];         /* the same, with runtime helper cycles moved to the C call site */
static int is_c[MAXF];
static uint32_t l_line[0x10000];         /* line_c for the current game loop */
#define MAXSPIKE 10
static int nspike;                       /* --spikes N: keep the N loops with the most busy cycles */
static struct spike { int loop, frame, nmis, n; uint64_t busy; uint16_t *addr; uint32_t *cyc; } spikes[MAXSPIKE];                   /* name starts with '_': C (or C-callable asm), not cc65 runtime */

static struct { int f, nmi; uint16_t site; } stk[MAXDEPTH];
static int depth, nmi_depth, overflows;

static uint64_t *cyc;
static uint64_t last_cyc;
static uint16_t last_pc, nmi_addr;
static uint8_t last_op, last_s;
static int have_last, counting;

static uint64_t fr_idle, fr_nmi;
static int fr_loops;
static int was_idle, loop_open, loop_nmis;
static uint64_t loop_busy, loop_start;
static FILE *loops_f, *loopfns_f;
static int nloops, nloops_slow, cur_frame, from_frame;
static uint64_t loops_busy_max;

static const uint8_t *prg;               /* PRG ROM, mapped at $8000 (mirrored) */
static long prg_size;
static unsigned rom_word(unsigned a) {
  if (a < 0x8000 || !prg_size) return 0x10000;
  return prg[(a - 0x8000) % prg_size] | prg[(a + 1 - 0x8000) % prg_size] << 8;
}

static int fn_lookup(const char *name) {
  for (int i = 0; i < nfn; i++) if (!strcmp(fn_name[i], name)) return i;
  return -1;
}

static int fn_of(uint16_t pc) {
  int f = fn_at[pc];
  return f >= 0 ? f : unmapped;
}

/* waiting: an --idle function (or something it called) runs outside the NMI */
static int in_idle(int f) {
  if (is_idle[f]) return 1;
  for (int i = depth - 1; i >= 0; i--) {
    if (stk[i].nmi) return 0;
    if (is_idle[stk[i].f]) return 1;
  }
  return 0;
}

static void charge(int f, uint64_t d, int nmi_now) {
  if (!counting || !d) return;
  self_c[f] += d; f_self[f] += d; l_self[f] += d;
  gen++;
  mark[f] = gen; incl_c[f] += d; f_incl[f] += d;
  /* callers up to the NMI frame if there is one (the NMI was not called by
     what it interrupted); waiting is not charged to callers */
  int idle = !nmi_now && in_idle(f);
  if (!idle)
    for (int i = depth - 1; i >= 0; i--) {
      int e = stk[i].f;
      if (mark[e] != gen) { mark[e] = gen; incl_c[e] += d; f_incl[e] += d; }
      if (stk[i].nmi) break;
    }
  if (nmi_now) fr_nmi += d;
  if (idle) fr_idle += d;
  else loop_busy += d;
}

static void top3(char *out, size_t n) {
  int best[3] = {-1, -1, -1};
  for (int i = 0; i < nfn; i++) {
    if (!l_self[i] || is_idle[i]) continue;
    for (int k = 0; k < 3; k++)
      if (best[k] < 0 || l_self[i] > l_self[best[k]]) {
        for (int j = 2; j > k; j--) best[j] = best[j - 1];
        best[k] = i;
        break;
      }
  }
  out[0] = 0;
  for (int k = 0; k < 3 && best[k] >= 0; k++) {
    size_t len = strlen(out);
    snprintf(out + len, n - len, "%s%s %llu", k ? ";" : "", fn_name[best[k]], (unsigned long long)l_self[best[k]]);
  }
}

static void close_loop(uint64_t now) {
  if (loop_open && counting) {
    char t[256];
    top3(t, sizeof t);
    nloops++;
    if (loop_nmis >= 2) nloops_slow++;
    if (loop_busy > loops_busy_max) loops_busy_max = loop_busy;
    fr_loops++;
    if (loops_f)
      fprintf(loops_f, "%d,%d,%llu,%llu,%d,%s\n", nloops - 1, cur_frame, (unsigned long long)loop_busy,
              (unsigned long long)(now - loop_start), loop_nmis, t);
    if (nspike) {
      int lo = 0;
      for (int k = 1; k < nspike; k++) if (spikes[k].busy < spikes[lo].busy) lo = k;
      if (loop_busy > spikes[lo].busy) {
        struct spike *sp = &spikes[lo];
        sp->loop = nloops - 1; sp->frame = cur_frame; sp->nmis = loop_nmis; sp->busy = loop_busy; sp->n = 0;
        for (int a = 0; a < 0x10000; a++)
          if (l_line[a] && sp->n < 8192) { sp->addr[sp->n] = (uint16_t)a; sp->cyc[sp->n] = l_line[a]; sp->n++; }
      }
    }
    if (loopfns_f)
      for (int i = 0; i < nfn; i++)
        if (l_self[i]) fprintf(loopfns_f, "%d,%s,%llu\n", nloops - 1, fn_name[i], (unsigned long long)l_self[i]);
  }
  loop_open = 1; loop_busy = 0; loop_nmis = 0; loop_start = now;
  memset(l_self, 0, sizeof l_self);
  if (nspike) memset(l_line, 0, sizeof l_line);
}

static void hook(uint16_t pc, uint8_t op, uint8_t s) {
  uint64_t now = *cyc;
  if (have_last) {
    uint64_t d = now - last_cyc;
    /* at the NMI vector without a JSR or JMP that names it: an NMI entry */
    int jumped_to_nmi = (last_op == 0x20 || last_op == 0x4C) && rom_word(last_pc + 1) == nmi_addr;
    int nmi_entry = pc == nmi_addr && !jumped_to_nmi;
    if (nmi_entry && d >= 7) d -= 7;
    if (counting) {
      uint16_t at = last_pc;
      if (!is_c[fn_of(last_pc)])
        for (int i = depth - 1; i >= 0 && !stk[i].nmi; i--)
          if (is_c[fn_of(stk[i].site)]) { at = stk[i].site; break; }
      pc_c[last_pc] += d;
      line_c[at] += d;
      l_line[at] += d;
    }
    charge(fn_of(last_pc), d, nmi_depth > 0);
    if (last_op == 0x20) {                       /* JSR: now in the callee (or an NMI came first) */
      unsigned target = rom_word(last_pc + 1);
      if (target > 0xFFFF) target = pc;
      int f = fn_of((uint16_t)target);
      if (depth < MAXDEPTH) { stk[depth].f = f; stk[depth].nmi = 0; stk[depth].site = last_pc; depth++; }
      else overflows++;
      if (fn_start[f] == target && counting) { calls[f]++; f_calls[f]++; }
    } else if (last_op == 0x60 || last_op == 0x40) {   /* RTS, RTI */
      if (depth > 0) { depth--; if (stk[depth].nmi) nmi_depth--; }
    }
    if (nmi_entry) {
      int f = fn_of(pc);
      if (depth < MAXDEPTH) { stk[depth].f = f; stk[depth].nmi = 1; stk[depth].site = pc; depth++; nmi_depth++; }
      else overflows++;
      if (counting) { calls[f]++; f_calls[f]++; }
      if (counting) { pc_c[pc] += 7; line_c[pc] += 7; l_line[pc] += 7; }
      charge(f, 7, 1);
      loop_nmis++;
    }
    if (!nmi_depth) {
      int idle_now = in_idle(fn_of(pc));
      if (was_idle && !idle_now) close_loop(now);
      was_idle = idle_now;
    }
  }
  last_cyc = now; last_pc = pc; last_op = op; last_s = s; have_last = 1;
}

/* --- libretro frontend glue (from nessprites, 2026-11-13) ----------------- */
static bool env_cb(unsigned cmd, void *data) {
  switch (cmd) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
      *(const char **)data = "/tmp";
      return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
      enum retro_pixel_format f = *(enum retro_pixel_format *)data;
      return f == RETRO_PIXEL_FORMAT_RGB565 || f == RETRO_PIXEL_FORMAT_XRGB8888;
    }
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
      if (cmd == RETRO_ENVIRONMENT_GET_CAN_DUPE) *(bool *)data = true;
      return true;
    default:
      return false;
  }
}
static void video_cb(const void *d, unsigned w, unsigned h, size_t p) { (void)d; (void)w; (void)h; (void)p; }
static void audio_cb(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio_batch_cb(const int16_t *d, size_t f) { (void)d; return f; }
static struct { int from, to; int16_t mask; } inputs[MAX_PAD_RULES];
static int ninputs;
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

static int load_sym(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) { perror(path); return -1; }
  for (int a = 0; a < 0x10000; a++) fn_at[a] = -1;
  unsigned a0, a1; char name[256];
  while (fscanf(f, "%x %x %255s", &a0, &a1, name) == 3) {
    int i = fn_lookup(name);
    if (i < 0) {
      if (nfn >= MAXF - 1) { fprintf(stderr, "too many functions\n"); return -1; }
      i = nfn++; fn_name[i] = strdup(name); fn_start[i] = a0;
    }
    for (unsigned a = a0; a < a1 && a < 0x10000; a++) fn_at[a] = i;
  }
  fclose(f);
  unmapped = nfn++;
  fn_name[unmapped] = strdup("[unmapped]");
  fn_start[unmapped] = 0x10000;
  return 0;
}

static int cmp_self(const void *a, const void *b) {
  uint64_t x = self_c[*(const int *)a], y = self_c[*(const int *)b];
  return x < y ? 1 : x > y ? -1 : 0;
}
static int cmp_incl(const void *a, const void *b) {
  uint64_t x = incl_c[*(const int *)a], y = incl_c[*(const int *)b];
  return x < y ? 1 : x > y ? -1 : 0;
}

#define LOAD(name) do { *(void **)&p_##name = dlsym(core, #name); \
  if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; } } while (0)

int main(int argc, char **argv) {
  if (argc < 4) {
    fprintf(stderr, "usage: %s core.so rom.nes frames --sym rom.sym [options, see the source header]\n", argv[0]);
    return 2;
  }
  const char *core_path = argv[1], *rom_path = argv[2];
  int frames = atoi(argv[3]), topn = 12;
  const char *sym_path = NULL, *csv_path = NULL, *funcs_path = NULL, *loops_path = NULL, *fnf_path = NULL, *loopfns_path = NULL, *oam_path = NULL, *pcs_path = NULL, *spikes_path = NULL;
  const char *idle_names[MAX_IDLE]; int nidle = 0;
  unsigned ram_addr[MAX_RAMB]; const char *ram_name[MAX_RAMB]; int nram = 0;
  for (int i = 4; i < argc; i++) {
    if (!strcmp(argv[i], "--sym") && i + 1 < argc) sym_path = argv[++i];
    else if (!strcmp(argv[i], "--idle") && i + 1 < argc && nidle < MAX_IDLE) idle_names[nidle++] = argv[++i];
    else if (!strcmp(argv[i], "--from") && i + 1 < argc) from_frame = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--csv") && i + 1 < argc) csv_path = argv[++i];
    else if (!strcmp(argv[i], "--funcs") && i + 1 < argc) funcs_path = argv[++i];
    else if (!strcmp(argv[i], "--loops") && i + 1 < argc) loops_path = argv[++i];
    else if (!strcmp(argv[i], "--loopfns") && i + 1 < argc) loopfns_path = argv[++i];
    else if (!strcmp(argv[i], "--fnframes") && i + 1 < argc) fnf_path = argv[++i];
    else if (!strcmp(argv[i], "--pcs") && i + 1 < argc) pcs_path = argv[++i];
    else if (!strcmp(argv[i], "--spikes") && i + 2 < argc) {
      nspike = atoi(argv[++i]); spikes_path = argv[++i];
      if (nspike < 1 || nspike > MAXSPIKE) { fprintf(stderr, "--spikes N: 1 to %d\n", MAXSPIKE); return 2; }
    } else if (!strcmp(argv[i], "--oam") && i + 1 < argc) oam_path = argv[++i];
    else if (!strcmp(argv[i], "--top") && i + 1 < argc) topn = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--input") && i + 1 < argc && ninputs < MAX_PAD_RULES) {
      char b[16] = "";
      if (sscanf(argv[++i], "%d-%d:%15s", &inputs[ninputs].from, &inputs[ninputs].to, b) == 3)
        inputs[ninputs++].mask = parse_buttons(b);
    } else if (!strcmp(argv[i], "--ram") && i + 1 < argc && nram < MAX_RAMB) {
      char *c = strchr(argv[++i], ':');
      if (!c) { fprintf(stderr, "--ram ADDR:name\n"); return 2; }
      *c = 0; ram_addr[nram] = (unsigned)strtoul(argv[i], NULL, 16) & 0x7FF; ram_name[nram++] = c + 1;
    } else { fprintf(stderr, "unknown option %s\n", argv[i]); return 2; }
  }
  if (!sym_path) { fprintf(stderr, "--sym is required\n"); return 2; }
  if (load_sym(sym_path)) return 1;
  for (int i = 0; i < nfn; i++) is_c[i] = fn_name[i][0] == '_';
  for (int k = 0; k < nspike; k++) {
    spikes[k].addr = malloc(8192 * sizeof(uint16_t)); spikes[k].cyc = malloc(8192 * sizeof(uint32_t));
    spikes[k].loop = -1;
  }
  if (!nidle) idle_names[nidle++] = "_wait_frame";
  for (int k = 0; k < nidle; k++) {
    int f = fn_lookup(idle_names[k]);
    if (f < 0) { fprintf(stderr, "idle function %s not in %s\n", idle_names[k], sym_path); return 1; }
    is_idle[f] = 1;
  }

  FILE *rf = fopen(rom_path, "rb");
  if (!rf) { perror(rom_path); return 1; }
  fseek(rf, 0, SEEK_END); long rom_size = ftell(rf); fseek(rf, 0, SEEK_SET);
  uint8_t *rom = malloc(rom_size);
  if (fread(rom, 1, rom_size, rf) != (size_t)rom_size) { fprintf(stderr, "short read\n"); return 1; }
  fclose(rf);
  long prg_end = 16 + (long)rom[4] * 16384;
  if (rom_size < prg_end || rom[4] == 0) { fprintf(stderr, "not an iNES ROM\n"); return 1; }
  nmi_addr = rom[prg_end - 6] | rom[prg_end - 5] << 8;
  prg = rom + 16; prg_size = (long)rom[4] * 16384;

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
  cyc = dlsym(core, "nesprof_cycles");
  void (**hookp)(uint16_t, uint8_t, uint8_t) = dlsym(core, "nesprof_hook");
  if (!cyc || !hookp) { fprintf(stderr, "core was not built with fceumm-nesprof.patch\n"); return 1; }

  p_retro_set_environment(env_cb);
  p_retro_init();
  p_retro_set_video_refresh(video_cb);
  p_retro_set_audio_sample(audio_cb);
  p_retro_set_audio_sample_batch(audio_batch_cb);
  p_retro_set_input_poll(input_poll_cb);
  p_retro_set_input_state(input_state_cb);
  struct retro_game_info gi = { rom_path, rom, (size_t)rom_size, NULL };
  if (!p_retro_load_game(&gi)) { fprintf(stderr, "load_game failed\n"); return 1; }
  p_retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
  const uint8_t *ram = p_retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
  *hookp = hook;
  const uint8_t *spram = dlsym(core, "SPRAM");
  FILE *oamf = oam_path ? fopen(oam_path, "w") : NULL;
  if (oamf && !spram) { fprintf(stderr, "core does not export SPRAM\n"); return 1; }

  FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
  FILE *fnf = fnf_path ? fopen(fnf_path, "w") : NULL;
  loops_f = loops_path ? fopen(loops_path, "w") : NULL;
  if (csv) {
    fprintf(csv, "frame,cycles,busy,idle,nmi,loops_ended,hot_fn,hot_self");
    for (int r = 0; r < nram; r++) fprintf(csv, ",%s", ram_name[r]);
    fprintf(csv, "\n");
  }
  if (fnf) fprintf(fnf, "frame,function,self,inclusive,calls\n");
  loopfns_f = loopfns_path ? fopen(loopfns_path, "w") : NULL;
  if (loopfns_f) fprintf(loopfns_f, "loop,function,self\n");
  if (loops_f) fprintf(loops_f, "loop,end_frame,busy,cycles,nmis,hottest\n");

  uint64_t tot_cycles = 0, tot_idle = 0, tot_nmi = 0, busy_max = 0;
  int busy_max_frame = -1, measured = 0;
  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  for (int fno = 0; fno < frames; fno++) {
    cur_frame = fno;
    counting = fno >= from_frame;
    uint64_t start = *cyc;
    fr_idle = fr_nmi = 0; fr_loops = 0;
    memset(f_self, 0, sizeof f_self); memset(f_incl, 0, sizeof f_incl); memset(f_calls, 0, sizeof f_calls);
    p_retro_run();
    if (oamf) {
      fprintf(oamf, "%d,", fno);
      for (int b = 0; b < 256; b++) fprintf(oamf, "%02x", spram[b]);
      fprintf(oamf, "\n");
    }
    if (!counting) continue;
    uint64_t c = *cyc - start, busy = c - fr_idle;
    measured++;
    tot_cycles += c; tot_idle += fr_idle; tot_nmi += fr_nmi;
    if (busy > busy_max) { busy_max = busy; busy_max_frame = fno; }
    int hot = -1;
    for (int i = 0; i < nfn; i++) if (!is_idle[i] && f_self[i] && (hot < 0 || f_self[i] > f_self[hot])) hot = i;
    if (csv) {
      fprintf(csv, "%d,%llu,%llu,%llu,%llu,%d,%s,%llu", fno, (unsigned long long)c, (unsigned long long)busy,
              (unsigned long long)fr_idle, (unsigned long long)fr_nmi, fr_loops, hot >= 0 ? fn_name[hot] : "",
              hot >= 0 ? (unsigned long long)f_self[hot] : 0ULL);
      for (int r = 0; r < nram; r++) fprintf(csv, ",%u", ram[ram_addr[r]]);
      fprintf(csv, "\n");
    }
    if (fnf)
      for (int i = 0; i < nfn; i++)
        if (f_self[i] || f_incl[i])
          fprintf(fnf, "%d,%s,%llu,%llu,%llu\n", fno, fn_name[i], (unsigned long long)f_self[i],
                  (unsigned long long)f_incl[i], (unsigned long long)f_calls[i]);
  }
  clock_gettime(CLOCK_MONOTONIC, &t1);
  double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
  if (csv) fclose(csv);
  if (fnf) fclose(fnf);
  if (oamf) fclose(oamf);
  if (loops_f) fclose(loops_f);
  if (loopfns_f) fclose(loopfns_f);

  if (pcs_path) {
    FILE *pf = fopen(pcs_path, "w");
    fprintf(pf, "addr,function,cycles,with_helpers\n");
    for (int a = 0; a < 0x10000; a++)
      if (pc_c[a] || line_c[a])
        fprintf(pf, "%04x,%s,%llu,%llu\n", a, fn_name[fn_of((uint16_t)a)], (unsigned long long)pc_c[a],
                (unsigned long long)line_c[a]);
    fclose(pf);
  }
  if (spikes_path) {
    FILE *sf = fopen(spikes_path, "w");
    fprintf(sf, "loop,end_frame,busy,nmis,addr,function,cycles\n");
    for (int k = 0; k < nspike; k++)
      for (int j = 0; j < spikes[k].n; j++)
        fprintf(sf, "%d,%d,%llu,%d,%04x,%s,%u\n", spikes[k].loop, spikes[k].frame, (unsigned long long)spikes[k].busy,
                spikes[k].nmis, spikes[k].addr[j], fn_name[fn_of(spikes[k].addr[j])], spikes[k].cyc[j]);
    fclose(sf);
  }
  uint64_t busy_all = tot_cycles - tot_idle, sum_self = 0;
  for (int i = 0; i < nfn; i++) sum_self += self_c[i];
  int order[MAXF];
  for (int i = 0; i < nfn; i++) order[i] = i;
  if (funcs_path) {
    FILE *ff = fopen(funcs_path, "w");
    qsort(order, nfn, sizeof order[0], cmp_self);
    fprintf(ff, "function,self,inclusive,calls,self_per_frame,inclusive_per_frame,self_share_of_busy\n");
    for (int k = 0; k < nfn; k++) {
      int i = order[k];
      if (!self_c[i] && !incl_c[i]) continue;
      fprintf(ff, "%s,%llu,%llu,%llu,%.1f,%.1f,%.4f\n", fn_name[i], (unsigned long long)self_c[i],
              (unsigned long long)incl_c[i], (unsigned long long)calls[i], (double)self_c[i] / measured,
              (double)incl_c[i] / measured, is_idle[i] ? 0.0 : (double)self_c[i] / busy_all);
    }
    fclose(ff);
  }

  printf("frames %d (measured %d from frame %d), wall %.2f s (%.0f frames/s)\n", frames, measured, from_frame, secs,
         frames / secs);
  printf("cycles per frame %.1f; busy %.1f (%.1f%%), idle %.1f, NMI handler %.1f; busiest frame %d with %llu\n",
         (double)tot_cycles / measured, (double)busy_all / measured, 100.0 * busy_all / tot_cycles,
         (double)tot_idle / measured, (double)tot_nmi / measured, busy_max_frame, (unsigned long long)busy_max);
  printf("game loops %d; loops that saw 2+ NMIs (missed a frame) %d; most busy cycles in one loop %llu\n", nloops,
         nloops_slow, (unsigned long long)loops_busy_max);
  printf("self cycles add up to %llu of %llu counted%s\n", (unsigned long long)sum_self,
         (unsigned long long)tot_cycles, overflows ? " (call stack overflowed; inclusive figures are partial)" : "");
  qsort(order, nfn, sizeof order[0], cmp_self);
  printf("\n%-16s %12s %12s %10s %8s\n", "self", "cycles/frame", "incl/frame", "calls/frm", "busy%");
  for (int k = 0, shown = 0; k < nfn && shown < topn; k++) {
    int i = order[k];
    if (!self_c[i]) break;
    printf("%-16s %12.1f %12.1f %10.2f %7.1f%%%s\n", fn_name[i], (double)self_c[i] / measured,
           (double)incl_c[i] / measured, (double)calls[i] / measured,
           is_idle[i] ? 0.0 : 100.0 * self_c[i] / busy_all, is_idle[i] ? " (idle)" : "");
    shown++;
  }
  qsort(order, nfn, sizeof order[0], cmp_incl);
  printf("\n%-16s %12s %8s\n", "inclusive", "cycles/frame", "busy%");
  for (int k = 0, shown = 0; k < nfn && shown < topn; k++) {
    int i = order[k];
    if (!incl_c[i]) break;
    if (is_idle[i]) continue;
    printf("%-16s %12.1f %7.1f%%\n", fn_name[i], (double)incl_c[i] / measured, 100.0 * incl_c[i] / busy_all);
    shown++;
  }
  return 0;
}
