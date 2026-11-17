Two changes remove most of the per-loop work. Neither alters what the scene shows.

- **No more `oam_hide_all` per loop.** It now runs once at startup. Inactive bullets are hidden individually by setting their Y to $FF.
- **Constant OAM fields written once.** Tiles, attributes and the enemy and player Y values never change, so they are set at init. Each loop only writes the changing X values and the active bullets.

The per-loop cost should drop from about 33,800 cycles to roughly 3,000, well under one frame. I haven't built or profiled this version.

```c
#include "nes.h"

#define ENEMY_COLS   10          /* spec: 10. Hardware fits only 4 per row without dropouts. */
#define ENEMY_ROWS   2
#define ENEMY_PITCH  20
#define ENEMY_ROW_Y0 40
#define ENEMY_ROW_DY 24
#define FORM_WIDTH   ((ENEMY_COLS - 1) * ENEMY_PITCH + 16)
#define FORM_MIN_X   8
#define FORM_MAX_X   (256 - 8 - FORM_WIDTH)

#define MAX_BULLETS  6
#define PLAYER_Y     208
#define PLAYER_MIN_X 8
#define PLAYER_MAX_X 232

static const unsigned char palette[32] = {
    /* background */
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    0x0F,0x00,0x10,0x30, 0x0F,0x00,0x10,0x30,
    /* sprites */
    0x0F,0x21,0x11,0x30, 0x0F,0x16,0x26,0x30,
    0x0F,0x1A,0x2A,0x30, 0x0F,0x28,0x38,0x30
};

static unsigned char bullet_y[MAX_BULLETS];
static unsigned char bullet_x[MAX_BULLETS];
static unsigned char bullet_on[MAX_BULLETS];

static unsigned char player_x;
static unsigned char form_x;
static signed char   form_dir;
static unsigned char pad, prev_pad;

/* write the fields of a ship that never change */
static void init_ship(unsigned char *p, unsigned char y, unsigned char attr)
{
    p[0] = y;
    p[1] = 0x10;
    p[2] = attr;
    p[4] = y;
    p[5] = 0x10;
    p[6] = attr | 0x40;      /* horizontal flip for the right half */
}

static void init_oam(void)
{
    unsigned char i, r, c;
    unsigned char *p;

    oam_hide_all();

    /* player: sprites 0,1 */
    init_ship(OAM, PLAYER_Y, 0);

    /* bullets: sprites 2..7, tile $00, attr 1 */
    p = OAM + 8;
    for (i = 0; i < MAX_BULLETS; ++i) {
        p[1] = 0x00;
        p[2] = 1;
        p += 4;
    }

    /* enemies: sprites 8.. */
    p = OAM + 32;
    for (r = 0; r < ENEMY_ROWS; ++r) {
        unsigned char y = ENEMY_ROW_Y0 + r * ENEMY_ROW_DY;
        for (c = 0; c < ENEMY_COLS; ++c) {
            init_ship(p, y, 1 + r);
            p += 8;
        }
    }
}

static void update(void)
{
    unsigned char i;

    prev_pad = pad;
    pad = pad_read();

    if ((pad & PAD_LEFT) && player_x > PLAYER_MIN_X) player_x -= 2;
    if ((pad & PAD_RIGHT) && player_x < PLAYER_MAX_X) player_x += 2;

    if ((pad & PAD_A) && !(prev_pad & PAD_A)) {
        for (i = 0; i < MAX_BULLETS; ++i) {
            if (!bullet_on[i]) {
                bullet_on[i] = 1;
                bullet_x[i] = player_x + 4;
                bullet_y[i] = PLAYER_Y - 4;
                break;
            }
        }
    }

    for (i = 0; i < MAX_BULLETS; ++i) {
        if (bullet_on[i]) {
            if (bullet_y[i] < 16) bullet_on[i] = 0;
            else bullet_y[i] -= 4;
        }
    }

    if (form_dir > 0) {
        if (form_x >= FORM_MAX_X) form_dir = -1; else ++form_x;
    } else {
        if (form_x <= FORM_MIN_X) form_dir = 1; else --form_x;
    }
}

static void draw(void)
{
    unsigned char i, x;
    unsigned char *p;

    /* player */
    x = player_x;
    OAM[3] = x;
    OAM[7] = x + 8;

    /* bullets: tile $00 in 8x16 mode = blank top, solid bottom => 8x8 at Y */
    p = OAM + 8;
    for (i = 0; i < MAX_BULLETS; ++i) {
        if (bullet_on[i]) {
            p[0] = bullet_y[i] - 8;
            p[3] = bullet_x[i];
        } else {
            p[0] = 0xFF;
        }
        p += 4;
    }

    /* enemy formation: only X changes; both rows share the same X */
    x = form_x;
    p = OAM + 32 + 3;
    for (i = 0; i < ENEMY_COLS; ++i) {
        p[0]  = x;
        p[80] = x;
        x += 8;
        p[4]  = x;
        p[84] = x;
        x += ENEMY_PITCH - 8;
        p += 8;
    }
}

void main(void)
{
    unsigned char i;

    ppu_setup(palette);
    ppu_ctrl |= CTRL_SPR_8X16;

    player_x = 120;
    form_x = FORM_MIN_X;
    form_dir = 1;
    pad = prev_pad = 0;
    for (i = 0; i < MAX_BULLETS; ++i) bullet_on[i] = 0;

    init_oam();

    for (;;) {
        update();
        draw();
        oam_ready();
        wait_frame();
    }
}
```