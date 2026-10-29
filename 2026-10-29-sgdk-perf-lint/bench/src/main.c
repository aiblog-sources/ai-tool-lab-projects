// Cost table ROM for sgdklint. Each phase runs one case once per frame for
// PHASE_LEN frames; mdprof records the busy cycles of every frame, and
// tools/costs.py turns (case - baseline) / repetitions into cycles per call.
// The phase number is read from RAM through symbol.txt. All code and art
// written for this project (MIT).
#include <genesis.h>
#include "resources.h"

#define PHASE_LEN 64
#define REPS 40          // repetitions per frame for the arithmetic cases

volatile u16 game_frames;
volatile u16 phase;

// operands live in volatile globals so GCC cannot fold the work away; the
// values are typical game magnitudes (positions, speeds, scores)
volatile s32 a32 = 123456, b32 = 321;
volatile u32 ua32 = 123456, ub32 = 321;
volatile s16 as16 = 12345, bs16 = 37;
volatile u16 au16 = 50000, bu16 = 37;
volatile float fa = 123.5f, fb = 3.25f;
volatile fix16 xa16 = FIX16(12.5), xb16 = FIX16(3.25);
volatile fix32 xa32 = FIX32(1234.5), xb32 = FIX32(3.25);
static volatile s32 sink;

static u16 rowbuf[40];
static u32 tilebuf[64 * 8];
static char textbuf[32];

#define ARITH(name, expr) \
    NO_INLINE void name(void) { s32 acc = 0; for (u16 i = 0; i < REPS; i++) acc += (expr); sink = acc; }

// baselines: the same loop with one or two volatile reads and an add
ARITH(base_s32_1, a32)
ARITH(base_s32_2, a32 + b32)
ARITH(base_u32_2, ua32 + ub32)
ARITH(base_s16_2, as16 + bs16)
ARITH(base_u16_2, au16 + bu16)
ARITH(base_f_2, (s32) fa)   // one float read + conversion, see costs.py

ARITH(div_s32_var, a32 / b32)
ARITH(div_u32_var, ua32 / ub32)
ARITH(div_s16_var, as16 / bs16)
ARITH(div_u16_var, au16 / bu16)
ARITH(mod_s32_var, a32 % b32)
ARITH(mod_u16_var, au16 % bu16)
ARITH(div_s32_c10, a32 / 10)
ARITH(div_u16_c10, au16 / 10)
ARITH(div_s32_c8, a32 / 8)
ARITH(div_u16_c8, au16 / 8)
ARITH(sgdk_divu, divu(ua32, bu16))
ARITH(sgdk_divs, divs(a32, bs16))
ARITH(mul_s32_var, a32 * b32)
ARITH(mul_s16_var, as16 * bs16)
ARITH(mul_u16_var, au16 * bu16)
ARITH(mul_s32_c10, a32 * 10)
ARITH(sgdk_muls, muls(as16, bs16))
ARITH(float_mul, (s32) (fa * fb))
ARITH(float_div, (s32) (fa / fb))
ARITH(float_add, (s32) (fa + fb))
ARITH(f16_mul, F16_mul(xa16, xb16))
ARITH(f16_div, F16_div(xa16, xb16))
ARITH(f32_mul, F32_mul(xa32, xb32))
// a u16 inside an expression is promoted to int: (u16 - 240) / 30 is signed
ARITH(div_u16expr_c30, (au16 - 240) / 30)
ARITH(mod_u16_c20, au16 % 20)
// mixed widths, as AI-written code had them
ARITH(mul_s16_s32, as16 * b32)
ARITH(mod_u16_fix16c, au16 % FIX16(2.7))
// SGDK 2.09 muls() is plain C: when GCC can prove one operand is non-negative
// it may stop using muls.w (as in an AI-written particle spawner)
ARITH(muls_known_positive, muls(as16, (s16) (256 + (au16 % 768))))
ARITH(muls_mod_only, as16 + (s16) (256 + (au16 % 768)))

// API cases: one call (or one row of 40 tiles) per frame
NO_INLINE void tile_xy_40(void)
{
    for (u16 x = 0; x < 40; x++)
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_USER_INDEX + (x & 3)), x, 20);
}
NO_INLINE void tile_fill_40(void)
{
    VDP_fillTileMapRect(BG_A, TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_USER_INDEX + 1), 0, 20, 40, 1);
}
NO_INLINE void tile_datarect_40_cpu(void)
{
    VDP_setTileMapDataRect(BG_A, rowbuf, 0, 20, 40, 1, 40, CPU);
}
NO_INLINE void tile_datarect_40_dma(void)
{
    VDP_setTileMapDataRect(BG_A, rowbuf, 0, 20, 40, 1, 40, DMA_QUEUE);
}
NO_INLINE void text_20(void)
{
    VDP_drawText("SCORE 0012345 LIVES", 1, 1);
}
NO_INLINE void sprintf_score(void)
{
    sprintf(textbuf, "SCORE %ld", (long) a32);
}
NO_INLINE void inttostr_score(void)
{
    intToStr(a32, textbuf, 7);
}
NO_INLINE void sprintf_and_text(void)
{
    sprintf(textbuf, "SCORE %ld", (long) a32);
    VDP_drawText(textbuf, 1, 2);
}
NO_INLINE void spr_add_release(void)
{
    Sprite *s = SPR_addSprite(&spr_ball, 100, 100, TILE_ATTR(PAL1, 0, 0, 0));
    SPR_releaseSprite(s);
}
NO_INLINE void load_tiles_64_dma(void)
{
    VDP_loadTileData(tilebuf, 256, 64, DMA_QUEUE);
}
NO_INLINE void load_tiles_64_cpu(void)
{
    VDP_loadTileData(tilebuf, 256, 64, CPU);
}
NO_INLINE void draw_image_none(void)
{
    VDP_drawImageEx(BG_B, &img_panel_none, TILE_ATTR_FULL(PAL1, 0, 0, 0, 512), 0, 0, FALSE, TRUE);
}
NO_INLINE void draw_image_fast(void)
{
    VDP_drawImageEx(BG_B, &img_panel_fast, TILE_ATTR_FULL(PAL1, 0, 0, 0, 512), 0, 0, FALSE, TRUE);
}
NO_INLINE void draw_image_aplib(void)
{
    VDP_drawImageEx(BG_B, &img_panel_aplib, TILE_ATTR_FULL(PAL1, 0, 0, 0, 512), 0, 0, FALSE, TRUE);
}
NO_INLINE void nothing(void) {}

typedef void (*case_fn)(void);
// the order here is the phase order; tools/costs.py has the same list
static const case_fn cases[] = {
    nothing,
    base_s32_1, base_s32_2, base_u32_2, base_s16_2, base_u16_2, base_f_2,
    div_s32_var, div_u32_var, div_s16_var, div_u16_var, mod_s32_var, mod_u16_var,
    div_s32_c10, div_u16_c10, div_s32_c8, div_u16_c8, sgdk_divu, sgdk_divs,
    mul_s32_var, mul_s16_var, mul_u16_var, mul_s32_c10, sgdk_muls,
    float_mul, float_div, float_add, f16_mul, f16_div, f32_mul,
    tile_xy_40, tile_fill_40, tile_datarect_40_cpu, tile_datarect_40_dma,
    text_20, sprintf_score, inttostr_score, sprintf_and_text,
    spr_add_release, load_tiles_64_dma, load_tiles_64_cpu,
    draw_image_none, draw_image_fast, draw_image_aplib,
    div_u16expr_c30, mod_u16_c20, mul_s16_s32, mod_u16_fix16c, muls_known_positive, muls_mod_only,
    nothing,
};
#define NCASES (sizeof(cases) / sizeof(cases[0]))

int main(bool hard)
{
    (void) hard;
    for (u16 i = 0; i < 40; i++) rowbuf[i] = TILE_ATTR_FULL(PAL0, 0, 0, 0, TILE_USER_INDEX + (i & 3));
    for (u16 i = 0; i < 64 * 8; i++) tilebuf[i] = 0x11223344 + i;
    PAL_setPalette(PAL1, spr_ball.palette->data, DMA);
    SPR_init();
    // one sprite stays on screen so SPR_update has work every frame
    SPR_addSprite(&spr_ball, 40, 40, TILE_ATTR(PAL1, 0, 0, 0));

    while (TRUE)
    {
        u16 p = game_frames / PHASE_LEN;
        if (p >= NCASES) p = NCASES - 1;
        phase = p;
        cases[p]();
        SPR_update();
        SYS_doVBlankProcess();
        game_frames++;
    }
    return 0;
}
