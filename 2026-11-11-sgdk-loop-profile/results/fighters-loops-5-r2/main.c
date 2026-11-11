/*
 * main.c: SGDK 2.11, Sega Mega Drive / Genesis
 * Fighting game scene: two 96x112 fighters, 6-frame idle + 6-frame punch.
 *
 * - No resource files. All art is drawn at runtime by a small software
 *   renderer into 4bpp tile buffers in RAM. Background tiles are generated
 *   in code too.
 * - Each fighter is 12 hardware sprites (3 columns x 4 rows, each 32 px wide,
 *   rows 32/32/32/16 px high), so 24 sprites total, 6 per scanline at most.
 *
 * DMA budget (the reason for the structure below):
 *   One 96x112 frame = 168 tiles = 5376 bytes. Two fighters changing frame
 *   in the same vblank would need 10752 bytes, which is more than the
 *   ~7 KB an NTSC vblank can move. So:
 *     1. Every fighter has TWO VRAM slots (double buffer). The visible slot
 *        is never written.
 *     2. The next frame is chosen at the start of the 3-tick animation
 *        step. Its three 32px columns are rendered one per video frame into
 *        a RAM buffer, each streamed right away (1792 bytes) into the
 *        hidden slot.
 *     3. When the animation step ends, only the sprite table changes
 *        (tile base pointer swap), which is a 192-byte DMA.
 *     4. The two fighters are offset by one tick.
 *   Worst case per vblank: 2 x 1792 + 192 = 3776 bytes.
 *
 * CPU budget: rendering is sliced per column (one column per fighter per
 * video frame). The rasteriser fills contiguous runs with unrolled long
 * writes and avoids multiplications (shifts and lookup tables only), so a
 * game loop fits in one video frame with margin.
 *
 * Controls (pad 1): D-pad left/right = P1 walks, A = P1 punches.
 * Because the next frame is prepared one step ahead, a punch starts
 * 3 to 6 video frames after the press.
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

#define FW              96              /* fighter width  (px)        */
#define FH              112             /* fighter height (px)        */
#define FTILES          168             /* 12 x 14 tiles              */
#define COL_TILES       56              /* tiles per 32px column      */
#define COL_WORDS32     (COL_TILES * 8) /* u32 per column (448)       */
#define CHUNKS          3               /* one chunk per column       */
#define SPR_PER_FIGHTER 12
#define SPRITES_TOTAL   24
#define ANIM_TICKS      3               /* video frames per anim frame*/
#define ANIM_FRAMES     6

#define ANIM_IDLE       0
#define ANIM_PUNCH      1

#define FLOOR_Y         208             /* feet touch this line       */

/* column offsets without multiplication */
#define COL_OFF32(c)    ((u16)(((c) << 8) + ((c) << 7) + ((c) << 6)))  /* c*448 */
#define COL_OFF_TILES(c) ((u16)(((c) << 5) + ((c) << 4) + ((c) << 3))) /* c*56  */

/* Tile layout in VRAM (indices relative to TILE_USER_INDEX) */
#define BG_TILES        TILE_USER_INDEX
#define T_SKY1          (BG_TILES + 0)
#define T_SKY2          (BG_TILES + 1)
#define T_SKY3          (BG_TILES + 2)
#define T_BLD           (BG_TILES + 3)
#define T_WIN           (BG_TILES + 4)
#define T_FLA           (BG_TILES + 5)
#define T_FLB           (BG_TILES + 6)
#define BG_TILE_COUNT   7

#define P1_SLOT0        (BG_TILES + 8)
#define P1_SLOT1        (P1_SLOT0 + FTILES)
#define P2_SLOT0        (P1_SLOT1 + FTILES)
#define P2_SLOT1        (P2_SLOT0 + FTILES)

/* ------------------------------------------------------------------ */
/* Data                                                               */
/* ------------------------------------------------------------------ */

typedef struct
{
    s16   x, y;
    u16   pal;
    bool  flip;
    u16   vram[2];          /* first tile index of each VRAM slot      */
    u8    shown;            /* slot currently displayed                */
    u8    anim, frame;      /* currently displayed                     */
    u8    nextAnim, nextFrame;
    u8    timer;
    u8    chunk;            /* next chunk to render+DMA (CHUNKS = done)*/
    bool  punchReq;
    u32  *buf;              /* RAM render buffer, sprite-tile order    */
} Fighter;

static u32 bufP1[FTILES * 8];
static u32 bufP2[FTILES * 8];
static u32 bgTiles[BG_TILE_COUNT][8];

static Fighter p1, p2;

static const u32 leftMask[8] =
{
    0xFFFFFFFF, 0x0FFFFFFF, 0x00FFFFFF, 0x000FFFFF,
    0x0000FFFF, 0x00000FFF, 0x000000FF, 0x0000000F
};
static const u32 rightMask[8] =
{
    0xF0000000, 0xFF000000, 0xFFF00000, 0xFFFF0000,
    0xFFFFF000, 0xFFFFFF00, 0xFFFFFFF0, 0xFFFFFFFF
};

/* palette index replicated over 8 pixels (replaces col * 0x11111111) */
static const u32 fillVal[16] =
{
    0x00000000, 0x11111111, 0x22222222, 0x33333333,
    0x44444444, 0x55555555, 0x66666666, 0x77777777,
    0x88888888, 0x99999999, 0xAAAAAAAA, 0xBBBBBBBB,
    0xCCCCCCCC, 0xDDDDDDDD, 0xEEEEEEEE, 0xFFFFFFFF
};

/* Pose tables */
static const s16 idleBob[ANIM_FRAMES]   = { 0, 1, 2, 2, 1, 0 };
static const s16 punchExt[ANIM_FRAMES]  = { -1, 8, 24, 24, 12, -1 };
static const s16 punchLean[ANIM_FRAMES] = { -2, 2, 6, 6, 3, 0 };
static const s16 punchDip[ANIM_FRAMES]  = { 0, 1, 2, 2, 1, 0 };

/* Palettes (0x0BGR) */
static const u16 palBg[16] =
{
    0x0000, 0x0C42, 0x0E84, 0x0EC8, 0x0468, 0x0246, 0x0224, 0x0642,
    0x0864, 0x08EE, 0, 0, 0, 0, 0, 0
};
static const u16 palFighter1[16] =
{
    0x0000, 0x0222, 0x06AE, 0x048C, 0x0024, 0x0EEE, 0x0CCA, 0x004E,
    0x0422, 0x0246, 0x0EEE, 0x004E, 0, 0, 0, 0
};
static const u16 palFighter2[16] =
{
    0x0000, 0x0222, 0x048C, 0x026A, 0x04CE, 0x0E64, 0x0A42, 0x0EEE,
    0x0422, 0x0246, 0x0EEE, 0x00EE, 0, 0, 0, 0
};

/* ------------------------------------------------------------------ */
/* Software renderer                                                  */
/* ------------------------------------------------------------------ */

/*
 * Buffer layout: sprite column c (32px) occupies tiles c*56 .. c*56+55.
 * Inside it, sprite row r (32px band, 16px for r==3) starts at tile
 * r*16, and tile column txl (0..3) inside the band starts at
 * txl*h tiles (h = tiles per band height). Within one tile column of one
 * band, pixel rows are contiguous u32s: offset = y & 31.
 */

/* Fill [x0,x1) x [y0,y1) with palette index col, only inside column c. */
static void fillRect(u32 *buf, u16 c, s16 x0, s16 y0, s16 x1, s16 y1, u8 col)
{
    const s16 xa = (s16)(c << 5);
    const s16 xb = xa + 32;

    if (x0 < xa) x0 = xa;
    if (y0 < 0) y0 = 0;
    if (x1 > xb) x1 = xb;
    if (y1 > FH) y1 = FH;
    if (x0 >= x1 || y0 >= y1) return;

    const u32 cv  = fillVal[col];
    const s16 tx0 = x0 >> 3;
    const s16 tx1 = (x1 - 1) >> 3;
    const u32 ml  = leftMask[x0 & 7];
    const u32 mr  = rightMask[(x1 - 1) & 7];
    const s16 r0  = y0 >> 5;
    const s16 r1  = (y1 - 1) >> 5;
    u32 *const cb = buf + COL_OFF32(c);

    for (s16 tx = tx0; tx <= tx1; tx++)
    {
        u32 m = 0xFFFFFFFFUL;
        if (tx == tx0) m &= ml;
        if (tx == tx1) m &= mr;
        const u16 txl = tx & 3;

        for (s16 r = r0; r <= r1; r++)
        {
            const u16 hs = (r == 3) ? 1 : 2;       /* log2(tiles per band) */
            const s16 by = r << 5;
            s16 ya = (y0 > by) ? y0 : by;
            s16 yb = (y1 < by + 32) ? y1 : by + 32;
            u32 *p = cb + ((((u16)r << 4) + (txl << hs)) << 3) + (ya - by);
            s16 n  = yb - ya;

            if (m == 0xFFFFFFFFUL)
            {
                while (n >= 4)
                {
                    *p++ = cv; *p++ = cv; *p++ = cv; *p++ = cv;
                    n -= 4;
                }
                while (n-- > 0) *p++ = cv;
            }
            else
            {
                const u32 nm = ~m;
                const u32 cm = cv & m;
                while (n >= 2)
                {
                    *p = (*p & nm) | cm; p++;
                    *p = (*p & nm) | cm; p++;
                    n -= 2;
                }
                if (n > 0) *p = (*p & nm) | cm;
            }
        }
    }
}

#define R(x0, y0, x1, y1, col) fillRect(buf, c, (x0), (y0), (x1), (y1), (col))

/*
 * Render only sprite column c (0..2) of the given pose.
 * Palette indices:
 * 1 outline/pupil  2 skin  3 skin shade  4 hair  5 gi  6 gi shade
 * 7 belt  8 dark pants  9 shoes  10 eye white  11 gloves/headband
 */
static void renderFighter(u32 *buf, u8 anim, u8 frame, u16 c)
{
    s16 b, L, ext = -1;

    memsetU32(buf + COL_OFF32(c), 0, COL_WORDS32);

    if (anim == ANIM_IDLE)
    {
        b = idleBob[frame];
        L = 0;
    }
    else
    {
        b   = punchDip[frame];
        L   = punchLean[frame];
        ext = punchExt[frame];
    }

    /* legs and feet (static) */
    if (c < 2)
    {
        R( 6,  66, 22, 104, 8);         /* back leg   */
        R( 2, 104, 26, 112, 9);         /* back foot  */
        R(28,  66, 46, 104, 5);         /* front leg  */
        R(26, 104, 54, 112, 9);         /* front foot */
        R(28,  66, 33, 104, 6);         /* leg shade  */
    }

    /* back arm (behind torso) */
    R( 6 + L, 36 + b, 14 + L, 56 + b, 6);
    R( 2 + L, 52 + b, 14 + L, 64 + b, 11);

    /* torso */
    R(12 + L, 28 + b, 46 + L, 68 + b, 5);
    R(12 + L, 28 + b, 19 + L, 68 + b, 6);
    R(12 + L, 58 + b, 46 + L, 64 + b, 7);
    R(26 + L, 28 + b, 34 + L, 36 + b, 2);

    /* head */
    R(18 + L,  4 + b, 40 + L, 28 + b, 2);
    R(40 + L, 15 + b, 42 + L, 20 + b, 2);   /* nose  */
    R(16 + L,  2 + b, 42 + L, 10 + b, 4);   /* hair  */
    R(16 + L,  2 + b, 22 + L, 18 + b, 4);
    R(18 + L, 10 + b, 40 + L, 13 + b, 11);  /* band  */
    R(31 + L, 16 + b, 38 + L, 21 + b, 10);  /* eye   */
    R(35 + L, 16 + b, 38 + L, 21 + b, 1);
    R(33 + L, 23 + b, 38 + L, 24 + b, 1);   /* mouth */

    /* front arm */
    if (ext < 0)
    {
        /* guard */
        R(40 + L, 32 + b, 52 + L, 42 + b, 5);
        R(46 + L, 28 + b, 54 + L, 42 + b, 2);
        R(48 + L, 18 + b, 62 + L, 32 + b, 11);
    }
    else
    {
        /* punch */
        R(40 + L, 34 + b, 50 + L, 44 + b, 5);
        R(50 + L, 37 + b, 56 + L + ext, 43 + b, 2);
        R(54 + L + ext, 32 + b, 66 + L + ext, 46 + b, 11);
    }
}

/* ------------------------------------------------------------------ */
/* Fighter logic                                                      */
/* ------------------------------------------------------------------ */

/* Choose the next animation frame; columns are rendered one per tick. */
static void startCycle(Fighter *f)
{
    if (f->anim == ANIM_PUNCH)
    {
        if (f->frame >= ANIM_FRAMES - 1)
        {
            f->nextAnim  = ANIM_IDLE;
            f->nextFrame = 0;
        }
        else
        {
            f->nextAnim  = ANIM_PUNCH;
            f->nextFrame = f->frame + 1;
        }
    }
    else if (f->punchReq)
    {
        f->punchReq  = FALSE;
        f->nextAnim  = ANIM_PUNCH;
        f->nextFrame = 0;
    }
    else
    {
        f->nextAnim  = ANIM_IDLE;
        f->nextFrame = (f->frame + 1) % ANIM_FRAMES;
    }

    f->chunk = 0;
}

static void initFighter(Fighter *f, s16 x, s16 y, u16 pal, bool flip,
                        u16 slot0, u16 slot1, u32 *buf, u8 timer)
{
    memset(f, 0, sizeof(Fighter));
    f->x = x;
    f->y = y;
    f->pal = pal;
    f->flip = flip;
    f->vram[0] = slot0;
    f->vram[1] = slot1;
    f->buf = buf;
    f->timer = timer;
    f->anim = ANIM_IDLE;
    f->frame = 0;

    /* frame 0 into the visible slot */
    for (u16 c = 0; c < CHUNKS; c++) renderFighter(buf, ANIM_IDLE, 0, c);
    VDP_loadTileData(buf, f->vram[0], FTILES, CPU);

    /* frame 1 into the hidden slot (display is off, so CPU copy is fine) */
    startCycle(f);
    for (u16 c = 0; c < CHUNKS; c++) renderFighter(buf, f->nextAnim, f->nextFrame, c);
    VDP_loadTileData(buf, f->vram[1], FTILES, CPU);
    f->chunk = CHUNKS;
}

/* Called once per video frame. */
static void fighterTick(Fighter *f)
{
    if (++f->timer >= ANIM_TICKS)
    {
        f->timer = 0;
        f->shown ^= 1;                  /* hidden slot is complete */
        f->anim  = f->nextAnim;
        f->frame = f->nextFrame;
        startCycle(f);                  /* pick next frame */
    }

    /* render one column, then stream it (1792 bytes) into the HIDDEN slot */
    if (f->chunk < CHUNKS)
    {
        const u16 ch   = f->chunk;
        const u16 tile = f->vram[f->shown ^ 1] + COL_OFF_TILES(ch);

        renderFighter(f->buf, f->nextAnim, f->nextFrame, ch);
        DMA_queueDma(DMA_VRAM,
                     f->buf + COL_OFF32(ch),
                     tile * 32,
                     COL_TILES * 16,        /* words */
                     2);
        f->chunk++;
    }
}

static void updateFighterSprites(const Fighter *f, u16 first)
{
    const u16 base = f->vram[f->shown];

    for (u16 c = 0; c < 3; c++)
    {
        for (u16 r = 0; r < 4; r++)
        {
            const u16 idx  = first + c * 4 + r;
            const s16 sx   = f->x + (f->flip ? (2 - c) * 32 : c * 32);
            const s16 sy   = f->y + r * 32;
            const u8  h    = (r == 3) ? 2 : 4;
            const u16 tile = base + COL_OFF_TILES(c) + r * 16;

            VDP_setSpriteFull(idx, sx, sy, SPRITE_SIZE(4, h),
                              TILE_ATTR_FULL(f->pal, FALSE, FALSE, f->flip, tile),
                              (idx == SPRITES_TOTAL - 1) ? 0 : idx + 1);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Background                                                         */
/* ------------------------------------------------------------------ */

static void makeSolid(u32 *t, u8 col)
{
    for (u16 i = 0; i < 8; i++) t[i] = fillVal[col];
}

static void initBackground(void)
{
    makeSolid(bgTiles[0], 1);
    makeSolid(bgTiles[1], 2);
    makeSolid(bgTiles[2], 3);
    makeSolid(bgTiles[3], 7);                       /* building        */

    makeSolid(bgTiles[4], 7);                       /* lit window      */
    for (u16 i = 2; i < 6; i++) bgTiles[4][i] = 0x77999977UL;

    makeSolid(bgTiles[5], 4);                       /* floor light     */
    makeSolid(bgTiles[6], 5);                       /* floor dark      */
    bgTiles[5][0] = 0x66666666UL;
    bgTiles[6][0] = 0x66666666UL;

    VDP_loadTileData(&bgTiles[0][0], BG_TILES, BG_TILE_COUNT, CPU);

    /* sky bands */
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_SKY1), 0, 0, 40, 8);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_SKY2), 0, 8, 40, 8);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, T_SKY3), 0, 16, 40, 10);

    /* skyline */
    for (u16 x = 0; x < 40; x++)
    {
        const u16 hgt = 3 + (((x * 7) ^ (x >> 1)) % 6);
        for (u16 y = 26 - hgt; y < 26; y++)
        {
            const u16 t = (y > 26 - hgt && ((x + y) & 3) == 0) ? T_WIN : T_BLD;
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, t), x, y);
        }
    }

    /* floor (rows 26-27 = y 208..223) */
    for (u16 y = 26; y < 28; y++)
    {
        for (u16 x = 0; x < 40; x++)
        {
            const u16 t = ((x + (y - 26)) & 1) ? T_FLB : T_FLA;
            VDP_setTileMapXY(BG_B, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, t), x, y);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    u16 prevPad = 0;

    VDP_setEnable(FALSE);
    VDP_setScreenWidth320();

    PAL_setPalette(PAL0, palBg, CPU);
    PAL_setPalette(PAL1, palFighter1, CPU);
    PAL_setPalette(PAL2, palFighter2, CPU);

    initBackground();

    /* P2's timer starts one tick ahead so the two fighters' render work and
       DMA chunks are staggered. */
    initFighter(&p1,  40, FLOOR_Y - FH, PAL1, FALSE, P1_SLOT0, P1_SLOT1, bufP1, 0);
    initFighter(&p2, 184, FLOOR_Y - FH, PAL2, TRUE,  P2_SLOT0, P2_SLOT1, bufP2, 1);

    updateFighterSprites(&p1, 0);
    updateFighterSprites(&p2, SPR_PER_FIGHTER);
    VDP_updateSprites(SPRITES_TOTAL, CPU);

    VDP_setEnable(TRUE);

    while (TRUE)
    {
        const u16 pad = JOY_readJoypad(JOY_1);

        /* --- player 1 input --- */
        if (p1.anim == ANIM_IDLE)
        {
            if (pad & BUTTON_LEFT)  p1.x -= 2;
            if (pad & BUTTON_RIGHT) p1.x += 2;
            if (p1.x < 0) p1.x = 0;
            if (p1.x > p2.x - 20) p1.x = p2.x - 20;
        }

        if ((pad & BUTTON_A) && !(prevPad & BUTTON_A) &&
            p1.anim == ANIM_IDLE && p1.nextAnim == ANIM_IDLE)
        {
            p1.punchReq = TRUE;
        }
        prevPad = pad;

        /* --- animation / streaming --- */
        fighterTick(&p1);
        fighterTick(&p2);

        /* --- sprites (192 bytes via the DMA queue) --- */
        updateFighterSprites(&p1, 0);
        updateFighterSprites(&p2, SPR_PER_FIGHTER);
        VDP_updateSprites(SPRITES_TOTAL, DMA_QUEUE);

        /* waits for vblank, flushes the DMA queue, reads pads */
        SYS_doVBlankProcess();
    }

    return 0;
}
