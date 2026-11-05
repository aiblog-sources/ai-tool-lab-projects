/*
 * main.c - 30-segment snake, SGDK 2.11, Sega Mega Drive / Genesis
 *
 * No resource files: tiles are const u32 arrays loaded with VDP_loadTileData.
 *
 * How it works
 * - The head moves 2 px/frame on a 16x16 cell grid, so a cell takes 8 frames.
 *   Segment k is drawn exactly where segment k-1 was 8 frames earlier.
 * - Steering is the d-pad on pad 1. Turns are applied at cell boundaries and
 *   the latest direction pressed is remembered. The snake starts with its head
 *   on the middle row, moving right.
 *
 * Why the sprite limit is respected
 * - The VDP shows at most 20 sprites (320 px) per scanline in H40 mode, and
 *   anything beyond that is dropped. 30 sprites of 16 px could exceed it.
 * - The playfield is 19 columns x 14 rows. The snake can never enter a cell
 *   its body occupies, so segments never stack.
 * - Before each step, the code counts how many sprites would touch every
 *   16-px row band during that step, including sprites sliding between rows.
 *   A step that would put more than 20 on one band is rejected and the next
 *   direction is tried.
 * - If no direction is possible (boxed in), the snake restarts.
 * - Sprites are linked 0..29 in order with the last link set to 0. Only 30 of
 *   the 80 hardware sprites are used.
 */

#include <genesis.h>

#define NSEG            30
#define COLS            19      /* 19*16 = 304 px, centred with an 8 px margin */
#define ROWS            14      /* 14*16 = 224 px                              */
#define CELL            16
#define FIELD_X         8
#define MIDDLE_ROW      7
#define STEP_FRAMES     8       /* frames per cell = delay between segments    */
#define PX_PER_FRAME    2       /* 8 frames * 2 px = 16 px = one cell          */
#define MAX_PER_LINE    20      /* H40 hardware limit, sprites per scanline    */

#define BODY_TILE       (TILE_USER_INDEX)
#define HEAD_TILE       (TILE_USER_INDEX + 4)

/* Colour indices: 0 transparent, 1 outline, 2 body, 3 highlight, 4 white, 5 black */
static const u16 snakePal[6] =
{
    0x0000, 0x0040, 0x00A4, 0x04E8, 0x0EEE, 0x0000
};

/* Two 16x16 sprites, 4 tiles each, in the VDP's column-major order:
   top-left, bottom-left, top-right, bottom-right. */
static const u32 snakeTiles[64] =
{
    /* ---- body ---- */
    /* top-left */
    0x00011111, 0x00122222, 0x01233222, 0x12332222,
    0x12322222, 0x12222222, 0x12222222, 0x12222222,
    /* bottom-left */
    0x12222222, 0x12222222, 0x12222222, 0x12222222,
    0x12222222, 0x01222222, 0x00122222, 0x00011111,
    /* top-right */
    0x11111000, 0x22222100, 0x22222210, 0x22222221,
    0x22222221, 0x22222221, 0x22222221, 0x22222221,
    /* bottom-right */
    0x22222221, 0x22222221, 0x22222221, 0x22222221,
    0x22222221, 0x22222210, 0x22222100, 0x11111000,

    /* ---- head (with eyes) ---- */
    /* top-left */
    0x00011111, 0x00122222, 0x01233222, 0x12332222,
    0x12322222, 0x12244442, 0x12245542, 0x12245542,
    /* bottom-left */
    0x12244442, 0x12222222, 0x12222222, 0x12222222,
    0x12222222, 0x01222222, 0x00122222, 0x00011111,
    /* top-right */
    0x11111000, 0x22222100, 0x22222210, 0x22222221,
    0x22222221, 0x24444221, 0x24554221, 0x24554221,
    /* bottom-right */
    0x24444221, 0x22222221, 0x22222221, 0x22222221,
    0x22222221, 0x22222210, 0x22222100, 0x11111000
};

/* Cell the segment stands in at the start of the current step. Index 0 = head. */
static s16 cellX[NSEG];
static s16 cellY[NSEG];
static s16 nextX, nextY;        /* cell the head is moving into               */
static s16 dirX, dirY;          /* current direction                          */
static s16 wantX, wantY;        /* latest direction requested on the d-pad    */
static u16 phase;               /* 0..STEP_FRAMES within the current step     */

static void resetSnake(void)
{
    u16 k;

    /* Head on the middle row heading right; the body trails left along the
       middle row, then folds back along the row above. All 30 segments are
       on screen, one per cell. */
    for (k = 0; k <= 10; k++)
    {
        cellX[k] = 10 - k;
        cellY[k] = MIDDLE_ROW;
    }
    for (k = 11; k < NSEG; k++)
    {
        cellX[k] = k - 11;
        cellY[k] = MIDDLE_ROW - 1;
    }

    dirX = 1;
    dirY = 0;
    wantX = 0;
    wantY = 0;
    phase = 0;
}

static bool cellOccupied(s16 x, s16 y)
{
    u16 k;
    /* The tail cell is vacated during the step, so it is not checked. */
    for (k = 0; k < NSEG - 1; k++)
    {
        if (cellX[k] == x && cellY[k] == y) return TRUE;
    }
    return FALSE;
}

/* Worst-case sprites touching each 16-px row band during a step into (nx,ny).
   A sprite moving between two rows counts for both. */
static bool lineLimitOk(s16 ny)
{
    u8 cnt[ROWS];
    u16 k;

    for (k = 0; k < ROWS; k++) cnt[k] = 0;

    for (k = 0; k < NSEG; k++)
    {
        s16 ay = cellY[k];
        s16 by = (k == 0) ? ny : cellY[k - 1];
        cnt[ay]++;
        if (by != ay) cnt[by]++;
    }
    for (k = 0; k < ROWS; k++)
    {
        if (cnt[k] > MAX_PER_LINE) return FALSE;
    }
    return TRUE;
}

static bool chooseStep(void)
{
    s16 cx[4], cy[4];
    u16 n = 0, i;

    if (wantX || wantY) { cx[n] = wantX; cy[n] = wantY; n++; }
    cx[n] = dirX;  cy[n] = dirY;  n++;      /* keep going straight  */
    cx[n] = -dirY; cy[n] = dirX;  n++;      /* auto-steer, one side */
    cx[n] = dirY;  cy[n] = -dirX; n++;      /* auto-steer, other    */

    for (i = 0; i < n; i++)
    {
        s16 nx = cellX[0] + cx[i];
        s16 ny = cellY[0] + cy[i];

        if (nx < 0 || nx >= COLS || ny < 0 || ny >= ROWS) continue;
        if (cellOccupied(nx, ny)) continue;     /* also rules out reversing */
        if (!lineLimitOk(ny)) continue;

        dirX = cx[i];
        dirY = cy[i];
        nextX = nx;
        nextY = ny;
        wantX = 0;
        wantY = 0;
        return TRUE;
    }
    return FALSE;
}

static void readInput(void)
{
    u16 pad = JOY_readJoypad(JOY_1);
    s16 wx = 0, wy = 0;

    if (pad & BUTTON_LEFT)       wx = -1;
    else if (pad & BUTTON_RIGHT) wx = 1;
    if (pad & BUTTON_UP)         wy = -1;
    else if (pad & BUTTON_DOWN)  wy = 1;

    if (wx && wy)
    {
        /* Diagonal: pick the axis perpendicular to the current motion. */
        if (dirX != 0) wx = 0; else wy = 0;
    }
    if (wx || wy)
    {
        wantX = wx;
        wantY = wy;
    }
}

static void drawSnake(void)
{
    u16 k;
    s16 prog = (s16)(phase * PX_PER_FRAME);     /* 2..16 px into the step */

    for (k = 0; k < NSEG; k++)
    {
        s16 ax = cellX[k];
        s16 ay = cellY[k];
        s16 bx = (k == 0) ? nextX : cellX[k - 1];
        s16 by = (k == 0) ? nextY : cellY[k - 1];
        s16 x = FIELD_X + ax * CELL + (bx - ax) * prog;
        s16 y = ay * CELL + (by - ay) * prog;

        /* Low-level VDP sprite API: coordinates carry the 0x80 hardware offset. */
        VDP_setSpriteFull(k,
                          x + 0x80,
                          y + 0x80,
                          SPRITE_SIZE(2, 2),
                          TILE_ATTR_FULL(PAL1, FALSE, FALSE, FALSE,
                                         (k == 0) ? HEAD_TILE : BODY_TILE),
                          (k + 1 < NSEG) ? (k + 1) : 0);   /* last link = 0 */
    }
    VDP_updateSprites(NSEG, DMA_QUEUE);
}

static void advanceCells(void)
{
    u16 k;
    for (k = NSEG - 1; k > 0; k--)
    {
        cellX[k] = cellX[k - 1];
        cellY[k] = cellY[k - 1];
    }
    cellX[0] = nextX;
    cellY[0] = nextY;
}

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    PAL_setColor(0, 0x0200);
    PAL_setColors(16, snakePal, 6, CPU);        /* PAL1 */
    VDP_loadTileData(snakeTiles, TILE_USER_INDEX, 8, CPU);
    VDP_resetSprites();

    resetSnake();

    while (TRUE)
    {
        readInput();

        if (phase == 0)
        {
            if (!chooseStep())
            {
                /* Boxed in: restart. */
                resetSnake();
                chooseStep();
            }
        }

        phase++;
        drawSnake();

        if (phase == STEP_FRAMES)
        {
            advanceCells();
            phase = 0;
        }

        SYS_doVBlankProcess();
    }

    return 0;
}
