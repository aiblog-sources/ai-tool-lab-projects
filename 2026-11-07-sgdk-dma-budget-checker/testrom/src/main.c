/*
 * Test ROM for mddma. Each phase queues a known number of tile bytes per frame
 * through SGDK's DMA queue (in 1 KB entries, like a game uploading sprite
 * frames), then the sprite table, then burns a fixed amount of CPU, for
 * PHASE_FRAMES frames. The uploaded tiles alternate between a red set and a
 * blue set every frame, so a transfer that lands during the display shows as a
 * red/blue split on screen, and eight sprites move one pixel per frame, so a
 * sprite table that never reaches the VDP shows as sprites that stop.
 * MIT license.
 */
#include <genesis.h>

#define PHASE_FRAMES 90
#define CHUNK 1024              /* bytes per queue entry */
#define NTILES 512              /* 16 KB of tiles per set */
#define NSPR 8
/* 1: queue each phase's tiles as one entry instead of 1 KB entries (run.sh ONE_ENTRY=1) */
#define ONE_ENTRY 0
#define BURN 1000               /* about 45% of an NTSC frame (calibrated in the 0 KB phase) */

/* read by mddma through --ram */
volatile u16 phase;
volatile u16 spr_x;

static const u32 tiles_red[NTILES * 8] = {[0 ... NTILES * 8 - 1] = 0x11111111};
static const u32 tiles_blue[NTILES * 8] = {[0 ... NTILES * 8 - 1] = 0x22222222};
static const u32 spr_tile[8] = {0x33333333, 0x33333333, 0x33333333, 0x33333333,
                                0x33333333, 0x33333333, 0x33333333, 0x33333333};

enum { SAT_LAST = 0, SAT_FIRST = 1 };

typedef struct {
    u16 kbytes;        /* tile bytes queued per frame, in KB */
    u8 ignore;         /* DMA_setIgnoreOverCapacity */
    u8 h32;            /* 256-pixel mode */
    u8 sat_first;      /* queue the sprite table before the tiles */
    u8 split;          /* send half of the tiles on even frames, half on odd */
} phase_t;

/* phase numbers start at 1; tools/analyze.py has the same table */
static const phase_t phases[] = {
    {0, 0, 0, 0, 0},  {2, 0, 0, 0, 0},  {4, 0, 0, 0, 0},  {6, 0, 0, 0, 0},  {7, 0, 0, 0, 0},
    {8, 0, 0, 0, 0},  {10, 0, 0, 0, 0}, {12, 0, 0, 0, 0}, {16, 0, 0, 0, 0},
    {8, 1, 0, 0, 0},  {10, 1, 0, 0, 0}, {12, 1, 0, 0, 0}, {16, 1, 0, 0, 0},
    {12, 0, 0, 1, 0}, {12, 1, 0, 1, 0}, {12, 0, 0, 0, 1},
    {4, 0, 1, 0, 0},  {6, 0, 1, 0, 0},  {8, 0, 1, 0, 0},  {12, 0, 1, 0, 0},
};
#define NPHASES (sizeof(phases) / sizeof(phases[0]))

static void burn(u16 n) {
    volatile u16 i;
    for (i = 0; i < n; i++) {}
}

/* the plane shows only the tiles this phase uploads, so a frame that got all of
   its tiles in VBlank is a single color */
static void draw_plane(u16 h32, u16 ntiles) {
    u16 w = h32 ? 32 : 40;
    if (ntiles == 0) ntiles = 1;
    for (u16 y = 0; y < 28; y++)
        for (u16 x = 0; x < 40; x++)
            VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE,
                             TILE_USER_INDEX + (x < w ? (y * w + x) % ntiles : 0)), x, y);
}

static void queue_tiles(u16 kb, u16 frame, u16 split) {
    const u32 *src = (frame & (split ? 2 : 1)) ? tiles_blue : tiles_red;
    u16 first = 0, n = kb;
    if (split) { n = kb / 2; first = (frame & 1) ? n : 0; }
    if (ONE_ENTRY) {
        if (n) DMA_queueDma(DMA_VRAM, (void *)((u32)src + first * CHUNK), TILE_USER_INDEX * 32 + first * CHUNK,
                            n * (CHUNK / 2), 2);
        return;
    }
    for (u16 c = first; c < first + n; c++) {
        u16 off = c * CHUNK;
        DMA_queueDma(DMA_VRAM, (void *)((u32)src + off), TILE_USER_INDEX * 32 + off, CHUNK / 2, 2);
    }
}

static void queue_sprites(u16 x) {
    for (u16 i = 0; i < NSPR; i++)
        VDP_setSpriteFull(i, x + i * 24, 100 + (i & 1) * 16, SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_USER_INDEX + NTILES),
                          (i + 1 < NSPR) ? i + 1 : 0);
    VDP_updateSprites(NSPR, DMA_QUEUE);
}

int main(bool hard) {
    (void)hard;
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0xe02020));
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0x2040e0));
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xffffff));
    VDP_loadTileData(spr_tile, TILE_USER_INDEX + NTILES, 1, CPU);
    VDP_loadTileData(tiles_red, TILE_USER_INDEX, NTILES, CPU);
    draw_plane(FALSE, NTILES);
    phase = 0;
    for (u16 f = 0; f < 30; f++) SYS_doVBlankProcess();

    for (u16 p = 0; p < NPHASES; p++) {
        const phase_t *ph = &phases[p];
        if (ph->h32) VDP_setScreenWidth256(); else VDP_setScreenWidth320();
        draw_plane(ph->h32, ph->kbytes * (CHUNK / 32));
        DMA_setIgnoreOverCapacity(ph->ignore);
        phase = p + 1;
        for (u16 f = 0; f < PHASE_FRAMES; f++) {
            spr_x = 32 + f;
            if (ph->sat_first) queue_sprites(spr_x);
            queue_tiles(ph->kbytes, f, ph->split);
            if (!ph->sat_first) queue_sprites(spr_x);
            burn(BURN);
            SYS_doVBlankProcess();
        }
    }
    phase = 0xffff;
    while (TRUE) SYS_doVBlankProcess();
    return 0;
}
