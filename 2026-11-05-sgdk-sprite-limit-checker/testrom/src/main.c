/*
 * Test ROM for mdsprites. Each phase puts a known sprite layout on one row for
 * PHASE_FRAMES frames, so the expected result can be worked out by hand.
 * MIT license. All art is drawn by tools/make_art.py.
 */
#include <genesis.h>
#include "sprites.h"

#define PHASE_FRAMES 120
#define ROW 96

/* read by mdsprites through --ram */
volatile u16 phase;

static Sprite *spr[40];
static u16 nspr;

static void clear_all(void) {
    SPR_reset();
    nspr = 0;
    SPR_update();
    SYS_doVBlankProcess();
}

static void add(const SpriteDefinition *def, s16 x, s16 y, u16 flags) {
    spr[nspr++] = SPR_addSpriteEx(def, x, y, TILE_ATTR(PAL1, TRUE, FALSE, FALSE), flags);
}

#define F_PLAIN (SPR_FLAG_AUTO_VRAM_ALLOC | SPR_FLAG_AUTO_TILE_UPLOAD)
#define F_AUTOVIS (F_PLAIN | SPR_FLAG_AUTO_VISIBILITY)
#define F_FASTVIS (F_AUTOVIS | SPR_FLAG_FAST_AUTO_VISIBILITY)

/* n sprites side by side, overlapping if needed so every one is on screen */
static void row(const SpriteDefinition *def, u16 n, u16 size, u16 flags) {
    u16 step = (n * size > 320) ? (320 - size) / (n - 1) : size;
    for (u16 i = 0; i < n; i++) add(def, i * step, ROW, flags);
}

static void hold(void) {
    for (u16 f = 0; f < PHASE_FRAMES; f++) {
        SPR_update();
        SYS_doVBlankProcess();
    }
}

/* the same sprites with their depth rotated each frame, so a different one is last */
static void hold_rotating(u16 n) {
    for (u16 f = 0; f < PHASE_FRAMES; f++) {
        for (u16 i = 0; i < n; i++) SPR_setDepth(spr[i], (s16)((i + f) % n));
        SPR_update();
        SYS_doVBlankProcess();
    }
}

/* no sprite engine: fill the table by hand, rotating which sprite comes first */
static const u32 raw_tile[4 * 8] = {
    0x11111111, 0x12222222, 0x12222222, 0x12222222, 0x12222222, 0x12222222, 0x12222222, 0x12222222,
    0x12222222, 0x12222222, 0x12222222, 0x12222222, 0x12222222, 0x12222222, 0x12222222, 0x11111111,
    0x11111111, 0x22222221, 0x22222221, 0x22222221, 0x22222221, 0x22222221, 0x22222221, 0x22222221,
    0x22222221, 0x22222221, 0x22222221, 0x22222221, 0x22222221, 0x22222221, 0x22222221, 0x11111111,
};

static void raw_rows(u16 n, u16 rotate, u16 tile) {
    u16 step = (320 - 16) / (n - 1);
    for (u16 f = 0; f < PHASE_FRAMES; f++) {
        u16 start = rotate ? f % n : 0;
        for (u16 i = 0; i < n; i++) {
            u16 which = (i + start) % n;
            VDP_setSpriteFull(i, which * step, ROW, SPRITE_SIZE(2, 2),
                              TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, tile), (i + 1 < n) ? i + 1 : 0);
        }
        VDP_updateSprites(n, DMA_QUEUE);
        SYS_doVBlankProcess();
    }
    VDP_clearSprites();
    VDP_updateSprites(1, DMA_QUEUE);
    SYS_doVBlankProcess();
}

int main(bool hard) {
    (void)hard;
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x101828));
    PAL_setPalette(PAL1, spr_box16.palette->data, DMA);
    SPR_init();

    /* 1, 2: the count limit with 8x8 sprites */
    phase = 1; row(&spr_box8, 20, 8, F_PLAIN); hold(); clear_all();
    phase = 2; row(&spr_box8, 21, 8, F_PLAIN); hold(); clear_all();
    /* 3, 4: 16 wide, where count and dots run out together */
    phase = 3; row(&spr_box16, 20, 16, F_PLAIN); hold(); clear_all();
    phase = 4; row(&spr_box16, 21, 16, F_PLAIN); hold(); clear_all();
    /* 5, 6: 32 wide, the dot limit arrives at 10 sprites */
    phase = 5; row(&spr_box32, 10, 32, F_PLAIN); hold(); clear_all();
    phase = 6; row(&spr_box32, 11, 32, F_PLAIN); hold(); clear_all();
    /* 7: 8 on screen, 4 off screen to the left and first in the list,
       added with plain SPR_addSprite flags (visibility on) */
    phase = 7;
    for (u16 i = 0; i < 4; i++) add(&spr_box32, -64, ROW, F_PLAIN);
    for (u16 i = 0; i < 8; i++) add(&spr_box32, 8 + i * 38, ROW, F_PLAIN);
    hold(); clear_all();
    /* 8: the same layout with SPR_FLAG_AUTO_VISIBILITY */
    phase = 8;
    for (u16 i = 0; i < 4; i++) add(&spr_box32, -64, ROW, F_AUTOVIS);
    for (u16 i = 0; i < 8; i++) add(&spr_box32, 8 + i * 38, ROW, F_AUTOVIS);
    hold(); clear_all();
    /* 9: the same layout, off-screen sprites last in the list */
    phase = 9;
    for (u16 i = 0; i < 8; i++) add(&spr_box32, 8 + i * 38, ROW, F_PLAIN);
    for (u16 i = 0; i < 4; i++) add(&spr_box32, -64, ROW, F_PLAIN);
    hold(); clear_all();
    /* 10, 11, 12: seven 48x48 sprites by rescomp optimization type */
    phase = 10; row(&spr_disc_bal, 7, 48, F_PLAIN); hold(); clear_all();
    phase = 11; row(&spr_disc_spr, 7, 48, F_PLAIN); hold(); clear_all();
    phase = 12; row(&spr_disc_tile, 7, 48, F_PLAIN); hold(); clear_all();
    /* 13, 14: big sprites half off screen, per-sprite vs fast visibility */
    phase = 13;
    for (u16 i = 0; i < 3; i++) add(&spr_disc_bal, -24, ROW, F_AUTOVIS);
    for (u16 i = 0; i < 6; i++) add(&spr_box32, 40 + i * 40, ROW, F_AUTOVIS);
    hold(); clear_all();
    phase = 14;
    for (u16 i = 0; i < 3; i++) add(&spr_disc_bal, -24, ROW, F_FASTVIS);
    for (u16 i = 0; i < 6; i++) add(&spr_box32, 40 + i * 40, ROW, F_FASTVIS);
    hold(); clear_all();
    /* 15, 16: 24 sprites on one line: fixed order vs depth rotated by the engine */
    phase = 15; row(&spr_box16, 24, 16, F_PLAIN); hold(); clear_all();
    phase = 16; row(&spr_box16, 24, 16, F_PLAIN); hold_rotating(24); clear_all();
    /* 17, 18: the same with a hand-written table, fixed vs rotated */
    u16 tile = TILE_USER_INDEX;
    VDP_loadTileData(raw_tile, tile, 4, DMA);
    phase = 17; raw_rows(24, FALSE, tile);
    phase = 18; raw_rows(24, TRUE, tile);
    /* 19: 24 sprites in two rows 16 pixels apart: each line has 12 */
    phase = 19;
    for (u16 i = 0; i < 12; i++) add(&spr_box16, i * 26, ROW, F_PLAIN);
    for (u16 i = 0; i < 12; i++) add(&spr_box16, i * 26, ROW + 16, F_PLAIN);
    hold(); clear_all();

    /* 20 to 28: one sprite partly over each edge, three visibility modes */
    static const s16 ex[3] = {-8, 300, 100};
    static const s16 ey[3] = {ROW, ROW, -8};
    static const u16 ef[3] = {F_PLAIN, F_AUTOVIS, F_FASTVIS};
    phase = 20;
    for (u16 e = 0; e < 3; e++)
        for (u16 m = 0; m < 3; m++) {
            add(&spr_box32, ex[e], ey[e], ef[m]);
            hold();
            clear_all();
            phase++;
        }

    phase = 99;
    while (TRUE) SYS_doVBlankProcess();
    return 0;
}
