// Test ROM for mdprof. Every workload here has a known cost, so the profiler's
// numbers can be checked against something. No input needed: the phase changes
// with the frame number. All code and art written for this project (MIT).
//
// Phase 0 A  (game frames    0-239): 24 moving sprites, light load
// Phase 1 B  (game frames  240-479): the same sprites, plus 32-bit divisions per
//                                    sprite that grow every 30 frames (1 to 8)
//                                    until the frame no longer fits (slowdown)
// Phase 2 B2 (game frames  480-599): the same 8 per sprite, done with a
//                                    reciprocal table and a 16-bit multiply
// Phase 3 C  (game frames  600-719): every 20th frame redraws the 40x28 plane
//                                    with one VDP_setTileMapXY() call per tile
// Phase 4 C2 (game frames  720-839): the same redraw as one VDP_fillTileMapRect()
// Phase 5 D  (game frames 840-1319): a DMA queue that grows by 512 bytes every
//                                    40 frames, from 4096 to 9728 bytes
// Every frame also runs burn(500), a dbra loop of 10*500+14 cycles, to check
// the cycle accounting.
#include <genesis.h>

#define NSPR 24

// read from the profiler through symbol.txt
volatile u16 game_frames;
volatile u16 sgdk_load;
volatile u16 phase;
volatile u16 div_per_sprite;
volatile u16 dma_bytes_queued;

static s16 sx[NSPR], sy[NSPR], vx[NSPR], vy[NSPR];
static volatile s32 sink;

// one 8x8 tile, a round blob in color 1 with a color 2 rim
static const u32 blob[8] = {
    0x00222200, 0x02111120, 0x21111112, 0x21111112,
    0x21111112, 0x21111112, 0x02111120, 0x00222200,
};
// a checker tile for the background
static const u32 checker[8] = {
    0x33334444, 0x33334444, 0x33334444, 0x33334444,
    0x44443333, 0x44443333, 0x44443333, 0x44443333,
};
// 10 KB of tile data in ROM, uploaded by the DMA phase into spare VRAM
static const u32 dma_src[2560] = { [0 ... 2559] = 0x12345678 };

NO_INLINE void burn(u16 n)
{
    // dbra: 10 cycles per taken branch, 14 when the counter expires
    __asm__ volatile ("1: dbra %0, 1b" : "+d" (n));
}

NO_INLINE void move_sprites(void)
{
    for (u16 i = 0; i < NSPR; i++)
    {
        sx[i] += vx[i];
        sy[i] += vy[i];
        if (sx[i] < 0 || sx[i] > 312) vx[i] = -vx[i];
        if (sy[i] < 0 || sy[i] > 216) vy[i] = -vy[i];
        VDP_setSpriteFull(i, sx[i], sy[i], SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, 1, FALSE, FALSE, TILE_USER_INDEX),
                          (i + 1 < NSPR) ? i + 1 : 0);
    }
    VDP_updateSprites(NSPR, DMA_QUEUE);
}

NO_INLINE void physics_with_divisions(u16 per_sprite)
{
    // a 32-bit division in the per-sprite loop: cheap on a modern CPU, a
    // libgcc call (__divsi3) on the 68000
    s32 acc = 0;
    for (u16 i = 0; i < NSPR; i++)
        for (u16 k = 0; k < per_sprite; k++)
            acc += ((s32) sx[i] * 1000 + k) / ((s32) sy[i] + 7 + k);
    sink = acc;
}

// 1000 * 256 / d for d = 1..511, so x * 1000 / d ~= (x * recip1000[d]) >> 8
static u16 recip1000[512];

NO_INLINE void physics_with_reciprocals(u16 per_sprite)
{
    // the usual fix: no division in the loop, one 16x16 multiply (mulu.w)
    s32 acc = 0;
    for (u16 i = 0; i < NSPR; i++)
        for (u16 k = 0; k < per_sprite; k++)
            acc += ((u32) (u16) sx[i] * recip1000[(u16) (sy[i] + 7 + k)]) >> 8;
    sink = acc;
}

NO_INLINE void redraw_plane_fill(void)
{
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_USER_INDEX + 1), 0, 0, 40, 28);
}

NO_INLINE void redraw_plane_per_tile(void)
{
    for (u16 y = 0; y < 28; y++)
        for (u16 x = 0; x < 40; x++)
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_USER_INDEX + 1), x, y);
}

NO_INLINE void queue_dma(u16 bytes)
{
    // VRAM 0x8000 is unused by this ROM; len is in words
    DMA_queueDma(DMA_VRAM, (void*) dma_src, 0x8000, bytes / 2, 2);
}

int main(bool hardReset)
{
    (void) hardReset;
    VDP_setScreenWidth320();
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xffcc33));
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0x995500));
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0x223344));
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0x2a3d50));
    VDP_loadTileData(blob, TILE_USER_INDEX, 1, CPU);
    VDP_loadTileData(checker, TILE_USER_INDEX + 1, 1, CPU);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, 0, FALSE, FALSE, TILE_USER_INDEX + 1), 0, 0, 40, 28);
    // no per-frame DMA cap, so a big queue spills out of VBlank instead of
    // being dropped (the default cap is 7200 bytes on NTSC)
    DMA_setMaxTransferSize(0);

    for (u16 d = 1; d < 512; d++) recip1000[d] = (u16) ((1000UL * 256) / d);
    for (u16 i = 0; i < NSPR; i++)
    {
        sx[i] = 10 + i * 12;
        sy[i] = 20 + (i * 37) % 180;
        vx[i] = (i & 1) ? 1 : -2;
        vy[i] = (i & 2) ? 2 : -1;
    }
    VDP_drawText("mdprof test ROM", 1, 1);

    while (TRUE)
    {
        u16 f = game_frames;
        burn(500);
        move_sprites();

        if (f < 240) phase = 0;
        else if (f < 480)
        {
            phase = 1;
            div_per_sprite = 1 + (f - 240) / 30;
            physics_with_divisions(div_per_sprite);
        }
        else if (f < 600)
        {
            phase = 2;
            div_per_sprite = 8;
            physics_with_reciprocals(div_per_sprite);
        }
        else if (f < 720)
        {
            phase = 3;
            if ((f % 20) == 0) redraw_plane_per_tile();
        }
        else if (f < 840)
        {
            phase = 4;
            if ((f % 20) == 0) redraw_plane_fill();
        }
        else if (f < 1320)
        {
            phase = 5;
            dma_bytes_queued = 4096 + ((f - 840) / 40) * 512;
            queue_dma(dma_bytes_queued);
        }
        else phase = 6;

        sgdk_load = SYS_getCPULoad();
        game_frames = f + 1;
        SYS_doVBlankProcess();
    }
    return 0;
}
