// main.c - SGDK 2.11 shoot-em-up scene for the Sega Mega Drive / Genesis
//
// - 3 rows x 12 enemy ships, 24x24 px each (one 3x3-tile hardware sprite per ship)
// - Formation marches left/right as a block
// - Player ship moves along the bottom with the d-pad (pad 1); A/B/C fires one bullet
// - No resource files: every tile is a const u32 array loaded with VDP_loadTileData
//
// Hardware sprite budget (H40 / 320 px mode):
//   limits: 80 sprites total, 20 sprites per scanline, 320 sprite pixels per scanline
//   - Total sprites: 36 enemies + 1 player + 1 bullet = 38 (limit 80)
//   - Rows are 32 px apart and the ships are 24 px tall, so two enemy rows never share
//     a scanline. Worst case is 12 enemies + 1 bullet = 13 sprites and
//     12*24 + 8 = 296 px per line (limits 20 / 320), so nothing drops out or flickers.
//   - The sprite table is written directly and sent by DMA once per frame in VBlank.

#include <genesis.h>

// ---------------------------------------------------------------- configuration
#define ROWS            3
#define COLS            12
#define NUM_ENEMIES     (ROWS * COLS)

#define SHIP_SIZE       24
#define PITCH_X         25                      // 24 px ship + 1 px gap
#define PITCH_Y         32                      // > 24, so rows never share a scanline
#define FORM_TOP        24
#define FORM_WIDTH      ((COLS - 1) * PITCH_X + SHIP_SIZE)   // 299
#define FORM_MAX_OFFS   (320 - FORM_WIDTH)                   // 21 px of travel
#define FORM_STEP_FRAMES 2                      // move 1 px every 2 frames

#define PLAYER_Y        192
#define PLAYER_SPEED    3
#define PLAYER_MAX_X    (320 - SHIP_SIZE)

#define BULLET_W        8
#define BULLET_H        16
#define BULLET_SPEED    5

// sprite table layout
#define SPR_ENEMY0      0                       // 0..35
#define SPR_PLAYER      (NUM_ENEMIES)           // 36
#define SPR_BULLET      (NUM_ENEMIES + 1)       // 37
#define SPRITE_COUNT    (NUM_ENEMIES + 2)       // 38

// VRAM tile layout (first free tile after the system font)
#define TILE_ENEMY      (TILE_USER_INDEX)           // 9 tiles
#define TILE_PLAYER     (TILE_USER_INDEX + 9)       // 9 tiles
#define TILE_BULLET     (TILE_USER_INDEX + 18)      // 2 tiles

// ---------------------------------------------------------------- tile data
// 4bpp, one u32 per 8-pixel row, one nibble per pixel (0 = transparent).
// Hardware sprites are stored column-major: tile index = column * height + row.

// Enemy ship, 24x24 (3x3 tiles). Colours: 1 outline, 2 body, 3 highlight, 4 cockpit
static const u32 enemyTiles[9 * 8] =
{
    // column 0, tile row 0
    0x00000000, 0x00000000, 0x00000011, 0x00000112,
    0x00001122, 0x00011222, 0x00112222, 0x01122224,
    // column 0, tile row 1
    0x11222224, 0x11222222, 0x12222222, 0x12233222,
    0x12233222, 0x12222222, 0x11222222, 0x01112222,
    // column 0, tile row 2
    0x00111222, 0x00110112, 0x01100111, 0x01100001,
    0x01000000, 0x01000000, 0x00000000, 0x00000000,

    // column 1, tile row 0
    0x00000000, 0x11111111, 0x22222222, 0x23333332,
    0x33333333, 0x22222222, 0x44222244, 0x44422444,
    // column 1, tile row 1
    0x44422444, 0x22222222, 0x22222222, 0x22222222,
    0x22222222, 0x22222222, 0x22222222, 0x22222222,
    // column 1, tile row 2
    0x22111122, 0x22111122, 0x22111122, 0x11111111,
    0x01111110, 0x00000000, 0x00000000, 0x00000000,

    // column 2, tile row 0
    0x00000000, 0x00000000, 0x11000000, 0x21100000,
    0x22110000, 0x22211000, 0x22221100, 0x42222110,
    // column 2, tile row 1
    0x42222211, 0x22222211, 0x22222221, 0x22233221,
    0x22233221, 0x22222221, 0x22222211, 0x22221110,
    // column 2, tile row 2
    0x22211100, 0x21101100, 0x11100110, 0x10000110,
    0x00000010, 0x00000010, 0x00000000, 0x00000000
};

// Player ship, 24x24 (3x3 tiles). Colours: 1 outline, 2 body, 3 highlight, 4 accent
static const u32 playerTiles[9 * 8] =
{
    // column 0, tile row 0
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000001, 0x00000001, 0x00000012,
    // column 0, tile row 1
    0x00000012, 0x00000112, 0x00001122, 0x00011222,
    0x00112222, 0x01122222, 0x11222222, 0x12222222,
    // column 0, tile row 2
    0x12233222, 0x12233222, 0x11222211, 0x01122110,
    0x00111100, 0x00000000, 0x00000000, 0x00000000,

    // column 1, tile row 0
    0x00111100, 0x01211210, 0x01211210, 0x12211221,
    0x13211231, 0x23211232, 0x23211232, 0x22211222,
    // column 1, tile row 1
    0x24211242, 0x24211242, 0x22211222, 0x22211222,
    0x22211222, 0x22211222, 0x22222222, 0x22222222,
    // column 1, tile row 2
    0x22222222, 0x22222222, 0x22222222, 0x12222221,
    0x11222211, 0x00111100, 0x00000000, 0x00000000,

    // column 2, tile row 0
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x10000000, 0x10000000, 0x21000000,
    // column 2, tile row 1
    0x21000000, 0x21100000, 0x22110000, 0x22211000,
    0x22221100, 0x22222110, 0x22222211, 0x22222221,
    // column 2, tile row 2
    0x22233221, 0x22233221, 0x11222211, 0x01122110,
    0x00111100, 0x00000000, 0x00000000, 0x00000000
};

// Bullet, 8x16 (1x2 tiles)
static const u32 bulletTiles[2 * 8] =
{
    0x00022000, 0x00033000, 0x00033000, 0x00033000,
    0x00033000, 0x00033000, 0x00033000, 0x00033000,

    0x00033000, 0x00033000, 0x00033000, 0x00033000,
    0x00033000, 0x00033000, 0x00022000, 0x00000000
};

// ---------------------------------------------------------------- state
static u8  alive[NUM_ENEMIES];
static u16 aliveCount;

static s16 formOffs;        // 0..FORM_MAX_OFFS
static s16 formDir;         // +1 / -1
static u8  formTimer;

static s16 playerX;

static bool bulletActive;
static s16  bulletX, bulletY;

static u16 prevPad;

// ---------------------------------------------------------------- sprite helpers
// Writes straight into the SGDK sprite cache. Screen coordinates are converted to
// hardware coordinates here (+128). Link order is simply index order.
static void putSprite(u16 i, s16 x, s16 y, u8 size, u16 attr)
{
    u8 link = (i + 1 < SPRITE_COUNT) ? (u8)(i + 1) : 0;

    vdpSpriteCache[i].y = y + 0x80;
    vdpSpriteCache[i].size_link = ((u16)size << 8) | link;
    vdpSpriteCache[i].attribut = attr;
    vdpSpriteCache[i].x = x + 0x80;
}

// Parks a sprite outside the visible area (hardware Y = 0 is above the screen)
// without removing it from the link chain.
static void hideSprite(u16 i, u8 size, u16 attr)
{
    u8 link = (i + 1 < SPRITE_COUNT) ? (u8)(i + 1) : 0;

    vdpSpriteCache[i].y = 0;
    vdpSpriteCache[i].size_link = ((u16)size << 8) | link;
    vdpSpriteCache[i].attribut = attr;
    vdpSpriteCache[i].x = 0x80;
}

// ---------------------------------------------------------------- game logic
static void resetFormation(void)
{
    u16 i;
    for (i = 0; i < NUM_ENEMIES; i++) alive[i] = 1;
    aliveCount = NUM_ENEMIES;
}

static void updateFormation(void)
{
    if (++formTimer >= FORM_STEP_FRAMES)
    {
        formTimer = 0;
        formOffs += formDir;
        if (formOffs <= 0)              { formOffs = 0;              formDir =  1; }
        if (formOffs >= FORM_MAX_OFFS)  { formOffs = FORM_MAX_OFFS;  formDir = -1; }
    }
}

static void updatePlayer(u16 pad)
{
    if (pad & BUTTON_LEFT)  playerX -= PLAYER_SPEED;
    if (pad & BUTTON_RIGHT) playerX += PLAYER_SPEED;
    if (playerX < 0) playerX = 0;
    if (playerX > PLAYER_MAX_X) playerX = PLAYER_MAX_X;

    // fire on button press (edge), one bullet at a time
    u16 pressed = pad & ~prevPad;
    if (!bulletActive && (pressed & (BUTTON_A | BUTTON_B | BUTTON_C)))
    {
        bulletActive = TRUE;
        bulletX = playerX + (SHIP_SIZE - BULLET_W) / 2;
        bulletY = PLAYER_Y - BULLET_H;
    }
}

static void updateBullet(void)
{
    if (!bulletActive) return;

    bulletY -= BULLET_SPEED;
    if (bulletY < -BULLET_H)
    {
        bulletActive = FALSE;
        return;
    }

    // bullet vs. enemies
    u16 row, col;
    for (row = 0; row < ROWS; row++)
    {
        s16 ey = FORM_TOP + row * PITCH_Y;
        if (bulletY >= ey + SHIP_SIZE || bulletY + BULLET_H <= ey) continue;

        for (col = 0; col < COLS; col++)
        {
            u16 idx = row * COLS + col;
            if (!alive[idx]) continue;

            s16 ex = formOffs + col * PITCH_X;
            if (bulletX < ex + SHIP_SIZE && bulletX + BULLET_W > ex)
            {
                alive[idx] = 0;
                aliveCount--;
                bulletActive = FALSE;
                return;
            }
        }
    }
}

static void drawSprites(void)
{
    u16 row, col;

    for (row = 0; row < ROWS; row++)
    {
        u16 attr = TILE_ATTR_FULL(PAL1 + row, TRUE, FALSE, FALSE, TILE_ENEMY);
        s16 y = FORM_TOP + row * PITCH_Y;

        for (col = 0; col < COLS; col++)
        {
            u16 idx = row * COLS + col;
            if (alive[idx])
                putSprite(SPR_ENEMY0 + idx, formOffs + col * PITCH_X, y,
                          SPRITE_SIZE(3, 3), attr);
            else
                hideSprite(SPR_ENEMY0 + idx, SPRITE_SIZE(3, 3), attr);
        }
    }

    putSprite(SPR_PLAYER, playerX, PLAYER_Y, SPRITE_SIZE(3, 3),
              TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_PLAYER));

    u16 battr = TILE_ATTR_FULL(PAL0, TRUE, FALSE, FALSE, TILE_BULLET);
    if (bulletActive)
        putSprite(SPR_BULLET, bulletX, bulletY, SPRITE_SIZE(1, 2), battr);
    else
        hideSprite(SPR_BULLET, SPRITE_SIZE(1, 2), battr);
}

// ---------------------------------------------------------------- main
int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setBackgroundColor(0);

    // Palettes (colour 0 of every palette is transparent for sprites)
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));   // backdrop

    // PAL0: player + bullet
    PAL_setColor(PAL0 * 16 + 1, RGB24_TO_VDPCOLOR(0x202060));
    PAL_setColor(PAL0 * 16 + 2, RGB24_TO_VDPCOLOR(0x4080E0));
    PAL_setColor(PAL0 * 16 + 3, RGB24_TO_VDPCOLOR(0xE0E0FF));
    PAL_setColor(PAL0 * 16 + 4, RGB24_TO_VDPCOLOR(0xFF4040));
    // PAL1..PAL3: one colour scheme per enemy row
    PAL_setColor(PAL1 * 16 + 1, RGB24_TO_VDPCOLOR(0x400000));
    PAL_setColor(PAL1 * 16 + 2, RGB24_TO_VDPCOLOR(0xC03030));
    PAL_setColor(PAL1 * 16 + 3, RGB24_TO_VDPCOLOR(0xFF9090));
    PAL_setColor(PAL1 * 16 + 4, RGB24_TO_VDPCOLOR(0xFFE040));

    PAL_setColor(PAL2 * 16 + 1, RGB24_TO_VDPCOLOR(0x003020));
    PAL_setColor(PAL2 * 16 + 2, RGB24_TO_VDPCOLOR(0x30B060));
    PAL_setColor(PAL2 * 16 + 3, RGB24_TO_VDPCOLOR(0x90FFB0));
    PAL_setColor(PAL2 * 16 + 4, RGB24_TO_VDPCOLOR(0xFFE040));

    PAL_setColor(PAL3 * 16 + 1, RGB24_TO_VDPCOLOR(0x302040));
    PAL_setColor(PAL3 * 16 + 2, RGB24_TO_VDPCOLOR(0xA050D0));
    PAL_setColor(PAL3 * 16 + 3, RGB24_TO_VDPCOLOR(0xE0B0FF));
    PAL_setColor(PAL3 * 16 + 4, RGB24_TO_VDPCOLOR(0xFFE040));

    // Tiles
    VDP_loadTileData(enemyTiles,  TILE_ENEMY,  9, CPU);
    VDP_loadTileData(playerTiles, TILE_PLAYER, 9, CPU);
    VDP_loadTileData(bulletTiles, TILE_BULLET, 2, CPU);

    // Sprite table
    VDP_resetSprites();

    JOY_init();

    resetFormation();
    formOffs = FORM_MAX_OFFS / 2;
    formDir = 1;
    formTimer = 0;
    playerX = (320 - SHIP_SIZE) / 2;
    bulletActive = FALSE;
    prevPad = 0;

    while (TRUE)
    {
        u16 pad = JOY_readJoypad(JOY_1);

        updatePlayer(pad);
        updateFormation();
        updateBullet();
        if (aliveCount == 0) resetFormation();      // endless demo: new wave
        prevPad = pad;

        drawSprites();
        VDP_updateSprites(SPRITE_COUNT, DMA_QUEUE); // sent to VRAM during VBlank
        SYS_doVBlankProcess();
    }

    return 0;
}
