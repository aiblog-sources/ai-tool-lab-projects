#include <genesis.h>

/* ---------- Level / physics constants ---------- */
#define LEVEL_W        40          /* tiles (40 * 8 = 320 px) */
#define LEVEL_H        28          /* tiles (28 * 8 = 224 px) */
#define TILE_PX        8
#define PLAYER_W       16
#define PLAYER_H       16

#define MOVE_SPEED     2           /* px / frame */
#define GRAVITY        1           /* px / frame^2 */
#define MAX_FALL       6           /* px / frame */
#define JUMP_VEL       (-9)        /* px / frame */

#define SCORE_LANDING  10

/* VRAM tile indices (after SGDK's font/system tiles) */
#define TILE_BRICK     TILE_USER_INDEX
#define TILE_PLAYER    (TILE_USER_INDEX + 1)   /* 4 tiles: TL, BL, TR, BR */

/* ---------- Level: '#' = solid tile, '.' = empty tile ---------- */
/* Row 0 is kept empty because the HUD text is drawn there. */
static const char levelRows[LEVEL_H][LEVEL_W + 1] = {
    /* 0  */ ".........." ".........." ".........." ".........."
    /* 1  */ ".........." ".........." ".........." ".........."
    /* 2  */ ".........." ".........." ".........." ".........."
    /* 3  */ ".........." ".........." ".........." ".........."
    /* 4  */ ".........." ".........." ".........." ".........."
    /* 5  */ ".........." ".........." ".........." ".........."
    /* 6  */ ".........." ".........." ".........." ".........."
    /* 7  */ ".........." ".........." ".........." ".........."
    /* 8  */ "....######" "###......." ".........." ".........."
    /* 9  */ ".........." ".........." ".........." ".........."
    /* 10 */ ".........." ".........." ".........." ".........."
    /* 11 */ ".........." ".........." "........##" "#####....."
    /* 12 */ ".........." ".........." ".........." ".........."
    /* 13 */ ".........." "........##" "#######..." ".........."
    /* 14 */ ".........." ".........." ".........." ".........."
    /* 15 */ ".........." ".........." ".........." ".........."
    /* 16 */ ".........." ".........." ".........." ".........."
    /* 17 */ ".........." ".........." ".........." ".........."
    /* 18 */ "..########" ".........." ".........." "########.."
    /* 19 */ ".........." ".........." ".........." ".........."
    /* 20 */ ".........." ".........." ".........." ".........."
    /* 21 */ ".........." ".........." ".........." ".........."
    /* 22 */ ".........." "....####.." ".........." ".........."
    /* 23 */ ".........." ".........." ".........." ".........."
    /* 24 */ "#####....." ".........." ".........." ".........."
    /* 25 */ ".........." ".........." ".........." ".........."
    /* 26 */ ".........." ".........." ".........." ".........."
    /* 27 */ "##########" "##########" "##########" "##########"
};

/* ---------- Tile graphics (4bpp, 8 x u32 per 8x8 tile) ---------- */
/* Brick tile: palette index 2 (gray) with darker edge 3 */
static const u32 tileBrick[8] = {
    0x33333333,
    0x32222222,
    0x32222222,
    0x32222222,
    0x32222222,
    0x32222222,
    0x32222222,
    0x22222221
};

/* Player 16x16 sprite = 4 tiles in column-major order (TL, BL, TR, BR).
   Palette index 4 = player colour, index 0 = transparent. */
static const u32 tilePlayer[32] = {
    /* TL */
    0x04444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    /* BL */
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x04444444,
    /* TR */
    0x44444440, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    /* BR */
    0x44444444, 0x44444444, 0x44444444, 0x44444444,
    0x44444444, 0x44444444, 0x44444444, 0x44444440
};

/* ---------- Palettes ---------- */
static const u16 palBg[16] = {
    0x0000,                       /* 0: black background */
    RGB24_TO_VDPCOLOR(0xFFFFFF),  /* 1: white (text) */
    RGB24_TO_VDPCOLOR(0x888888),  /* 2: brick gray */
    RGB24_TO_VDPCOLOR(0x444444),  /* 3: brick edge */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    RGB24_TO_VDPCOLOR(0xFFFFFF)   /* 15: white (font default index) */
};

static const u16 palPlayer[16] = {
    0x0000,                       /* 0: transparent */
    0, 0, 0,
    RGB24_TO_VDPCOLOR(0xFF6600),  /* 4: player orange */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* ---------- Game state ---------- */
static s16 px, py;      /* player top-left position (px) */
static s16 vx, vy;      /* velocity (px/frame) */
static u8  onGround;
static u16 score;
static u16 prevPad;

/* ---------- Collision ---------- */
static u8 solidAt(s16 tx, s16 ty)
{
    if (tx < 0 || tx >= LEVEL_W || ty < 0 || ty >= LEVEL_H)
        return FALSE;
    return (levelRows[ty][tx] == '#');
}

/* Returns TRUE if the player box at (x, y) overlaps any solid tile
   or is outside the screen bounds. */
static u8 blocked(s16 x, s16 y)
{
    s16 x0, x1, y0, y1, tx, ty;

    if (x < 0 || y < 0 ||
        x + PLAYER_W > LEVEL_W * TILE_PX ||
        y + PLAYER_H > LEVEL_H * TILE_PX)
        return TRUE;

    x0 = x / TILE_PX;
    x1 = (x + PLAYER_W - 1) / TILE_PX;
    y0 = y / TILE_PX;
    y1 = (y + PLAYER_H - 1) / TILE_PX;

    for (ty = y0; ty <= y1; ty++)
        for (tx = x0; tx <= x1; tx++)
            if (solidAt(tx, ty))
                return TRUE;

    return FALSE;
}

/* Move horizontally one pixel at a time to stay flush against walls */
static void moveX(void)
{
    s16 dx = vx;
    while (dx != 0)
    {
        s16 step = (dx > 0) ? 1 : -1;
        if (blocked(px + step, py))
        {
            vx = 0;
            break;
        }
        px += step;
        dx -= step;
    }
}

/* Move vertically one pixel at a time; stop on floors and ceilings */
static void moveY(void)
{
    s16 dy = vy;
    while (dy != 0)
    {
        s16 step = (dy > 0) ? 1 : -1;
        if (blocked(px, py + step))
        {
            vy = 0;
            break;
        }
        py += step;
        dy -= step;
    }
}

/* ---------- Drawing ---------- */
static void drawLevel(void)
{
    s16 x, y;
    for (y = 0; y < LEVEL_H; y++)
    {
        for (x = 0; x < LEVEL_W; x++)
        {
            u16 tile = (levelRows[y][x] == '#') ? TILE_BRICK : 0;
            VDP_setTileMapXY(VDP_BG_A,
                             TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, tile),
                             x, y);
        }
    }
}

static void drawScore(void)
{
    char buf[6];
    u16 v = score;
    s16 i;

    for (i = 4; i >= 0; i--)
    {
        buf[i] = (char)('0' + (v % 10));
        v /= 10;
    }
    buf[5] = '\0';

    VDP_drawText(buf, 7, 0);
}

/* ---------- Entry point ---------- */
int main(bool hardReset)
{
    (void)hardReset;

    JOY_init();
    VDP_setScreenWidth320();

    /* Graphics */
    VDP_loadTileData(tileBrick,  TILE_BRICK,  1, CPU);
    VDP_loadTileData(tilePlayer, TILE_PLAYER, 4, CPU);
    PAL_setPalette(PAL0, palBg,     CPU);
    PAL_setPalette(PAL1, palPlayer, CPU);

    drawLevel();

    /* HUD */
    VDP_drawText("SCORE", 1, 0);
    score = 0;
    drawScore();

    /* Player start: standing on the floor */
    px = 32;
    py = LEVEL_H * TILE_PX - PLAYER_H - 8;   /* 200 */
    vx = 0;
    vy = 0;
    onGround = TRUE;
    prevPad = 0;

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);
        u16 pressed = pad & ~prevPad;
        u8  ground;

        prevPad = pad;

        /* Horizontal input */
        vx = 0;
        if (pad & BUTTON_LEFT)
            vx = -MOVE_SPEED;
        else if (pad & BUTTON_RIGHT)
            vx = MOVE_SPEED;

        /* Jump on A press (only from ground), otherwise apply gravity */
        if ((pressed & BUTTON_A) && onGround)
        {
            vy = JUMP_VEL;
        }
        else
        {
            vy += GRAVITY;
            if (vy > MAX_FALL)
                vy = MAX_FALL;
        }

        /* Move and resolve collisions */
        moveX();
        moveY();

        /* Ground check and landing score */
        ground = blocked(px, py + 1);
        if (ground && !onGround)
        {
            if (score <= 65535 - SCORE_LANDING)
                score += SCORE_LANDING;
            drawScore();
        }
        onGround = ground;

        /* Draw player sprite (2x2 tiles, high priority) */
        VDP_setSprite(0, px, py, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_PLAYER));
        VDP_updateSprites(1, CPU);

        VDP_waitVSync();
    }

    return 0;
}
