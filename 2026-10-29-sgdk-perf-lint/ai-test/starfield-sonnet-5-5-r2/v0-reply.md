```c
#include <genesis.h>

#define NUM_STARS       64
#define SHIP_SPRITE     0                    /* lowest index = drawn on top */
#define STAR_SPRITE0    1                    /* stars use sprites 1..64 */
#define NUM_SPRITES     (NUM_STARS + 1)

#define SCREEN_W        320
#define SCREEN_H        224
#define SHIP_SPEED      3

/* Tile layout in VRAM */
#define TILE_STAR0      (TILE_USER_INDEX + 0)   /* far  - dim,    slow   */
#define TILE_STAR1      (TILE_USER_INDEX + 1)   /* mid  - grey,   medium */
#define TILE_STAR2      (TILE_USER_INDEX + 2)   /* near - bright, fast   */
#define TILE_SHIP       (TILE_USER_INDEX + 3)   /* 4 tiles (16x16)       */

/* 8x8 tiles, 4bpp: one u32 per row, one nibble per pixel */
static const u32 starTiles[3 * 8] =
{
    /* Far star: single dim pixel (color 1) */
    0x00000000, 0x00000000, 0x00000000, 0x00010000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* Mid star: single pixel (color 2) */
    0x00000000, 0x00000000, 0x00000000, 0x00020000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,

    /* Near star: 2x2 bright block (color 3) */
    0x00000000, 0x00000000, 0x00033000, 0x00033000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/* 16x16 ship as a 2x2 sprite. Hardware sprite tile order is column-major:
   top-left, bottom-left, top-right, bottom-right. */
static const u32 shipTiles[4 * 8] =
{
    /* Top-left */
    0x00000000, 0x00440000, 0x00444000, 0x00444440,
    0x00444444, 0x66444444, 0x66444444, 0x06444554,
    /* Bottom-left */
    0x06444554, 0x66444444, 0x66444444, 0x00444444,
    0x00444440, 0x00444000, 0x00440000, 0x00000000,
    /* Top-right */
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x40000000, 0x44400000, 0x44444000, 0x44444440,
    /* Bottom-right */
    0x44444440, 0x44444000, 0x44400000, 0x40000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000
};

typedef struct
{
    s16 x;
    s16 y;
    u8  speed;
} Star;

static Star stars[NUM_STARS];
static s16 shipX, shipY;

static void initStars(void)
{
    for (u16 i = 0; i < NUM_STARS; i++)
    {
        Star *s = &stars[i];
        u8 layer = i % 3;                    /* 0 = far, 1 = mid, 2 = near */

        s->speed = layer + 1;                /* 1, 2, 3 px per frame */
        s->x = random() % SCREEN_W;
        /* Spread rows evenly to stay under the 20 sprites/scanline limit */
        s->y = (i * 7) >> 1;                 /* 0..220 */

        VDP_setSpriteFull(STAR_SPRITE0 + i,
                          s->x, s->y,
                          SPRITE_SIZE(1, 1),
                          TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_STAR0 + layer),
                          STAR_SPRITE0 + i + 1);
    }
}

static void updateStars(void)
{
    for (u16 i = 0; i < NUM_STARS; i++)
    {
        Star *s = &stars[i];

        s->x -= s->speed;
        if (s->x < -8)
        {
            s->x = SCREEN_W + (random() & 15);
        }
        VDP_setSpritePosition(STAR_SPRITE0 + i, s->x, s->y);
    }
}

static void updateShip(void)
{
    u16 joy = JOY_readJoypad(JOY_1);

    if (joy & BUTTON_LEFT)  shipX -= SHIP_SPEED;
    if (joy & BUTTON_RIGHT) shipX += SHIP_SPEED;
    if (joy & BUTTON_UP)    shipY -= SHIP_SPEED;
    if (joy & BUTTON_DOWN)  shipY += SHIP_SPEED;

    if (shipX < 0)                shipX = 0;
    if (shipX > SCREEN_W - 16)    shipX = SCREEN_W - 16;
    if (shipY < 0)                shipY = 0;
    if (shipY > SCREEN_H - 16)    shipY = SCREEN_H - 16;

    VDP_setSpritePosition(SHIP_SPRITE, shipX, shipY);
}

int main(bool hardReset)
{
    (void)hardReset;

    VDP_setScreenWidth320();
    VDP_setScreenHeight224();

    /* Palette 0 */
    PAL_setColor(0, RGB24_TO_VDPCOLOR(0x000000));  /* backdrop */
    PAL_setColor(1, RGB24_TO_VDPCOLOR(0x484848));  /* far star */
    PAL_setColor(2, RGB24_TO_VDPCOLOR(0x909090));  /* mid star */
    PAL_setColor(3, RGB24_TO_VDPCOLOR(0xFFFFFF));  /* near star */
    PAL_setColor(4, RGB24_TO_VDPCOLOR(0x2468D8));  /* ship body */
    PAL_setColor(5, RGB24_TO_VDPCOLOR(0x48E8E8));  /* cockpit */
    PAL_setColor(6, RGB24_TO_VDPCOLOR(0xFF8800));  /* engine flame */

    /* Load tiles from code (no resources) */
    VDP_loadTileData(starTiles, TILE_STAR0, 3, CPU);
    VDP_loadTileData(shipTiles, TILE_SHIP,  4, CPU);

    setRandomSeed(0x1234);

    VDP_resetSprites();

    /* Ship at the lowest sprite index (on top of stars), linked to star 1 */
    shipX = 40;
    shipY = (SCREEN_H - 16) / 2;
    VDP_setSpriteFull(SHIP_SPRITE, shipX, shipY,
                      SPRITE_SIZE(2, 2),
                      TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_SHIP),
                      STAR_SPRITE0);

    initStars();

    /* Terminate the sprite linked list at the last sprite */
    VDP_setSpriteLink(NUM_SPRITES - 1, 0);

    while (TRUE)
    {
        updateShip();
        updateStars();

        VDP_updateSprites(NUM_SPRITES, DMA_QUEUE);
        SYS_doVBlankProcess();
    }

    return 0;
}
```

Notes:
- The ship is sprite 0, so it draws in front of the stars. Stars are sprites 1–64, linked in one chain.
- The three layers move 1, 2 and 3 px/frame. Each layer has its own tile: dim, mid-grey and a bright 2×2 block.
- Star rows are spread evenly (`(i*7)>>1`) to stay under the VDP's 20-sprites-per-scanline limit.
- I wrote this from memory of the SGDK 2.x sprite API and haven't built it here. If your build complains about `VDP_setSpriteFull`, `VDP_setSpritePosition` or `VDP_setSpriteLink`, check their signatures in `vdp_spr.h`.