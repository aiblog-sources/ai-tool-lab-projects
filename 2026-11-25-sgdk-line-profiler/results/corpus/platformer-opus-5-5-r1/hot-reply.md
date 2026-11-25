The profile shows little real game work. Most of the "busy" time is `SYS_doVBlankProcess` waiting for and servicing VBlank, which is fixed overhead. I didn't build or run this version, so the savings below are estimates from your profile's numbers, not new measurements.

These are the parts the game controls:

1. **`drawLevel` (line 199, 324 cycles per tile):** it now fills a RAM tilemap and sends it with one `VDP_setTileMapDataRect(..., CPU)` call. The lint gives about 37 cycles per tile for that call. The tile values are the same and the upload is still immediate CPU writes, so the result on screen is the same.
2. **`VDP_updateSprites` (about 1,300 cycles every frame):** the sprite table is now set and queued only when the player's position or facing changes. Standing still costs nearly nothing. The table in VRAM is identical either way.
3. **`sprintf` in `drawScore`:** replaced with a small decimal formatter that produces the same text (`SCORE %05u`, `ROUND %u`).
4. **Collision and coin checks:** made cheaper. `isSolid` is inlined, and the coin scan reads level rows through a pointer. These checks are now written as unsigned comparisons; the logic is the same.

```c
// main.c - One-screen platformer for SGDK 2.09
// No resource files: all graphics are defined as const u32 arrays in this file.
// Controls (pad 1): LEFT/RIGHT to move, A to jump (hold for a higher jump)

#include <genesis.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define LVL_W           40
#define LVL_H           28

#define TILE_SOLID      (TILE_USER_INDEX + 0)
#define TILE_COIN       (TILE_USER_INDEX + 1)
#define TILE_PLAYER     (TILE_USER_INDEX + 2)   // 4 tiles (16x16 sprite)

// Fixed point: 8 fractional bits
#define FP_SHIFT        8
#define TO_FP(v)        ((s32)(v) << FP_SHIFT)
#define FROM_FP(v)      ((s16)((v) >> FP_SHIFT))

#define GRAVITY         0x40        // 0.25 px/frame^2
#define JUMP_SPEED      0x500       // 5 px/frame
#define MAX_FALL        0x600       // 6 px/frame (less than a tile: no tunnelling)
#define WALK_SPEED      0x180       // 1.5 px/frame

// Player hitbox (inside the 16x16 sprite)
#define HB_OFS_X        3
#define HB_W            10
#define HB_H            16

#define COIN_POINTS     10

#define ATTR_SOLID      TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_SOLID)
#define ATTR_COIN       TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE, TILE_COIN)

// ---------------------------------------------------------------------------
// Tile graphics (4bpp, 8 pixels per u32, one nibble per pixel)
// ---------------------------------------------------------------------------
static const u32 tileSolid[8] =
{
    0x22222221,
    0x22222221,
    0x22222221,
    0x11111111,
    0x22212222,
    0x22212222,
    0x22212222,
    0x11111111
};

static const u32 tileCoin[8] =
{
    0x00055000,
    0x00544500,
    0x05444450,
    0x054A4450,
    0x05444450,
    0x05444450,
    0x00544500,
    0x00055000
};

// 16x16 player, sprite tile order is column-major:
// tile0 = top-left, tile1 = bottom-left, tile2 = top-right, tile3 = bottom-right
static const u32 tilePlayer[4 * 8] =
{
    // top-left
    0x00000777,
    0x00007777,
    0x00006666,
    0x00066666,
    0x00066666,
    0x00000666,
    0x00008888,
    0x00088888,
    // bottom-left
    0x00688888,
    0x06688888,
    0x06688888,
    0x00088888,
    0x00088800,
    0x00088000,
    0x00999000,
    0x09999000,
    // top-right
    0x77700000,
    0x77777000,
    0x69600000,
    0x69666000,
    0x66660000,
    0x66600000,
    0x88800000,
    0x88880000,
    // bottom-right
    0x88886000,
    0x88886600,
    0x88886600,
    0x88880000,
    0x08880000,
    0x00880000,
    0x00999000,
    0x00999900
};

// Palette 1 (level + player)
static const u16 palGame[16] =
{
    0x0000,                         // 0 transparent
    RGB24_TO_VDPCOLOR(0x502800),    // 1 dark brown (mortar)
    RGB24_TO_VDPCOLOR(0xB05828),    // 2 brick
    RGB24_TO_VDPCOLOR(0x20A020),    // 3 green
    RGB24_TO_VDPCOLOR(0xFFE000),    // 4 yellow
    RGB24_TO_VDPCOLOR(0xA07000),    // 5 dark yellow
    RGB24_TO_VDPCOLOR(0xFFC090),    // 6 skin
    RGB24_TO_VDPCOLOR(0xE02020),    // 7 red
    RGB24_TO_VDPCOLOR(0x2040E0),    // 8 blue
    RGB24_TO_VDPCOLOR(0x202020),    // 9 black
    RGB24_TO_VDPCOLOR(0xFFFFFF),    // 10 white
    0, 0, 0, 0, 0
};

// ---------------------------------------------------------------------------
// Level: 40x28 tiles.  '#' = solid, '.' = empty, 'o' = coin (empty + pickup)
// Each row is written as four 10-character chunks (columns 0-9, 10-19, 20-29, 30-39)
// ---------------------------------------------------------------------------
static const char levelSrc[LVL_H][LVL_W + 1] =
{
    /*  0 */ "#........." ".........." ".........." ".........#",
    /*  1 */ "#........." ".........." ".........." ".........#",
    /*  2 */ "#........." ".........." ".........." ".........#",
    /*  3 */ "#........." ".........." ".........." ".........#",
    /*  4 */ "#........." ".........." ".........." ".........#",
    /*  5 */ "#......ooo" "o........." ".........o" "ooo......#",
    /*  6 */ "#.....####" "##........" "........##" "####.....#",
    /*  7 */ "#........." ".........." ".........." ".........#",
    /*  8 */ "#........." ".........." ".........." ".........#",
    /*  9 */ "#........." "........oo" "oo........" ".........#",
    /* 10 */ "#........." "......####" "####......" ".........#",
    /* 11 */ "#........." ".........." ".........." ".........#",
    /* 12 */ "#........." ".........." ".........." ".........#",
    /* 13 */ "#.....oooo" ".........." ".........." "oooo.....#",
    /* 14 */ "#...######" "##........" "........##" "######...#",
    /* 15 */ "#........." ".........." ".........." ".........#",
    /* 16 */ "#........." ".........." ".........." ".........#",
    /* 17 */ "#........." ".......ooo" "ooo......." ".........#",
    /* 18 */ "#........." "....######" "######...." ".........#",
    /* 19 */ "#........." ".........." ".........." ".........#",
    /* 20 */ "#........." ".........." ".........." ".........#",
    /* 21 */ "#....oo..." ".........." ".........." "..oo.....#",
    /* 22 */ "#...######" ".........." ".........." "######...#",
    /* 23 */ "#........." ".........." ".........." ".........#",
    /* 24 */ "#........." ".........." ".........." ".........#",
    /* 25 */ "#........." "..o..o..o." ".o..o..o.." ".........#",
    /* 26 */ "##########" "##########" "##########" "##########",
    /* 27 */ "##########" "##########" "##########" "##########"
};

// Working copy (coins get removed when collected)
static char level[LVL_H][LVL_W];
static u16 coinsLeft;

// Tilemap staging buffer for one-shot plane upload
static u16 mapBuf[LVL_H * LVL_W];

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------
static s32 px, py;      // hitbox top-left, fixed point
static s32 vx, vy;      // velocity, fixed point
static bool onGround;
static bool facingLeft;
static u16 score;
static u16 round;

// Last sprite state sent to the VDP (to skip redundant uploads)
static s16 lastSx, lastSy;
static bool lastFacing;
static bool spriteDirty;

// ---------------------------------------------------------------------------
// Level helpers
// ---------------------------------------------------------------------------
static void loadLevel(void)
{
    u16 coins = 0;
    for (u16 y = 0; y < LVL_H; y++)
    {
        const char *src = levelSrc[y];
        char *dst = level[y];
        for (u16 x = 0; x < LVL_W; x++)
        {
            char c = src[x];
            dst[x] = c;
            if (c == 'o') coins++;
        }
    }
    coinsLeft = coins;
}

static void drawLevel(void)
{
    const char *src = &level[0][0];
    u16 *dst = mapBuf;

    for (u16 i = 0; i < LVL_W * LVL_H; i++)
    {
        char c = *src++;
        u16 attr = 0;   // tile 0 = transparent

        if (c == '#')      attr = ATTR_SOLID;
        else if (c == 'o') attr = ATTR_COIN;

        *dst++ = attr;
    }

    VDP_setTileMapDataRect(BG_A, mapBuf, 0, 0, LVL_W, LVL_H, LVL_W, CPU);
}

static inline bool isSolid(s16 tx, s16 ty)
{
    if ((u16)tx >= LVL_W) return TRUE;      // covers tx < 0 too
    if (ty < 0) return FALSE;
    if (ty >= LVL_H) return TRUE;
    return level[ty][tx] == '#';
}

// ---------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------
// Writes v in decimal, zero-padded to at least minDigits; returns end pointer
static char *putU16(char *p, u16 v, u16 minDigits)
{
    char tmp[5];
    u16 n = 0;

    do
    {
        tmp[n++] = '0' + (v % 10);
        v /= 10;
    } while (v);

    while (n < minDigits) tmp[n++] = '0';
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

static void drawScore(void)
{
    char buf[16];

    buf[0] = 'S'; buf[1] = 'C'; buf[2] = 'O'; buf[3] = 'R'; buf[4] = 'E'; buf[5] = ' ';
    putU16(buf + 6, score, 5);
    VDP_drawText(buf, 2, 1);

    buf[0] = 'R'; buf[1] = 'O'; buf[2] = 'U'; buf[3] = 'N'; buf[4] = 'D'; buf[5] = ' ';
    putU16(buf + 6, round, 1);
    VDP_drawText(buf, 30, 1);
}

// ---------------------------------------------------------------------------
// Player
// ---------------------------------------------------------------------------
static void resetPlayer(void)
{
    px = TO_FP(3 * 8);
    py = TO_FP(26 * 8 - HB_H);
    vx = 0;
    vy = 0;
    onGround = TRUE;
    facingLeft = FALSE;
}

static void moveX(void)
{
    if (vx == 0) return;    // nothing moves, no collision can change

    px += vx;

    s16 x = FROM_FP(px);
    s16 y = FROM_FP(py);
    s16 ty0 = y >> 3;
    s16 ty1 = (y + HB_H - 1) >> 3;

    if (vx > 0)
    {
        s16 tx = (x + HB_W - 1) >> 3;
        for (s16 ty = ty0; ty <= ty1; ty++)
        {
            if (isSolid(tx, ty))
            {
                x = (tx << 3) - HB_W;
                px = TO_FP(x);
                vx = 0;
                break;
            }
        }
    }
    else
    {
        s16 tx = x >> 3;
        for (s16 ty = ty0; ty <= ty1; ty++)
        {
            if (isSolid(tx, ty))
            {
                x = (tx + 1) << 3;
                px = TO_FP(x);
                vx = 0;
                break;
            }
        }
    }
}

static void moveY(void)
{
    py += vy;
    onGround = FALSE;

    s16 x = FROM_FP(px);
    s16 y = FROM_FP(py);
    s16 tx0 = x >> 3;
    s16 tx1 = (x + HB_W - 1) >> 3;

    if (vy > 0)
    {
        s16 ty = (y + HB_H - 1) >> 3;
        for (s16 tx = tx0; tx <= tx1; tx++)
        {
            if (isSolid(tx, ty))
            {
                y = (ty << 3) - HB_H;
                py = TO_FP(y);
                vy = 0;
                onGround = TRUE;
                break;
            }
        }
    }
    else if (vy < 0)
    {
        s16 ty = y >> 3;
        for (s16 tx = tx0; tx <= tx1; tx++)
        {
            if (isSolid(tx, ty))
            {
                y = (ty + 1) << 3;
                py = TO_FP(y);
                vy = 0;     // bonk head
                break;
            }
        }
    }
}

static void collectCoins(void)
{
    s16 x = FROM_FP(px);
    s16 y = FROM_FP(py);
    s16 tx0 = x >> 3;
    s16 tx1 = (x + HB_W - 1) >> 3;
    s16 ty0 = y >> 3;
    s16 ty1 = (y + HB_H - 1) >> 3;
    bool changed = FALSE;

    if (tx0 < 0) tx0 = 0;
    if (tx1 > LVL_W - 1) tx1 = LVL_W - 1;
    if (ty0 < 0) ty0 = 0;
    if (ty1 > LVL_H - 1) ty1 = LVL_H - 1;

    for (s16 ty = ty0; ty <= ty1; ty++)
    {
        char *row = level[ty];
        for (s16 tx = tx0; tx <= tx1; tx++)
        {
            if (row[tx] == 'o')
            {
                row[tx] = '.';
                VDP_setTileMapXY(BG_A, 0, tx, ty);
                score += COIN_POINTS;
                coinsLeft--;
                changed = TRUE;
            }
        }
    }

    if (changed)
    {
        if (coinsLeft == 0)
        {
            // All coins collected: start a new round with the coins restored
            round++;
            loadLevel();
            drawLevel();
        }
        drawScore();
    }
}

static void updatePlayer(u16 joy, u16 joyPrev)
{
    // Horizontal input
    if (joy & BUTTON_LEFT)
    {
        vx = -WALK_SPEED;
        facingLeft = TRUE;
    }
    else if (joy & BUTTON_RIGHT)
    {
        vx = WALK_SPEED;
        facingLeft = FALSE;
    }
    else
    {
        vx = 0;
    }

    // Jump on A (edge-triggered), only when standing
    if ((joy & BUTTON_A) && !(joyPrev & BUTTON_A) && onGround)
    {
        vy = -JUMP_SPEED;
        onGround = FALSE;
    }

    // Variable jump height: releasing A while rising cuts the jump
    if (!(joy & BUTTON_A) && vy < -0x200)
        vy = -0x200;

    // Gravity
    vy += GRAVITY;
    if (vy > MAX_FALL) vy = MAX_FALL;

    moveX();
    moveY();
    collectCoins();
}

static void drawPlayer(void)
{
    s16 sx = FROM_FP(px) - HB_OFS_X;
    s16 sy = FROM_FP(py);

    // Only touch the sprite table when something visible changed
    if (!spriteDirty && sx == lastSx && sy == lastSy && facingLeft == lastFacing)
        return;

    lastSx = sx;
    lastSy = sy;
    lastFacing = facingLeft;
    spriteDirty = FALSE;

    VDP_setSpriteFull(0, sx, sy, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, facingLeft, TILE_PLAYER),
                      0);
    VDP_updateSprites(1, DMA_QUEUE);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    // Palettes: PAL0 keeps the default grey ramp (white font = color 15)
    PAL_setPalette(PAL1, palGame, CPU);
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x5080C8));   // sky / backdrop

    // Load tiles
    VDP_loadTileData(tileSolid,  TILE_SOLID,  1, CPU);
    VDP_loadTileData(tileCoin,   TILE_COIN,   1, CPU);
    VDP_loadTileData(tilePlayer, TILE_PLAYER, 4, CPU);

    // Planes
    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);
    VDP_setTextPlane(BG_B);         // HUD behind the level plane (A)

    // Game init
    score = 0;
    round = 1;
    loadLevel();
    drawLevel();
    resetPlayer();
    drawScore();

    spriteDirty = TRUE;             // force the first sprite upload

    u16 joyPrev = 0;

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);

        // START: restart everything
        if ((joy & BUTTON_START) && !(joyPrev & BUTTON_START))
        {
            score = 0;
            round = 1;
            loadLevel();
            drawLevel();
            resetPlayer();
            VDP_clearTextArea(0, 1, 40, 1);
            drawScore();
        }

        updatePlayer(joy, joyPrev);
        drawPlayer();

        joyPrev = joy;
        SYS_doVBlankProcess();
    }

    return 0;
}
```

Three details to be aware of:
- **Early return in `moveX`:** it returns immediately when `vx == 0`. This is safe because a stationary player can't have a new horizontal collision.
- **Sprite updates:** skipping the update relies on nothing else writing to the sprite table. That is true for this program.
- **VBlank wait:** the ~3,500 cycles in `SYS_doVBlankProcess` are SGDK waiting for and servicing the vertical blank. Game code can't remove that, and it is idle time, not real load.