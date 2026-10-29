/*
 * main.c - Game HUD demo for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 *  - Countdown timer (minutes:seconds), starts at 3:00
 *  - Score: +10 every frame while A is held (pad 1)
 *  - Lives: drawn as heart tiles
 *  - Health bar: tile based, shrinks while B is held (pad 1)
 *  - START restarts after GAME OVER / TIME UP
 *
 * No resource files: all tiles are const u32 arrays loaded with VDP_loadTileData.
 */

#include <genesis.h>

/* ------------------------------------------------------------------ */
/* Tiles (4bpp, 8 u32 per 8x8 tile, one nibble per pixel)              */
/* Palette indexes: 0 = transparent, 1 = red, 2 = dark gray, 3 = white */
/* ------------------------------------------------------------------ */

/* Heart icon */
static const u32 heartTile[8] =
{
    0x00000000,
    0x01100110,
    0x11311111,
    0x11111111,
    0x01111110,
    0x00111100,
    0x00011000,
    0x00000000
};

/* Health bar segment: white border on top/bottom, red fill, gray empty part.
   Variant n has n pixels filled (0..8) from the left. */
#define BAR_TILE(p) { 0x33333333, p, p, p, p, p, p, 0x33333333 }

static const u32 barTiles[9][8] =
{
    BAR_TILE(0x22222222),
    BAR_TILE(0x12222222),
    BAR_TILE(0x11222222),
    BAR_TILE(0x11122222),
    BAR_TILE(0x11112222),
    BAR_TILE(0x11111222),
    BAR_TILE(0x11111122),
    BAR_TILE(0x11111112),
    BAR_TILE(0x11111111)
};

/* ------------------------------------------------------------------ */
/* Tile VRAM layout                                                    */
/* ------------------------------------------------------------------ */
#define TILE_HEART      (TILE_USER_INDEX)
#define TILE_BAR_BASE   (TILE_USER_INDEX + 1)   /* 9 tiles: fill 0..8 */

/* ------------------------------------------------------------------ */
/* HUD layout (in tiles, screen is 40x28)                              */
/* ------------------------------------------------------------------ */
#define TIME_X          2
#define TIME_Y          2
#define SCORE_X         22
#define SCORE_Y         2
#define LIVES_X         2
#define LIVES_Y         5
#define LIVES_SLOTS     5
#define HEALTH_X        2
#define HEALTH_Y        8
#define BAR_TILES       10
#define MSG_X           12
#define MSG_Y           13

/* ------------------------------------------------------------------ */
/* Game constants                                                      */
/* ------------------------------------------------------------------ */
#define START_SECONDS   180
#define START_LIVES     3
#define HEALTH_MAX      (BAR_TILES * 8)         /* 8 units per tile */
#define SCORE_STEP      10
#define SCORE_MAX       999999

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
static u32 score;
static u16 secondsLeft;
static u16 frameCount;
static u16 lives;
static u16 health;
static u16 framesPerSecond;
static bool gameOver;
static bool timeUp;

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void drawTime(void)
{
    char buf[8];
    u16 m = secondsLeft / 60;
    u16 s = secondsLeft % 60;

    buf[0] = '0' + (m / 10);
    buf[1] = '0' + (m % 10);
    buf[2] = ':';
    buf[3] = '0' + (s / 10);
    buf[4] = '0' + (s % 10);
    buf[5] = '\0';

    VDP_drawText(buf, TIME_X + 6, TIME_Y);
}

static void drawScore(void)
{
    char buf[12];
    uintToStr(score, buf, 6);   /* zero padded to 6 digits */
    VDP_drawText(buf, SCORE_X + 7, SCORE_Y);
}

static void drawLives(void)
{
    u16 i;
    for (i = 0; i < LIVES_SLOTS; i++)
    {
        u16 tile = (i < lives) ? TILE_HEART : 0;
        VDP_setTileMapXY(BG_A, TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, tile),
                         LIVES_X + 7 + i * 2, LIVES_Y);
    }
}

static void drawHealth(void)
{
    u16 i;
    for (i = 0; i < BAR_TILES; i++)
    {
        s16 fill = (s16)health - (s16)(i * 8);
        if (fill < 0) fill = 0;
        if (fill > 8) fill = 8;
        VDP_setTileMapXY(BG_A,
                         TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_BAR_BASE + fill),
                         HEALTH_X + 7 + i, HEALTH_Y);
    }
}

static void drawMessage(void)
{
    if (gameOver)
        VDP_drawText("  GAME OVER   ", MSG_X, MSG_Y);
    else if (timeUp)
        VDP_drawText("   TIME UP    ", MSG_X, MSG_Y);
    else
        VDP_drawText("              ", MSG_X, MSG_Y);

    if (gameOver || timeUp)
        VDP_drawText(" PRESS START  ", MSG_X, MSG_Y + 2);
    else
        VDP_drawText("              ", MSG_X, MSG_Y + 2);
}

static void drawAll(void)
{
    VDP_drawText("TIME", TIME_X, TIME_Y);
    VDP_drawText("SCORE", SCORE_X, SCORE_Y);
    VDP_drawText("LIVES", LIVES_X, LIVES_Y);
    VDP_drawText("HEALTH", HEALTH_X, HEALTH_Y);
    VDP_drawText("A: SCORE   B: DAMAGE", 10, 22);

    drawTime();
    drawScore();
    drawLives();
    drawHealth();
    drawMessage();
}

/* ------------------------------------------------------------------ */
/* Game logic                                                          */
/* ------------------------------------------------------------------ */
static void resetGame(void)
{
    score = 0;
    secondsLeft = START_SECONDS;
    frameCount = 0;
    lives = START_LIVES;
    health = HEALTH_MAX;
    gameOver = FALSE;
    timeUp = FALSE;
    drawAll();
}

int main(bool hardReset)
{
    u16 pad, prevPad = 0, pressed;
    (void)hardReset;

    /* Video setup: 320x224 (H40, V28) */
    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palette 0: 0 = background, 1 = red, 2 = dark gray, 3 = white, 15 = font */
    PAL_setColor(0,  RGB24_TO_VDPCOLOR(0x101028));
    PAL_setColor(1,  RGB24_TO_VDPCOLOR(0xE02020));
    PAL_setColor(2,  RGB24_TO_VDPCOLOR(0x404050));
    PAL_setColor(3,  RGB24_TO_VDPCOLOR(0xFFFFFF));
    PAL_setColor(15, RGB24_TO_VDPCOLOR(0xFFFFFF));

    VDP_setTextPalette(PAL0);
    VDP_setTextPriority(TRUE);

    /* Load tiles from const arrays in ROM */
    VDP_loadTileData(heartTile, TILE_HEART, 1, CPU);
    VDP_loadTileData((const u32 *)barTiles, TILE_BAR_BASE, 9, CPU);

    framesPerSecond = IS_PAL_SYSTEM ? 50 : 60;

    VDP_clearPlane(BG_A, TRUE);
    resetGame();

    while (TRUE)
    {
        pad = JOY_readJoypad(JOY_1);
        pressed = pad & ~prevPad;
        prevPad = pad;

        if (gameOver || timeUp)
        {
            if (pressed & BUTTON_START)
                resetGame();
        }
        else
        {
            /* Score: +10 every frame while A is held */
            if (pad & BUTTON_A)
            {
                score += SCORE_STEP;
                if (score > SCORE_MAX) score = SCORE_MAX;
                drawScore();
            }

            /* Health: shrinks while B is held */
            if ((pad & BUTTON_B) && health > 0)
            {
                health--;
                if (health == 0)
                {
                    if (lives > 0) lives--;
                    if (lives == 0)
                    {
                        gameOver = TRUE;
                        drawMessage();
                    }
                    else
                    {
                        health = HEALTH_MAX;    /* refill for next life */
                    }
                    drawLives();
                }
                drawHealth();
            }

            /* Countdown timer */
            frameCount++;
            if (frameCount >= framesPerSecond)
            {
                frameCount = 0;
                if (secondsLeft > 0) secondsLeft--;
                drawTime();
                if (secondsLeft == 0 && !gameOver)
                {
                    timeUp = TRUE;
                    drawMessage();
                }
            }
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
