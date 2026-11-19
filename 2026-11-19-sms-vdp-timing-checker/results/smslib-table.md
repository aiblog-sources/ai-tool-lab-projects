| SMSlib call | Writes per call | Smallest gap (cycles) | Under 23 | Under 26 | Under 29 | Lines |
| --- | --- | --- | --- | --- | --- | --- |
| SMS_loadTiles (4 tiles) | 128 | 28 | 0 | 0 | 127 | 65-81 |
| SMS_VRAMmemcpy (128 B) | 128 | 28 | 0 | 0 | 127 | 65-81 |
| SMS_VRAMmemcpy_brief (128 B) | 128 | 28 | 0 | 0 | 127 | 65-81 |
| SMS_VRAMmemset (128 B) | 128 | 28 | 0 | 0 | 127 | 65-81 |
| SMS_VRAMmemsetW (128 B) | 128 | 28 | 0 | 0 | 127 | 65-81 |
| SMS_loadTileMap (32 entries) | 64 | 28 | 0 | 0 | 63 | 65-73 |
| SMS_loadTileMapArea (8x4) | 64 | 27 | 0 | 0 | 60 | 65-74 |
| SMS_loadTileMapColumn (24) | 48 | 30 | 0 | 0 | 0 | 65-78 |
| SMS_setTileatXY x32 | 64 | 27 | 0 | 0 | 32 | 65-91 |
| SMS_setNextTileatXY + SMS_setTile x32 | 64 | 27 | 0 | 0 | 32 | 65-79 |
| SMS_loadBGPalette | 16 | 28 | 0 | 0 | 15 | 65-67 |
| SMS_loadSpritePalette | 16 | 28 | 0 | 0 | 15 | 65-67 |
| SMS_setBGPaletteColor x16 | 16 | 208 | 0 | 0 | 0 | 65-79 |
| SMS_zeroBGPalette | 16 | 28 | 0 | 0 | 15 | 65-67 |
| SMS_loadBGPaletteHalfBrightness | 16 | 48 | 0 | 0 | 0 | 65-68 |
| SMS_load1bppTiles (4 tiles) | 128 | 128 | 0 | 0 | 0 | 67-143 |
| SMS_load2bppTiles (4 tiles) | 128 | 27 | 0 | 0 | 127 | 65-80 |
| SMS_print (32 chars) | 64 | 27 | 0 | 0 | 32 | 65-82 |
| SMS_copySpritestoSAT (16 sprites) | 49 | 28 | 0 | 0 | 46 | 65-71 |
| UNSAFE_SMS_copySpritestoSAT (16 sprites) | 49 | 16 | 46 | 46 | 46 | 65-70 |
| UNSAFE_SMS_VRAMmemcpy128 | 128 | 16 | 127 | 127 | 127 | 65-74 |
| UNSAFE_SMS_VRAMmemcpy (128 B) | 128 | 16 | 127 | 127 | 127 | 65-74 |
| SMS_readVRAM (64 B, reads) | 64 reads | | | 0 reads | | |
| SMS_setAddr + SMS_setTile x32 | 64 | 27 | 0 | 0 | 32 | 65-88 |
