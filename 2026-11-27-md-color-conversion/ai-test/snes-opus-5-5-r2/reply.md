```python
"""Convert SNES BGR555 colors to Sega Mega Drive / Genesis CRAM color words.

SNES color word (CGRAM), 15 bits, bit 15 ignored:
    0bbbbbgg gggrrrrr        R = bits 0-4, G = bits 5-9, B = bits 10-14

Mega Drive CRAM word, 3 bits per channel:
    0000BBB0 GGG0RRR0        R = bits 1-3, G = bits 5-7, B = bits 9-11
"""

__all__ = ["snes_to_md", "md_to_rgb888", "MD_LEVELS_LINEAR", "MD_LEVELS_MEASURED"]

# Linear 3-bit to 8-bit expansion (round(level * 255 / 7)).
MD_LEVELS_LINEAR = (0, 36, 73, 109, 146, 182, 219, 255)

# Approximate measured analog output of the MD VDP in normal (non-shadow)
# mode, scaled to 0-255. Real hardware is not linear.
MD_LEVELS_MEASURED = (0, 52, 87, 116, 144, 172, 206, 255)


def _snes_channel_to_8bit(v: int) -> int:
    """Expand a 5-bit SNES channel to 8 bits (bit replication)."""
    return (v << 3) | (v >> 2)


def _nearest_level(value8: int, levels) -> int:
    """Return the 3-bit index whose 8-bit level is closest to value8."""
    best_index = 0
    best_dist = abs(levels[0] - value8)
    for i in range(1, 8):
        d = abs(levels[i] - value8)
        if d < best_dist:  # strict '<' means ties go to the darker level
            best_index, best_dist = i, d
    return best_index


def snes_to_md(color: int, nonlinear: bool = False) -> int:
    """Convert a SNES BGR555 color word to the closest Mega Drive CRAM word.

    Args:
        color: SNES color word, 0x0000-0xFFFF. Bit 15 is ignored, as on hardware.
        nonlinear: If False (the default), map each channel onto a linear
            0-7 scale. If True, choose the closest level from
            MD_LEVELS_MEASURED, which matches real hardware output more closely.

    Returns:
        A Mega Drive CRAM color word in the form 0000BBB0GGG0RRR0.

    Raises:
        TypeError: If color is not an int.
        ValueError: If color is outside 0x0000-0xFFFF.
    """
    if not isinstance(color, int) or isinstance(color, bool):
        raise TypeError("color must be an int")
    if not 0 <= color <= 0xFFFF:
        raise ValueError("color must be in range 0x0000-0xFFFF")

    r5 = color & 0x1F
    g5 = (color >> 5) & 0x1F
    b5 = (color >> 10) & 0x1F

    if nonlinear:
        r3 = _nearest_level(_snes_channel_to_8bit(r5), MD_LEVELS_MEASURED)
        g3 = _nearest_level(_snes_channel_to_8bit(g5), MD_LEVELS_MEASURED)
        b3 = _nearest_level(_snes_channel_to_8bit(b5), MD_LEVELS_MEASURED)
    else:
        # Nearest of round(v * 7 / 31), in integer math:
        # floor((14v + 31) / 62)
        r3 = (r5 * 14 + 31) // 62
        g3 = (g5 * 14 + 31) // 62
        b3 = (b5 * 14 + 31) // 62

    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def md_to_rgb888(md_color: int, nonlinear: bool = False) -> tuple:
    """Decode a Mega Drive CRAM word into an (r, g, b) 8-bit tuple, for previews."""
    levels = MD_LEVELS_MEASURED if nonlinear else MD_LEVELS_LINEAR
    return (
        levels[(md_color >> 1) & 7],
        levels[(md_color >> 5) & 7],
        levels[(md_color >> 9) & 7],
    )


if __name__ == "__main__":
    # Quick self-check
    assert snes_to_md(0x0000) == 0x000
    assert snes_to_md(0x7FFF) == 0xEEE
    assert snes_to_md(0x001F) == 0x00E   # pure red
    assert snes_to_md(0x03E0) == 0x0E0   # pure green
    assert snes_to_md(0x7C00) == 0xE00   # pure blue
    assert snes_to_md(0xFFFF) == 0xEEE   # bit 15 ignored
    assert snes_to_md(0x7FFF, nonlinear=True) == 0xEEE
    print("ok")
```

**Notes**
- **Default mode (linear):** each 5-bit SNES channel is rounded to the nearest 3-bit level. For example, the value 31 becomes 7.
- **`nonlinear=True`:** use this mode if you want results that look closer to real hardware. It picks the closest level from an approximate measured MD output curve. That curve has brighter low and mid levels than a linear scale.
- **Precision loss:** dropping from 32 to 8 levels per channel will merge colors. A ported palette can end up with fewer distinct colors than the SNES original. Check for duplicates if your tool depends on palette indices staying unique.