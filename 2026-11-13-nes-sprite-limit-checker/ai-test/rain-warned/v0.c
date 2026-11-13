/* Rain and umbrella for the NES (cc65), mapper 0.
 *
 * The PPU draws at most 8 sprites per scanline; any more are dropped and
 * flicker on real hardware. The program is built so that cannot happen:
 *  - The umbrella is 4x2 8x8 sprites, so it puts 4 sprites on any scanline.
 *  - The 40 drops form 4 layers of 10. All drops in a layer share one speed
 *    and are spaced 24 lines apart (10 * 24 = 240, the wrap length), so no
 *    two drops of a layer ever overlap vertically. That is at most 1 drop
 *    per layer per scanline, so at most 4 drops per scanline.
 *  Worst case is 4 + 4 = 8 sprites per scanline, which fits.
 */
#include "nes.h"

#define NUM_DROPS   40
#define NUM_LAYERS  4
#define WRAP_Y      240
#define SPACING     24

#define TILE_DROP   0x08

#define UMB_W       32
#define UMB_MIN_Y   100
#define UMB_MAX_Y   208
#define UMB_SPEED   3

static const unsigned char palette[32] = {
    /* background */
    0x0F, 0x01, 0x11, 0x30,
    0x0F, 0x01, 0x11, 0x30,
    0x0F, 0x01, 0x11, 0x30,
    0x0F, 0x01, 0x11, 0x30,
    /* sprites: 0 = rain, 1 = umbrella */
    0x0F, 0x2C, 0x21, 0x30,
    0x0F, 0x16, 0x27, 0x30,
    0x0F, 0x16, 0x27, 0x30,
    0x0F, 0x16, 0x27, 0x30
};

static const unsigned char layer_speed[NUM_LAYERS] = { 2, 3, 4, 6 };

static unsigned char drop_x[NUM_DROPS];
static unsigned char drop_y[NUM_DROPS];

static unsigned char umb_x = 112;
static unsigned char umb_y = 200;

/* Umbrella tiles: top row = canopy, bottom row = ribs at the ends. */
static const unsigned char umb_tiles[8] = {
    0x02, 0x02, 0x02, 0x02,
    0x01, 0x00, 0x00, 0x01
};

static void init_drops(void)
{
    unsigned char i;
    for (i = 0; i < NUM_DROPS; ++i) {
        /* i = j * 4 + layer; y = j * 24 + layer * 6 */
        drop_y[i] = (unsigned char)((i >> 2) * SPACING + (i & 3) * 6);
        drop_x[i] = rnd8();
    }
}

static void update_drops(void)
{
    unsigned char i, y;
    for (i = 0; i < NUM_DROPS; ++i) {
        y = drop_y[i] + layer_speed[i & 3];   /* max 239 + 6 < 256 */
        if (y >= WRAP_Y) {
            y -= WRAP_Y;
            drop_x[i] = rnd8();
        }
        drop_y[i] = y;
    }
}

static void update_umbrella(void)
{
    unsigned char pad = pad_read();

    if (pad & PAD_LEFT) {
        umb_x = (umb_x >= UMB_SPEED) ? umb_x - UMB_SPEED : 0;
    }
    if (pad & PAD_RIGHT) {
        umb_x += UMB_SPEED;
        if (umb_x > 256 - UMB_W) umb_x = 256 - UMB_W;
    }
    if (pad & PAD_UP) {
        umb_y -= UMB_SPEED;
        if (umb_y < UMB_MIN_Y) umb_y = UMB_MIN_Y;
    }
    if (pad & PAD_DOWN) {
        umb_y += UMB_SPEED;
        if (umb_y > UMB_MAX_Y) umb_y = UMB_MAX_Y;
    }
}

static void fill_oam(void)
{
    unsigned char i, n = 0;

    /* umbrella: 8 sprites, palette 1 */
    for (i = 0; i < 8; ++i) {
        OAM_Y(n)    = umb_y + ((i >> 2) << 3);
        OAM_TILE(n) = umb_tiles[i];
        OAM_ATTR(n) = 1;
        OAM_X(n)    = umb_x + ((i & 3) << 3);
        ++n;
    }

    /* raindrops: 40 sprites, palette 0 */
    for (i = 0; i < NUM_DROPS; ++i) {
        OAM_Y(n)    = drop_y[i];
        OAM_TILE(n) = TILE_DROP;
        OAM_ATTR(n) = 0;
        OAM_X(n)    = drop_x[i];
        ++n;
    }
    /* entries 48..63 stay hidden from oam_hide_all() */
}

void main(void)
{
    ppu_setup(palette);
    ppu_ctrl &= (unsigned char)~CTRL_SPR_8X16;   /* 8x8 sprites */
    oam_hide_all();
    init_drops();

    for (;;) {
        update_umbrella();
        update_drops();
        fill_oam();
        oam_ready();
        wait_frame();
    }
}
