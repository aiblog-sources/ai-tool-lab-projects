// Shows res/bg.png (an indexed PNG written by `pixcheck.py md ... --export`) on plane B, so the emulator
// screenshot can be compared with the source image pixel by pixel. MIT license.
#include <genesis.h>
#include "resources.h"

volatile u16 user_tiles;   // TILE_USER_LENGTH, read by mdprof --ram
volatile u16 shown;        // set to 1 once the image is on screen

int main(bool hard)
{
    user_tiles = TILE_USER_LENGTH;
    VDP_setScreenWidth320();
    PAL_setColors(0, bg_pal.data, 64, DMA);
    VDP_setBackgroundColor(0);
    VDP_drawImageEx(BG_B, &bg, TILE_ATTR_FULL(PAL0, FALSE, FALSE, FALSE, TILE_USER_INDEX), 0, 0, FALSE, DMA);
    shown = 1;
    while (TRUE) SYS_doVBlankProcess();
    return 0;
}
