/*
 * main.c - One-screen platformer for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 *  - Player: 16x16 hardware sprite (tiles defined in code, no rescomp)
 *  - Level: 40x28 array of solid/empty tiles, drawn on plane A
 *  - Gravity, jump on A, D-pad left/right, tile collision
 *  - Coins to collect, score displayed at the top
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */
#define MAP_W           40
#define MAP_H           28

#define PLAYER_W        16
#define PLAYER_H        16

#define MOVE_SPEED      2           /* pixels per frame                */
#define GRAVITY         64          /* 8.8 fixed: 0.25 px/frame^2      */
#define JUMP_SPEED      1152        /* 8.8 fixed: 4.5 px/frame         */
#define MAX_FALL        (6 * 256)   /* 8.8 fixed: 6 px/frame           */

/* Tile indices in VRAM */
#define TILE_BLOCK      (TILE_USER_INDEX + 0)
#define TILE_COIN       (TILE_USER_INDEX + 1)
#define TILE_PLAYER     (TILE_USER_INDEX + 2)   /* 4 tiles: TL,BL,TR,BR */
#define NUM_TILES       6

/* ------------------------------------------------------------------ */
/* Tile graphics (8x8, 4bpp, one u32 per row)                         */
/* ------------------------------------------------------------------ */
static const u32 tileData[NUM_TILES * 8] =
{
    /* 0: solid block (PAL0 colors 1..3) */
    0x33333333,
    0x32222221,
    0x32222221,
    0x32222221,
    0x32222221,
    0x32222221,
    0x32222221,
    0x11111111,

    /* 1: coin (PAL0 colors 4..5) */
    0x00444400,
    0x04455440,
    0x04544540,
    0x04544540,
    0x04544540,
    0x04544540,
    0x04455440,
    0x00444400,

    /* 2: player top-left (PAL1) */
    0x00001111,
    0x00012222,
    0x00122222,
    0x01222222,
    0x01223322,
    0x01223322,
    0x01222222,
    0x01222222,

    /* 3: player bottom-left */
    0x01222222,
    0x01222211,
    0x01222222,
    0x00122222,
    0x00122100,
    0x00122100,
    0x00111100,
    0x00000000,

    /* 4: player top-right */
    0x11110000,
    0x22221000,
    0x22222100,
    0x22222210,
    0x22332210,
    0x22332210,
    0x22222210,
    0x22222210,

    /* 5: player bottom-right */
    0x22222210,
    0x11222210,
    0x22222210,
    0x22222100,
    0x00122100,
    0x00122100,
    0x00111100,
    0x00000000
};

/* ------------------------------------------------------------------ */
/* Level: 40x28, '#' = solid, '.' = empty (each row is 40 chars)      */
/* ------------------------------------------------------------------ */
static const char level[MAP_H][MAP_W + 1] =
{
    /* 0  */ "........................................",
    /* 1  */ "........................................",
    /* 2  */ "........................................",
    /* 3  */ "........................................",
    /* 4  */ "#......................................#",
    /* 5  */ "#......................................#",
    /* 6  */ "#......................................#",
    /* 7  */ "#......................................#",
    /* 8  */ "#......................................#",
    /* 9  */ "#......................................#",
    /* 10 */ "#...............########................#",
    /* 11 */ "#......................................#",
    /* 12 */ "#......................................#",
    /* 13 */ "#......................................#",
    /* 14 */ "#...######..........##......######...#",
    /* 15 */ "#......................................#",
    /* 16 */ "#......................................#",
    /* 17 */ "#......................................#",
    /* 18 */ "#.........####################........#",
    /* 19 */ "#......................................#",
    /* 20 */ "#......................................#",
    /* 21 */ "#......................................#",
    /* 22 */ "#.....########..........######.....####"
    /* placeholder replaced below */ ,
    /* 23 */ "#......................................#",
    /* 24 */ "#......................................#",
    /* 25 */ "#......................................#",
    /* 26 */ "########################################",
    /* 27 */ "########################################"
};

/* ------------------------------------------------------------------ */
/* Coins                                                              */
/* ------------------------------------------------------------------ */
typedef struct
{
    u8   x;         /* tile coordinates */
    u8   y;
    bool taken;
} Coin;

#define NUM_COINS 10
static Coin coins[NUM_COINS] =
{
    {  9, 21, FALSE }, { 30, 21, FALSE },
    { 19, 17, FALSE }, { 20, 17, FALSE },
    {  7, 13, FALSE }, { 32, 13, FALSE },
    { 19,  9, FALSE }, { 20,  9, FALSE },
    { 12, 25, FALSE }, { 28, 25, FALSE }
};

/* ------------------------------------------------------------------ */
/* Game state                                                         */
/* ------------------------------------------------------------------ */
static s32  posY8;          /* Y in 8.8 fixed point */
static s16  posX;           /* X in pixels          */
static s32  velY8;          /* vertical speed, 8.8  */
static bool onGround;
static u16  score;
static u16  prevPad;

/* ------------------------------------------------------------------ */
/* Collision helpers                                                  */
/* ------------------------------------------------------------------ */
static bool solidAt(s16 tx, s16 ty)
{
    if (tx < 0 || tx >= MAP_W) return TRUE;
    if (ty < 0) return FALSE;
    if (ty >= MAP_H) return TRUE;
    return level[ty][tx] == '#';
}

/* Does a PLAYER_W x PLAYER_H box at pixel (px,py) overlap any solid tile? */
static bool boxHitsSolid(s16 px, s16 py)
{
    s16 tx0 = px >> 3;
    s16 tx1 = (px + PLAYER_W - 1) >> 3;
    s16 ty0 = py >> 3;
    s16 ty1 = (py + PLAYER_H - 1) >> 3;

    for (s16 ty = ty0; ty <= ty1; ty++)
        for (s16 tx = tx0; tx <= tx1; tx++)
            if (solidAt(tx, ty)) return TRUE;

    return FALSE;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                            */
/* ------------------------------------------------------------------ */
static void drawLevel(void)
{
    const u16 blockAttr = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_BLOCK);
    const u16 coinAttr  = TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_COIN);

    for (u16 y = 0; y < MAP_H; y++)
        for (u16 x = 0; x < MAP_W; x++)
            if (level[y][x] == '#')
                VDP_setTileMapXY(BG_A, blockAttr, x, y);

    for (u16 i = 0; i < NUM_COINS; i++)
        VDP_setTileMapXY(BG_A, coinAttr, coins[i].x, coins[i].y);
}

static void drawScore(void)
{
    char str[8];
    uintToStr(score, str, 3);
    VDP_drawText("SCORE", 1, 1);
    VDP_drawText(str, 7, 1);

    if (score >= NUM_COINS)
        VDP_drawText("ALL COINS COLLECTED!", 12, 1);
}

static void updateSprite(void)
{
    const u16 attr = TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER);
    VDP_setSpriteFull(0, posX, (s16)(posY8 >> 8), SPRITE_SIZE(2, 2), attr, 0);
    VDP_updateSprites(1, DMA_QUEUE);
}

/* ------------------------------------------------------------------ */
/* Game logic                                                         */
/* ------------------------------------------------------------------ */
static void collectCoins(void)
{
    const s16 py = (s16)(posY8 >> 8);

    for (u16 i = 0; i < NUM_COINS; i++)
    {
        if (coins[i].taken) continue;

        const s16 cx = coins[i].x * 8;
        const s16 cy = coins[i].y * 8;

        if (posX < cx + 8 && posX + PLAYER_W > cx &&
            py   < cy + 8 && py   + PLAYER_H > cy)
        {
            coins[i].taken = TRUE;
            VDP_setTileMapXY(BG_A, 0, coins[i].x, coins[i].y);
            score++;
            drawScore();
        }
    }
}

static void updatePlayer(void)
{
    const u16 pad = JOY_readJoypad(JOY_1);

    /* ---- Horizontal movement ---- */
    s16 dx = 0;
    if (pad & BUTTON_LEFT)  dx -= MOVE_SPEED;
    if (pad & BUTTON_RIGHT) dx += MOVE_SPEED;

    if (dx != 0)
    {
        const s16 py = (s16)(posY8 >> 8);
        const s16 step = (dx > 0) ? 1 : -1;

        /* move pixel by pixel so we stop flush against walls */
        for (s16 i = 0; i < MOVE_SPEED; i++)
        {
            if (boxHitsSolid(posX + step, py)) break;
            posX += step;
        }
    }

    /* ---- Jump (A, on press only) ---- */
    if ((pad & BUTTON_A) && !(prevPad & BUTTON_A) && onGround)
    {
        velY8 = -JUMP_SPEED;
        onGround = FALSE;
    }
    prevPad = pad;

    /* ---- Gravity ---- */
    velY8 += GRAVITY;
    if (velY8 > MAX_FALL) velY8 = MAX_FALL;

    /* ---- Vertical movement + collision ---- */
    const s32 newY8 = posY8 + velY8;
    const s16 newPy = (s16)(newY8 >> 8);

    if (!boxHitsSolid(posX, newPy))
    {
        posY8 = newY8;
    }
    else
    {
        if (velY8 > 0)
        {
            /* landed: snap feet to top of the tile below */
            const s16 ty = (newPy + PLAYER_H - 1) >> 3;
            posY8 = (s32)((ty * 8) - PLAYER_H) << 8;
        }
        else
        {
            /* bumped head: snap to bottom of the tile above */
            const s16 ty = newPy >> 3;
            posY8 = (s32)((ty + 1) * 8) << 8;
        }
        velY8 = 0;
    }

    /* ---- Ground check (1 pixel below feet) ---- */
    onGround = boxHitsSolid(posX, (s16)(posY8 >> 8) + 1);
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */
int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setPlaneSize(64, 32, TRUE);

    /* Palettes: PAL0 = background/level/text, PAL1 = player */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x101830));  /* backdrop (dark blue) */
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0x205020));  /* block dark           */
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x40A040));  /* block mid            */
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0x90E090));  /* block light          */
    PAL_setColor(4,  RGB24_TO_VDPCOLOR(0xFFE020));  /* coin yellow          */
    PAL_setColor(5,  RGB24_TO_VDPCOLOR(0xE08000));  /* coin orange          */
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));  /* text                 */

    PAL_setColor(16 + 0, RGB24_TO_VDPCOLOR(0x000000));
    PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x600000));  /* outline */
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0xE02020));  /* body    */
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0xFFFFFF));  /* eyes    */

    /* Load tiles */
    VDP_loadTileData(tileData, TILE_USER_INDEX, NUM_TILES, CPU);

    /* Text goes on plane A (rows 0-3 of the level are empty) */
    VDP_setTextPlane(BG_A);
    VDP_setTextPalette(PAL0);

    VDP_clearPlane(BG_A, TRUE);
    VDP_clearPlane(BG_B, TRUE);

    drawLevel();

    /* Init player on the ground, left side */
    posX     = 24;
    posY8    = (s32)(26 * 8 - PLAYER_H) << 8;
    velY8    = 0;
    onGround = TRUE;
    score    = 0;
    prevPad  = 0;

    JOY_init();
    drawScore();
    updateSprite();

    while (TRUE)
    {
        updatePlayer();
        collectCoins();
        updateSprite();

        SYS_doVBlankProcess();
    }

    return 0;
}
