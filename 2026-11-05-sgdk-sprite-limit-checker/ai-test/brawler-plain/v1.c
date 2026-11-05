/*
 * main.c - SGDK 2.11 beat-em-up scene (no resource files)
 *
 * - 10 enemies + 1 player, each 32x48 px, built from hardware sprites
 *   (16x16 head + 32x16 torso + 16x16 legs per character = 33 hardware sprites)
 *   The head and legs only use the two middle tile columns, so they are
 *   16 px wide instead of 32 to save sprite pixels per scanline.
 * - Enemies patrol back and forth along the same ground line at different speeds
 * - Player walks with the pad 1 d-pad
 * - All tiles are const u32 arrays loaded with VDP_loadTileData
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Tile data (4bpp, 8 u32 per 8x8 tile, one u32 = one row of 8 pixels) */
/* ------------------------------------------------------------------ */

#define ROW(c) (0x11111111UL * (c))

static const u32 tEmpty[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* colour 1 = skin */
static const u32 tSkin[8] = {
    ROW(1), ROW(1), ROW(1), ROW(1), ROW(1), ROW(1), ROW(1), ROW(1)
};

/* colour 2 = hair */
static const u32 tHair[8] = {
    ROW(2), ROW(2), ROW(2), ROW(2), ROW(2), ROW(2), ROW(2), ROW(2)
};

/* colour 3 = shirt */
static const u32 tShirt[8] = {
    ROW(3), ROW(3), ROW(3), ROW(3), ROW(3), ROW(3), ROW(3), ROW(3)
};

/* colour 4 = pants */
static const u32 tPants[8] = {
    ROW(4), ROW(4), ROW(4), ROW(4), ROW(4), ROW(4), ROW(4), ROW(4)
};

/* colour 5 = shoes */
static const u32 tShoe[8] = {
    ROW(5), ROW(5), ROW(5), ROW(5), ROW(5), ROW(5), ROW(5), ROW(5)
};

/* skin with an eye (colour 6) on the right side, so hflip changes facing */
static const u32 tEye[8] = {
    0x11111111, 0x11111111, 0x11116611, 0x11116611,
    0x11111111, 0x11111111, 0x11111111, 0x11111111
};

/* background tiles (palette 3) */
static const u32 tSky[8] = {
    ROW(1), ROW(1), ROW(1), ROW(1), ROW(1), ROW(1), ROW(1), ROW(1)
};
static const u32 tGroundEdge[8] = {
    ROW(2), ROW(2), ROW(3), ROW(3), ROW(3), ROW(3), ROW(3), ROW(3)
};
static const u32 tGround[8] = {
    ROW(3), ROW(3), ROW(3), ROW(3), ROW(3), ROW(3), ROW(3), 0x33333333
};

/* Tile kinds used by the character layout */
enum { K_EMPTY = 0, K_SKIN, K_HAIR, K_SHIRT, K_PANTS, K_SHOE, K_EYE, K_COUNT };

static const u32 *const kindTiles[K_COUNT] = {
    tEmpty, tSkin, tHair, tShirt, tPants, tShoe, tEye
};

/* 4 columns x 6 rows = 32x48 pixels */
static const u8 layout[6][4] = {
    { K_EMPTY, K_HAIR,  K_HAIR,  K_EMPTY },   /* head top  */
    { K_EMPTY, K_SKIN,  K_EYE,   K_EMPTY },   /* face      */
    { K_SHIRT, K_SHIRT, K_SHIRT, K_SHIRT },   /* shoulders */
    { K_SKIN,  K_SHIRT, K_SHIRT, K_SKIN  },   /* torso/arms*/
    { K_EMPTY, K_PANTS, K_PANTS, K_EMPTY },   /* legs      */
    { K_EMPTY, K_SHOE,  K_SHOE,  K_EMPTY }    /* feet      */
};

/* ------------------------------------------------------------------ */
/* Palettes (0x0BGR)                                                   */
/* ------------------------------------------------------------------ */

static const u16 palPlayer[7] = {
    0x0000, 0x06AE, 0x0024, 0x022E, 0x0A40, 0x0222, 0x0000
};
static const u16 palEnemyA[7] = {
    0x0000, 0x048C, 0x0088, 0x00E2, 0x0644, 0x0222, 0x0000
};
static const u16 palEnemyB[7] = {
    0x0000, 0x068C, 0x0EEE, 0x0E0E, 0x0222, 0x0004, 0x0000
};
static const u16 palBg[4] = {
    0x0000, 0x0E86, 0x0468, 0x0248
};

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

#define CHAR_W          32
#define CHAR_H          48
#define SCREEN_W        320
#define SCREEN_H        224

#define NUM_ENEMIES     10
#define NUM_CHARS       (NUM_ENEMIES + 1)
#define SPR_PER_CHAR    3
#define NUM_HW_SPRITES  (NUM_CHARS * SPR_PER_CHAR)

/* tile offsets inside a character's tile block (16 tiles) */
#define T_HEAD          0                         /* 4 tiles, 16x16 */
#define T_TORSO         4                         /* 8 tiles, 32x16 */
#define T_LEGS          12                        /* 4 tiles, 16x16 */

#define TILE_CHAR       (TILE_USER_INDEX)         /* 16 tiles */
#define TILE_SKY        (TILE_CHAR + 16)
#define TILE_GEDGE      (TILE_SKY + 1)
#define TILE_GROUND     (TILE_GEDGE + 1)

#define GROUND_ROW      21                        /* first ground tile row (y=168) */
#define ENEMY_Y         144                       /* sprite top: feet at y=192 */
#define PLAYER_MIN_Y    120                       /* feet at 168 (ground edge) */
#define PLAYER_MAX_Y    176                       /* feet at 224 (screen bottom) */
#define MAX_X           (SCREEN_W - CHAR_W)

#define FIX             8                         /* enemy positions in 1/8 px */

/* ------------------------------------------------------------------ */
/* Game state                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    s16 x8;       /* x position in 1/8 pixel */
    s16 speed;    /* 1/8 pixel per frame, always positive */
    s16 dir;      /* +1 right, -1 left */
    u16 pal;      /* PAL1 or PAL2 */
} Enemy;

static Enemy enemies[NUM_ENEMIES];

static s16 playerX = 144;
static s16 playerY = 150;
static u8  playerFlip = 0;

/* different speeds for each enemy (1/8 px per frame: 0.5 .. 2.0 px) */
static const s16 enemySpeeds[NUM_ENEMIES] = { 4, 6, 8, 10, 12, 14, 16, 5, 9, 13 };

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void loadGraphics(void)
{
    u16 r, c;

    /* Head sprite (16x16): layout rows 0-1, columns 1-2, column-major.
       Torso sprite (32x16): rows 2-3, columns 0-3, column-major.
       Legs sprite (16x16): rows 4-5, columns 1-2, column-major. */
    for (r = 0; r < 6; r++)
    {
        for (c = 0; c < 4; c++)
        {
            u16 idx;

            if (r < 2)
            {
                if (c < 1 || c > 2) continue;       /* empty columns not drawn */
                idx = T_HEAD + (c - 1) * 2 + r;
            }
            else if (r < 4)
            {
                idx = T_TORSO + c * 2 + (r - 2);
            }
            else
            {
                if (c < 1 || c > 2) continue;
                idx = T_LEGS + (c - 1) * 2 + (r - 4);
            }
            VDP_loadTileData(kindTiles[layout[r][c]], TILE_CHAR + idx, 1, CPU);
        }
    }

    /* Background tiles */
    VDP_loadTileData(tSky,        TILE_SKY,    1, CPU);
    VDP_loadTileData(tGroundEdge, TILE_GEDGE,  1, CPU);
    VDP_loadTileData(tGround,     TILE_GROUND, 1, CPU);

    /* Palettes */
    PAL_setColors(PAL0 * 16, palPlayer, 7, CPU);
    PAL_setColors(PAL1 * 16, palEnemyA, 7, CPU);
    PAL_setColors(PAL2 * 16, palEnemyB, 7, CPU);
    PAL_setColors(PAL3 * 16, palBg,     4, CPU);
}

static void drawBackground(void)
{
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_SKY),
                        0, 0, 40, GROUND_ROW);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_GEDGE),
                        0, GROUND_ROW, 40, 1);
    VDP_fillTileMapRect(BG_B, TILE_ATTR_FULL(PAL3, 0, 0, 0, TILE_GROUND),
                        0, GROUND_ROW + 1, 40, 28 - (GROUND_ROW + 1));
}

/* Place one 32x48 character using three hardware sprites.
   'slot' is the character slot (0 = player). */
static void setCharacter(u16 slot, s16 x, s16 y, u16 pal, u8 hflip)
{
    u16 i = slot * SPR_PER_CHAR;
    u16 nextLink = (slot + 1 < NUM_CHARS) ? (i + SPR_PER_CHAR) : 0;

    /* head: centred 16 px, symmetric so hflip keeps the same box */
    VDP_setSpriteFull(i, x + 8, y, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(pal, 0, 0, hflip, TILE_CHAR + T_HEAD), i + 1);
    /* torso: full 32 px */
    VDP_setSpriteFull(i + 1, x, y + 16, SPRITE_SIZE(4, 2),
                      TILE_ATTR_FULL(pal, 0, 0, hflip, TILE_CHAR + T_TORSO), i + 2);
    /* legs and feet: centred 16 px */
    VDP_setSpriteFull(i + 2, x + 8, y + 32, SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(pal, 0, 0, hflip, TILE_CHAR + T_LEGS), nextLink);
}

static void initEnemies(void)
{
    u16 i;
    for (i = 0; i < NUM_ENEMIES; i++)
    {
        enemies[i].x8    = (s16)((i * 28) * FIX);
        enemies[i].speed = enemySpeeds[i];
        enemies[i].dir   = (i & 1) ? -1 : 1;
        enemies[i].pal   = (i & 1) ? PAL2 : PAL1;
    }
}

static void updateEnemies(void)
{
    u16 i;
    for (i = 0; i < NUM_ENEMIES; i++)
    {
        Enemy *e = &enemies[i];

        e->x8 += e->dir * e->speed;

        if (e->x8 <= 0)
        {
            e->x8 = 0;
            e->dir = 1;
        }
        else if (e->x8 >= MAX_X * FIX)
        {
            e->x8 = MAX_X * FIX;
            e->dir = -1;
        }
    }
}

static void updatePlayer(void)
{
    u16 j = JOY_readJoypad(JOY_1);

    if (j & BUTTON_LEFT)
    {
        playerX -= 2;
        playerFlip = 1;
    }
    if (j & BUTTON_RIGHT)
    {
        playerX += 2;
        playerFlip = 0;
    }
    if (j & BUTTON_UP)    playerY -= 1;
    if (j & BUTTON_DOWN)  playerY += 1;

    if (playerX < 0)       playerX = 0;
    if (playerX > MAX_X)   playerX = MAX_X;
    if (playerY < PLAYER_MIN_Y) playerY = PLAYER_MIN_Y;
    if (playerY > PLAYER_MAX_Y) playerY = PLAYER_MAX_Y;
}

static void drawSprites(void)
{
    u16 i;

    /* player first: lowest sprite index = highest priority, never dropped */
    setCharacter(0, playerX, playerY, PAL0, playerFlip);

    for (i = 0; i < NUM_ENEMIES; i++)
    {
        const Enemy *e = &enemies[i];
        setCharacter(i + 1, e->x8 / FIX, ENEMY_Y, e->pal, (e->dir < 0) ? 1 : 0);
    }

    VDP_updateSprites(NUM_HW_SPRITES, DMA_QUEUE);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();
    VDP_setPlaneSize(64, 32, TRUE);

    loadGraphics();
    drawBackground();

    VDP_resetSprites();
    initEnemies();

    while (TRUE)
    {
        updatePlayer();
        updateEnemies();
        drawSprites();

        SYS_doVBlankProcess();
    }

    return 0;
}
