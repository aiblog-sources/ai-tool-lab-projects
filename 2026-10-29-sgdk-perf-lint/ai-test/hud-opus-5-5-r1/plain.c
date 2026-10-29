/*
 * main.c - Game HUD demo for Sega Mega Drive / Genesis (SGDK 2.09)
 *
 * Countdown timer in minutes:seconds
 * Score that increases by 10 every frame while A is held
 * Lives counter
 * Tile-based health bar that shrinks while B is held
 *
 * When health reaches 0, a life is lost and the bar refills.
 * When the timer or lives run out, press START to restart.
 *
 * No resource files. All tiles are const u32 arrays loaded with VDP_loadTileData.
 *
 * Performance notes (68000 has no 32-bit divide):
 *  - Score, timer and lives are kept as decimal digit arrays and updated
 *    with carry/borrow. No uintToStr and no divisions at runtime.
 *  - Only the digit tiles that actually changed are written to VRAM
 *    (usually 1 tile per frame for the score). This replaces a full
 *    VDP_drawText call.
 *  - The bar tile attributes and the text tile base are computed once.
 */

#include <genesis.h>

/* Layout (40x28 tile screen at 320x224) */
#define SCORE_X         2
#define SCORE_Y         2
#define TIME_X          29
#define TIME_Y          2
#define HP_LABEL_X      2
#define BAR_X           5
#define BAR_Y           4
#define LIVES_X         29
#define LIVES_Y         4
#define MSG_Y           13

/* Value positions (same places the original drawText calls used) */
#define SCORE_VAL_X     (SCORE_X + 6)
#define TIME_VAL_X      (TIME_X + 5)
#define LIVES_VAL_X     (LIVES_X + 6)

/* Health bar settings */
#define BAR_TILES       16
#define HEALTH_MAX      (BAR_TILES * 8)     /* 8 health units per tile = 1 per pixel */
#define DRAIN_DELAY     2                   /* frames per health unit while B held */

/* Game settings */
#define START_LIVES     3
#define START_TIME_SEC  (3 * 60)            /* 3:00 */
#define SCORE_DIGITS    8                   /* max 99999990 */

/* Tile VRAM location */
#define TILE_BAR_BASE   TILE_USER_INDEX

/*
 * Health bar tiles: index n = n pixels filled (0..8).
 * Color 1 = fill, color 2 = empty background, color 3 = border.
 * Each u32 is one row of 8 pixels, 4 bits per pixel, leftmost pixel in the high nibble.
 */
#define BORDER_ROW  0x33333333
#define TILE_ROWS(r) BORDER_ROW, r, r, r, r, r, r, BORDER_ROW

static const u32 barTiles[9 * 8] =
{
    TILE_ROWS(0x22222222),   /* 0/8 */
    TILE_ROWS(0x12222222),   /* 1/8 */
    TILE_ROWS(0x11222222),   /* 2/8 */
    TILE_ROWS(0x11122222),   /* 3/8 */
    TILE_ROWS(0x11112222),   /* 4/8 */
    TILE_ROWS(0x11111222),   /* 5/8 */
    TILE_ROWS(0x11111122),   /* 6/8 */
    TILE_ROWS(0x11111112),   /* 7/8 */
    TILE_ROWS(0x11111111)    /* 8/8 */
};

#define BAR_ATTR(fill)  TILE_ATTR_FULL(PAL1, TRUE, FALSE, FALSE, TILE_BAR_BASE + (fill))

/* Game state */
typedef enum
{
    STATE_RUNNING,
    STATE_TIME_UP,
    STATE_GAME_OVER
} GameState;

static GameState state;
static u16 lives;
static u16 health;
static u16 timeLeft;        /* seconds */
static u16 frameCounter;
static u16 drainCounter;
static u16 fps;
static s16 barColorLevel;   /* -1 = not set yet */

/* Decimal digit storage (values 0..9) */
static u8 scoreDig[SCORE_DIGITS];   /* most significant first, last is always 0 */
static bool scoreMaxed;
static u8 timeDig[4];               /* m tens, m ones, s tens, s ones */

/* Tile attribute of the '0' glyph in the current text palette/priority */
static u16 digitBase;

/* Lowest digit index that changed. Larger than the last index = clean. */
static u16 scoreDirtyFrom;
static u16 timeDirtyFrom;
static bool livesDirty;

/* x offsets of the time digits inside "MM:SS" */
static const u8 timeDigX[4] = { 0, 1, 3, 4 };

/* ------------------------------------------------------------------ */

static inline void drawDigit(u16 d, u16 x, u16 y)
{
    VDP_setTileMapXY(BG_A, digitBase + d, x, y);
}

static void drawBarTile(u16 i)
{
    s16 fill = (s16)health - (s16)(i << 3);
    if (fill < 0) fill = 0;
    else if (fill > 8) fill = 8;

    VDP_setTileMapXY(BG_A, BAR_ATTR(fill), BAR_X + i, BAR_Y);
}

static void drawFullBar(void)
{
    for (u16 i = 0; i < BAR_TILES; i++)
        VDP_setTileMapXY(BG_A, BAR_ATTR(8), BAR_X + i, BAR_Y);
}

/* Bar fill color: green > 50%, yellow > 25%, red otherwise. Only touches CRAM on change. */
static void updateBarColor(void)
{
    s16 level;

    if (health > HEALTH_MAX / 2)      level = 2;
    else if (health > HEALTH_MAX / 4) level = 1;
    else                              level = 0;

    if (level == barColorLevel) return;
    barColorLevel = level;

    switch (level)
    {
        case 2:  PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0x00E000)); break;
        case 1:  PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0xE0E000)); break;
        default: PAL_setColor(16 + 1, RGB24_TO_VDPCOLOR(0xE00000)); break;
    }
}

static void drawScore(void)
{
    for (u16 i = scoreDirtyFrom; i < SCORE_DIGITS; i++)
        drawDigit(scoreDig[i], SCORE_VAL_X + i, SCORE_Y);
}

static void drawTime(void)
{
    for (u16 i = timeDirtyFrom; i < 4; i++)
        drawDigit(timeDig[i], TIME_VAL_X + timeDigX[i], TIME_Y);
}

static void drawLives(void)
{
    /* lives <= 3, so the tens digit is always 0 (drawn on reset) */
    drawDigit(lives, LIVES_VAL_X + 1, LIVES_Y);
}

static void drawStaticHud(void)
{
    VDP_drawText("SCORE", SCORE_X, SCORE_Y);
    VDP_drawText("TIME", TIME_X, TIME_Y);
    VDP_drawText("HP", HP_LABEL_X, BAR_Y);
    VDP_drawText("LIVES", LIVES_X, LIVES_Y);
    VDP_drawText("HOLD A: SCORE   HOLD B: DAMAGE", 5, 25);
    VDP_drawText(":", TIME_VAL_X + 2, TIME_Y);
    VDP_drawText("0", LIVES_VAL_X, LIVES_Y);
}

static void clearMessage(void)
{
    VDP_clearText(0, MSG_Y, 40);
    VDP_clearText(0, MSG_Y + 2, 40);
}

static void showMessage(const char *msg)
{
    u16 len = strlen(msg);
    VDP_drawText(msg, (40 - len) / 2, MSG_Y);
    VDP_drawText("PRESS START", (40 - 11) / 2, MSG_Y + 2);
}

/* score += 10, capped at 99999990 */
static void addScore10(void)
{
    s16 i = SCORE_DIGITS - 2;

    while (scoreDig[i] == 9)
    {
        scoreDig[i] = 0;
        i--;
    }
    scoreDig[i]++;

    if ((u16)i < scoreDirtyFrom) scoreDirtyFrom = i;

    /* Check for the cap only when the tens digit just became 9 */
    if (i == SCORE_DIGITS - 2 && scoreDig[i] == 9)
    {
        bool all9 = TRUE;
        for (u16 j = 0; j < SCORE_DIGITS - 2; j++)
        {
            if (scoreDig[j] != 9) { all9 = FALSE; break; }
        }
        scoreMaxed = all9;
    }
}

/* Countdown of the MM:SS digits by one second (caller ensures time > 0) */
static void decTimeDigits(void)
{
    u16 i = 3;

    if (timeDig[3]-- == 0)
    {
        timeDig[3] = 9;
        i = 2;
        if (timeDig[2]-- == 0)
        {
            timeDig[2] = 5;
            i = 1;
            if (timeDig[1]-- == 0)
            {
                timeDig[1] = 9;
                i = 0;
                timeDig[0]--;
            }
        }
    }

    if (i < timeDirtyFrom) timeDirtyFrom = i;
}

static void resetGame(void)
{
    state = STATE_RUNNING;
    lives = START_LIVES;
    health = HEALTH_MAX;
    timeLeft = START_TIME_SEC;
    frameCounter = 0;
    drainCounter = 0;

    memset(scoreDig, 0, sizeof(scoreDig));
    scoreMaxed = FALSE;

    timeDig[0] = (START_TIME_SEC / 60) / 10;
    timeDig[1] = (START_TIME_SEC / 60) % 10;
    timeDig[2] = (START_TIME_SEC % 60) / 10;
    timeDig[3] = (START_TIME_SEC % 60) % 10;

    clearMessage();
    drawFullBar();
    updateBarColor();

    scoreDirtyFrom = 0;
    timeDirtyFrom = 0;
    livesDirty = TRUE;
}

/* ------------------------------------------------------------------ */

int main(bool hardReset)
{
    (void)hardReset;

    u16 prevJoy = 0;

    VDP_setScreenWidth320();
    fps = SYS_isPAL() ? 50 : 60;

    /* Same tile attribute VDP_drawText would use for '0' */
    digitBase = TILE_ATTR_FULL(VDP_getTextPalette(), VDP_getTextPriority(), FALSE, FALSE,
                               TILE_FONT_INDEX + ('0' - 32));

    /* Load health bar tiles */
    VDP_loadTileData(barTiles, TILE_BAR_BASE, 9, DMA);

    /* PAL0 keeps the default font color (index 15 = white).
       PAL1 is for the health bar. */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));               /* backdrop */
    PAL_setColor(16 + 2, RGB24_TO_VDPCOLOR(0x202040));          /* empty part */
    PAL_setColor(16 + 3, RGB24_TO_VDPCOLOR(0xC0C0C0));          /* border */

    barColorLevel = -1;
    drawStaticHud();
    resetGame();

    while (TRUE)
    {
        u16 joy = JOY_readJoypad(JOY_1);
        u16 pressed = joy & ~prevJoy;
        prevJoy = joy;

        if (state == STATE_RUNNING)
        {
            /* Score: +10 per frame while A held */
            if ((joy & BUTTON_A) && !scoreMaxed)
                addScore10();

            /* Health: shrink while B held */
            if (joy & BUTTON_B)
            {
                if (++drainCounter >= DRAIN_DELAY)
                {
                    drainCounter = 0;

                    if (health > 0)
                    {
                        health--;
                        drawBarTile(health >> 3);   /* only the tile that changed */
                        updateBarColor();

                        if (health == 0)
                        {
                            lives--;
                            livesDirty = TRUE;

                            if (lives == 0)
                            {
                                state = STATE_GAME_OVER;
                                showMessage("GAME OVER");
                            }
                            else
                            {
                                health = HEALTH_MAX;
                                drawFullBar();
                                updateBarColor();
                            }
                        }
                    }
                }
            }
            else
            {
                drainCounter = 0;
            }

            /* Countdown timer */
            if (state == STATE_RUNNING && ++frameCounter >= fps)
            {
                frameCounter = 0;
                if (timeLeft > 0)
                {
                    timeLeft--;
                    decTimeDigits();

                    if (timeLeft == 0)
                    {
                        state = STATE_TIME_UP;
                        showMessage("TIME UP!");
                    }
                }
            }
        }
        else if (pressed & BUTTON_START)
        {
            resetGame();
        }

        /* Redraw only what changed */
        if (scoreDirtyFrom < SCORE_DIGITS) { drawScore(); scoreDirtyFrom = SCORE_DIGITS; }
        if (timeDirtyFrom < 4)             { drawTime();  timeDirtyFrom = 4; }
        if (livesDirty)                    { drawLives(); livesDirty = FALSE; }

        SYS_doVBlankProcess();
    }

    return 0;
}
