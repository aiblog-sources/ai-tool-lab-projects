// main.c - SGDK 2.11 fighting game scene (no resource files)
//
// - Two 96x112 fighters, each with a 6-frame idle and a 6-frame punch animation.
// - Animation frame changes every 3 video frames.
// - Player 1 (pad 1): D-pad walks left/right, A punches. Player 2 stands still (idle only).
// - All graphics are generated in code at startup.
//
// Memory strategy:
//   * 6 idle frames (6 x 168 tiles) are rendered once and kept in VRAM, shared by both
//     fighters (different palettes, P2 is flipped horizontally).
//   * 6 punch frames are rendered once into RAM and the current one is DMA'd
//     into one 168-tile VRAM buffer when the frame changes.
//   * Each fighter is built from 12 hardware sprites (3 x 4 grid: 32x32 blocks, last row 32x16).

#include <genesis.h>

#define FW              96
#define FH              112
#define FTW             (FW / 8)            // 12 tiles wide
#define FTH             (FH / 8)            // 14 tiles tall
#define FTILES          (FTW * FTH)         // 168 tiles per frame

#define NUM_BG_TILES    11
#define BG_BASE         (TILE_USER_INDEX)
#define IDLE_BASE       (BG_BASE + NUM_BG_TILES)
#define DYN_BASE        (IDLE_BASE + 6 * FTILES)

#define FIGHT_Y         100                 // sprite top Y (feet land on the floor)
#define P2_X            190
#define P1_START_X      40
#define P1_MIN_X        (-8)
#define P1_MAX_X        (P2_X - 50)
#define WALK_SPEED      2
#define ANIM_TICKS      3

// ---------------------------------------------------------------------------
// Palettes (0x0BGR)
// ---------------------------------------------------------------------------
static const u16 palBg[16] =
{
    0x0000, 0x0A40, 0x0C62, 0x0A86, 0x06AE, 0x0424, 0x0248, 0x0246,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0EEE
};

static const u16 palP1[16] =
{
    0x0000, 0x0222, 0x06AE, 0x0CCC, 0x0A98, 0x0228, 0x04CE, 0x0024,
    0x0EEE, 0x0EEC, 0x0E62, 0x04EE, 0x0000, 0x0000, 0x0000, 0x0000
};

static const u16 palP2[16] =
{
    0x0000, 0x0222, 0x048C, 0x0666, 0x0444, 0x0262, 0x00AE, 0x002A,
    0x0EEE, 0x0ECA, 0x02CE, 0x04EE, 0x0000, 0x0000, 0x0000, 0x0000
};

// ---------------------------------------------------------------------------
// Buffers
// ---------------------------------------------------------------------------
static u8  canvas[FH][FW];                  // 8-bit-per-pixel work canvas (10752 bytes)
static u32 punchTiles[6][FTILES * 8];       // 6 punch frames in RAM (32256 bytes)
static u32 bgTiles[NUM_BG_TILES][8];

// ---------------------------------------------------------------------------
// Tiny software drawing library (draws into canvas)
// ---------------------------------------------------------------------------
static void cvRect(s16 x, s16 y, s16 w, s16 h, u8 c)
{
    s16 x1 = x + w;
    s16 y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > FW) x1 = FW;
    if (y1 > FH) y1 = FH;
    if (x >= x1 || y >= y1) return;
    for (s16 yy = y; yy < y1; yy++)
        memset(&canvas[yy][x], c, x1 - x);
}

static s16 isq(s32 v)
{
    s16 r = 0;
    while ((s32)(r + 1) * (r + 1) <= v) r++;
    return r;
}

// Thick line made by stamping discs. Optionally with a 1px dark outline.
static void cvStroke(s16 x0, s16 y0, s16 x1, s16 y1, s16 r, u8 c, u8 outline)
{
    s16 dx = x1 - x0;
    s16 dy = y1 - y0;
    s16 adx = dx < 0 ? -dx : dx;
    s16 ady = dy < 0 ? -dy : dy;
    s16 n = adx > ady ? adx : ady;
    s16 st = (r >> 1) + 1;

    for (u8 pass = outline ? 0 : 1; pass < 2; pass++)
    {
        s16 rr = pass ? r : r + 1;
        u8 col = pass ? c : 1;
        s16 hw[16];
        for (s16 k = 0; k <= rr; k++) hw[k] = isq((s32)rr * rr - k * k);

        s16 i = 0;
        for (;;)
        {
            s16 x = n ? x0 + (s16)((s32)dx * i / n) : x0;
            s16 y = n ? y0 + (s16)((s32)dy * i / n) : y0;
            for (s16 k = -rr; k <= rr; k++)
            {
                s16 w = hw[k < 0 ? -k : k];
                cvRect(x - w, y + k, w * 2 + 1, 1, col);
            }
            if (i >= n) break;
            i += st;
            if (i > n) i = n;
        }
    }
}

static void cvDisc(s16 cx, s16 cy, s16 r, u8 c)
{
    cvStroke(cx, cy, cx, cy, r, c, 0);
}

static void cvEllipse(s16 cx, s16 cy, s16 rx, s16 ry, u8 c)
{
    for (s16 dy = -ry; dy <= ry; dy++)
    {
        s32 t = (s32)rx * rx * ((s32)ry * ry - (s32)dy * dy) / ((s32)ry * ry);
        s16 w = isq(t);
        cvRect(cx - w, cy + dy, w * 2 + 1, 1, c);
    }
}

static void drawLeg(s16 hx, s16 hy, s16 kx, s16 ky, s16 ax)
{
    cvStroke(hx, hy, kx, ky, 7, 5, 1);          // thigh
    cvStroke(kx, ky, ax, 102, 6, 5, 1);         // calf
    cvRect(ax - 7, 99, 22, 13, 1);              // boot outline
    cvRect(ax - 6, 100, 20, 11, 10);            // boot
    cvRect(ax - 6, 100, 8, 3, 5);               // pant cuff
}

static void drawArm(s16 sx, s16 sy, s16 ex, s16 ey, s16 fx, s16 fy)
{
    cvStroke(sx, sy, ex, ey, 4, 3, 1);          // sleeve
    cvStroke(ex, ey, fx, fy, 3, 2, 1);          // forearm
    cvDisc(fx, fy, 7, 1);                       // glove outline
    cvDisc(fx, fy, 6, 10);                      // glove
    cvRect(fx - 3, fy - 3, 3, 2, 9);            // glove shine
}

// Renders one 96x112 fighter frame (facing right) into canvas.
static void renderFrame(u8 punch, u8 f)
{
    static const s8 bobI[6]   = { 0, 1, 2, 3, 2, 1 };
    static const s8 swayI[6]  = { 0, -1, -2, -2, -1, 0 };
    static const s8 leanI[6]  = { 0, 0, 1, 1, 1, 0 };
    static const s8 bobP[6]   = { 2, 2, 3, 3, 2, 1 };
    static const s8 leanP[6]  = { -3, 1, 5, 6, 3, 0 };
    static const s8 fistXP[6] = { 50, 68, 84, 85, 70, 56 };
    static const s8 fistYO[6] = { 8, 4, 2, 2, 4, 6 };
    static const s8 sagP[6]   = { 10, 6, 1, 0, 3, 8 };

    memset(canvas, 0, sizeof(canvas));

    s16 b    = punch ? bobP[f]  : bobI[f];
    s16 lean = punch ? leanP[f] : leanI[f];
    s16 sway = punch ? 0 : swayI[f];

    s16 hipX = 36, hipY = 66 + b;
    s16 shX = 40 + lean, shY = 38 + b;
    s16 s1x = shX + 6, s1y = shY + 2;           // front shoulder
    s16 s2x = shX - 6, s2y = shY + 2;           // back shoulder

    s16 fx, fy, ex, ey;
    if (punch)
    {
        fx = fistXP[f];
        fy = shY + fistYO[f];
        ex = (s1x + fx) / 2;
        ey = (s1y + fy) / 2 + sagP[f];
    }
    else
    {
        fx = shX + 30;
        fy = shY - 2 + sway;
        ex = shX + 20;
        ey = shY + 14;
    }

    // motion streaks (behind everything)
    if (punch && f >= 1 && f <= 3)
    {
        cvRect(fx - 36, fy - 13, 24, 2, 9);
        cvRect(fx - 30, fy + 11, 20, 2, 9);
        cvRect(fx - 40, fy - 1, 12, 1, 9);
    }

    // back arm (guard)
    drawArm(s2x, s2y, shX + 2, shY + 20, shX + 18, shY + 2 + sway);

    // legs: back then front
    drawLeg(hipX - 4, hipY + 2, (hipX - 4 + 16) / 2 - 4, (hipY + 2 + 102) / 2, 16);
    drawLeg(hipX + 4, hipY + 2, (hipX + 4 + 58) / 2 + 4, (hipY + 2 + 102) / 2, 58);

    // torso (gi) with shading, collar and belt
    cvStroke(hipX + 2, hipY - 2, shX, shY, 10, 3, 1);
    cvStroke(hipX - 5, hipY - 4, shX - 6, shY + 4, 3, 4, 0);
    cvStroke(shX + 5, shY - 6, shX + 5, shY - 6, 3, 2, 0);
    cvRect(hipX - 12, hipY - 8, 26, 6, 1);
    cvRect(hipX - 11, hipY - 7, 24, 4, 6);
    cvRect(hipX + 10, hipY - 3, 4, 9, 1);
    cvRect(hipX + 11, hipY - 3, 2, 8, 6);

    // neck + head
    cvRect(shX + 2, shY - 12, 8, 8, 2);
    s16 hx = shX + 6, hy = shY - 18;
    cvDisc(hx, hy, 12, 1);                      // outline
    cvDisc(hx, hy, 11, 7);                      // hair
    cvEllipse(hx + 2, hy + 3, 9, 8, 2);         // face
    cvRect(hx - 10, hy - 4, 20, 3, 6);          // headband
    cvRect(hx - 15, hy - 3, 6, 2, 6);           // headband tails
    cvRect(hx - 17, hy + 0, 5, 2, 6);
    cvRect(hx + 4, hy + 1, 4, 3, 8);            // eye
    cvRect(hx + 6, hy + 1, 2, 3, 1);            // pupil
    cvRect(hx + 3, hy, 6, 1, 1);                // brow
    cvRect(hx + 5, hy + 7, 4, 1, 1);            // mouth

    // front arm (the punching arm)
    drawArm(s1x, s1y, ex, ey, fx, fy);

    // impact spark
    if (punch && f == 3)
    {
        cvRect(93, fy - 1, 3, 3, 11);
        cvRect(92, fy - 6, 2, 3, 11);
        cvRect(92, fy + 4, 2, 3, 11);
    }
}

// ---------------------------------------------------------------------------
// Convert canvas to VDP tiles, laid out in hardware-sprite order:
// 12 blocks of 4x4 tiles (last row 4x2), each stored column-major.
// ---------------------------------------------------------------------------
static u16 blockStart(u16 bx, u16 by)
{
    return (by < 3) ? (by * 3 + bx) * 16 : 144 + bx * 8;
}

static u16 tileIdx(u16 tx, u16 ty)
{
    u16 bx = tx >> 2, by = ty >> 2;
    u16 bh = (by == 3) ? 2 : 4;
    return blockStart(bx, by) + (tx & 3) * bh + (ty & 3);
}

static void cvToTiles(u32 *dst)
{
    for (u16 ty = 0; ty < FTH; ty++)
    {
        for (u16 tx = 0; tx < FTW; tx++)
        {
            u32 *t = dst + tileIdx(tx, ty) * 8;
            for (u16 r = 0; r < 8; r++)
            {
                const u8 *p = &canvas[ty * 8 + r][tx * 8];
                t[r] = ((u32)p[0] << 28) | ((u32)p[1] << 24) | ((u32)p[2] << 20) | ((u32)p[3] << 16) |
                       ((u32)p[4] << 12) | ((u32)p[5] << 8)  | ((u32)p[6] << 4)  |  (u32)p[7];
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Background
// ---------------------------------------------------------------------------
static void buildBgTiles(void)
{
    for (u16 r = 0; r < 8; r++)
    {
        bgTiles[0][r]  = 0x11111111;
        bgTiles[1][r]  = 0x22222222;
        bgTiles[2][r]  = 0x33333333;
        bgTiles[3][r]  = 0x44444444;
        bgTiles[4][r]  = (r < 3) ? 0x44444444 : 0x55555555;   // short skyline
        bgTiles[5][r]  = (r < 1) ? 0x44444444 : 0x55555555;   // tall skyline
        bgTiles[6][r]  = 0x55555555;
        bgTiles[7][r]  = 0x77777777;
        bgTiles[8][r]  = 0x66666666;
        bgTiles[9][r]  = 0x67676767;
        bgTiles[10][r] = 0x76767676;
    }
}

#define BGA(t)  TILE_ATTR_FULL(PAL0, 0, 0, 0, BG_BASE + (t))

static void drawBackground(void)
{
    VDP_fillTileMapRect(BG_B, BGA(0), 0, 0, 40, 6);
    VDP_fillTileMapRect(BG_B, BGA(1), 0, 6, 40, 5);
    VDP_fillTileMapRect(BG_B, BGA(2), 0, 11, 40, 4);
    VDP_fillTileMapRect(BG_B, BGA(3), 0, 15, 40, 2);

    for (u16 x = 0; x < 40; x++)
    {
        u16 v = (x * 5 + x / 3) & 3;
        u16 t = (v == 0) ? 5 : ((v == 3) ? 3 : 4);
        VDP_setTileMapXY(BG_B, BGA(t), x, 17);
    }
    VDP_fillTileMapRect(BG_B, BGA(6), 0, 18, 40, 2);
    VDP_fillTileMapRect(BG_B, BGA(7), 0, 20, 40, 1);

    for (u16 y = 21; y < 28; y++)
    {
        if (y & 1)
        {
            VDP_fillTileMapRect(BG_B, BGA(8), 0, y, 40, 1);
        }
        else
        {
            for (u16 x = 0; x < 40; x++)
                VDP_setTileMapXY(BG_B, BGA(((x + (y >> 1)) & 1) ? 9 : 10), x, y);
        }
    }
}

// ---------------------------------------------------------------------------
// Fighters / sprites
// ---------------------------------------------------------------------------
typedef struct
{
    s16 x;
    u8  frame;
    u8  timer;
    u8  punching;
} Fighter;

// Sets the 12 hardware sprites of a fighter (consecutive sprite indices).
static void placeFighter(u16 first, s16 x, s16 y, u16 baseTile, u16 pal, u8 flip, u8 last)
{
    for (u16 by = 0; by < 4; by++)
    {
        for (u16 bx = 0; bx < 3; bx++)
        {
            u16 idx  = first + by * 3 + bx;
            s16 sx   = flip ? (x + (2 - bx) * 32) : (x + bx * 32);
            u8  size = (by == 3) ? SPRITE_SIZE(4, 2) : SPRITE_SIZE(4, 4);
            u8  link = (last && by == 3 && bx == 2) ? 0 : (u8)(idx + 1);
            VDP_setSpriteFull(idx, sx, y + by * 32, size,
                              TILE_ATTR_FULL(pal, 0, 0, flip, baseTile + blockStart(bx, by)),
                              link);
        }
    }
}

static void uploadPunchFrame(u8 f)
{
    VDP_loadTileData(punchTiles[f], DYN_BASE, FTILES, DMA_QUEUE);
}

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setEnable(FALSE);

    PAL_setColors(0,  palBg, 16, CPU);
    PAL_setColors(16, palP1, 16, CPU);
    PAL_setColors(32, palP2, 16, CPU);

    // Background
    buildBgTiles();
    VDP_loadTileData((const u32 *)bgTiles, BG_BASE, NUM_BG_TILES, CPU);
    drawBackground();
    VDP_drawText("D-PAD: WALK    A: PUNCH", 2, 2);

    // Idle frames: render and load straight into VRAM (punchTiles[0] used as scratch)
    for (u8 f = 0; f < 6; f++)
    {
        renderFrame(0, f);
        cvToTiles(punchTiles[0]);
        VDP_loadTileData(punchTiles[0], IDLE_BASE + f * FTILES, FTILES, CPU);
    }

    // Punch frames: render into RAM (uploaded to VRAM on demand)
    for (u8 f = 0; f < 6; f++)
    {
        renderFrame(1, f);
        cvToTiles(punchTiles[f]);
    }
    VDP_loadTileData(punchTiles[0], DYN_BASE, FTILES, CPU);

    Fighter p1 = { P1_START_X, 0, 0, FALSE };
    Fighter p2 = { P2_X, 0, 0, FALSE };
    u16 prevPad = 0;

    VDP_setEnable(TRUE);

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);
        u16 pressed = pad & ~prevPad;
        prevPad = pad;

        // ---- Player 1: input ----
        u8 startedPunch = FALSE;
        if (!p1.punching)
        {
            if (pad & BUTTON_LEFT)  p1.x -= WALK_SPEED;
            if (pad & BUTTON_RIGHT) p1.x += WALK_SPEED;
            if (p1.x < P1_MIN_X) p1.x = P1_MIN_X;
            if (p1.x > P1_MAX_X) p1.x = P1_MAX_X;

            if (pressed & BUTTON_A)
            {
                p1.punching = TRUE;
                p1.frame = 0;
                p1.timer = 0;
                uploadPunchFrame(0);
                startedPunch = TRUE;
            }
        }

        // ---- Player 1: animation (frame changes every 3 video frames) ----
        if (!startedPunch)
        {
            if (++p1.timer >= ANIM_TICKS)
            {
                p1.timer = 0;
                p1.frame++;
                if (p1.punching)
                {
                    if (p1.frame >= 6)
                    {
                        p1.punching = FALSE;
                        p1.frame = 0;
                    }
                    else
                    {
                        uploadPunchFrame(p1.frame);
                    }
                }
                else if (p1.frame >= 6)
                {
                    p1.frame = 0;
                }
            }
        }

        // ---- Player 2: idle animation only ----
        if (++p2.timer >= ANIM_TICKS)
        {
            p2.timer = 0;
            if (++p2.frame >= 6) p2.frame = 0;
        }

        // ---- Sprites ----
        u16 base1 = p1.punching ? DYN_BASE : (IDLE_BASE + p1.frame * FTILES);
        u16 base2 = IDLE_BASE + p2.frame * FTILES;
        placeFighter(0,  p1.x, FIGHT_Y, base1, PAL1, FALSE, FALSE);
        placeFighter(12, p2.x, FIGHT_Y, base2, PAL2, TRUE,  TRUE);
        VDP_updateSprites(24, DMA_QUEUE);

        SYS_doVBlankProcess();
    }

    return 0;
}
