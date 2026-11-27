```python
"""
Mega Drive / Genesis CRAM color conversion helpers.

CRAM color word layout (9-bit color, 16-bit word):

    bit: 15..12  11..9  8   7..5   4   3..1   0
         0000    BBB    0   GGG    0   RRR    0

Each channel has 3 bits (8 levels), stored in the upper three bits of
each nibble, so valid words match the mask 0x0EEE.
"""

__all__ = [
    "rgb_to_md",
    "md_to_rgb",
    "LINEAR_LEVELS",
    "HARDWARE_LEVELS",
]

# Evenly spaced 8-level ramp (0, 36, 73, ..., 255). This is what most
# emulators and art tools assume, and it round-trips cleanly.
LINEAR_LEVELS = tuple(round(i * 255 / 7) for i in range(8))

# Approximate measured output of real Mega Drive VDP hardware (normal,
# non-shadow/highlight mode). The DAC is nonlinear. Use this if your art
# was authored against captures of real hardware.
HARDWARE_LEVELS = (0, 52, 87, 116, 144, 172, 206, 255)


def _check_channel(name: str, value: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an int, got {type(value).__name__}")
    if not 0 <= value <= 255:
        raise ValueError(f"{name} must be in 0..255, got {value}")
    return value


def _nearest_level(value: int, levels: tuple) -> int:
    """Return the 3-bit index (0..7) of the level closest to value.

    Ties go to the lower index, which keeps the result deterministic.
    """
    best_index = 0
    best_dist = abs(value - levels[0])
    for i in range(1, 8):
        dist = abs(value - levels[i])
        if dist < best_dist:
            best_index = i
            best_dist = dist
    return best_index


def rgb_to_md(r: int, g: int, b: int, levels: tuple = LINEAR_LEVELS) -> int:
    """Convert a 24-bit RGB color to the closest Mega Drive CRAM color word.

    Args:
        r, g, b: Channel values in 0..255.
        levels: An 8-entry ascending table giving the 8-bit intensity each
            3-bit level represents. Defaults to LINEAR_LEVELS; pass
            HARDWARE_LEVELS to match real-hardware DAC output.

    Returns:
        A 16-bit CRAM word in the form 0000BBB0GGG0RRR0 (always & 0x0EEE).

    Each channel is matched independently, which also gives the closest
    color overall by Euclidean distance in RGB, since the channels don't
    interact in the 9-bit palette.

    >>> hex(rgb_to_md(255, 255, 255))
    '0xeee'
    >>> hex(rgb_to_md(255, 0, 0))
    '0xe'
    >>> hex(rgb_to_md(0, 255, 0))
    '0xe0'
    >>> hex(rgb_to_md(0, 0, 255))
    '0xe00'
    >>> rgb_to_md(0, 0, 0)
    0
    """
    _check_channel("r", r)
    _check_channel("g", g)
    _check_channel("b", b)
    if len(levels) != 8:
        raise ValueError("levels must contain exactly 8 entries")

    r3 = _nearest_level(r, levels)
    g3 = _nearest_level(g, levels)
    b3 = _nearest_level(b, levels)
    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def md_to_rgb(word: int, levels: tuple = LINEAR_LEVELS) -> tuple:
    """Convert a CRAM color word back to an (r, g, b) tuple.

    Useful for previews and for checking how much a color shifted.
    Bits outside the 0x0EEE mask are ignored.

    >>> md_to_rgb(0x0EEE)
    (255, 255, 255)
    """
    if isinstance(word, bool) or not isinstance(word, int):
        raise TypeError(f"word must be an int, got {type(word).__name__}")
    if len(levels) != 8:
        raise ValueError("levels must contain exactly 8 entries")
    r3 = (word >> 1) & 7
    g3 = (word >> 5) & 7
    b3 = (word >> 9) & 7
    return (levels[r3], levels[g3], levels[b3])


if __name__ == "__main__":
    import doctest

    doctest.testmod()
```

**Notes:**

- **Format:** The returned word is `0000 BBB0 GGG0 RRR0`, ready to write big-endian (`word.to_bytes(2, "big")`) into a palette blob for CRAM.
- **Rounding:** With the default linear ramp, each channel goes to the nearest of 8 evenly spaced levels. This works out to `round(v * 7 / 255)`. It's more accurate than the common `v >> 5` shortcut, which always truncates downward and darkens art.
- **Hardware accuracy:** The real VDP's DAC is nonlinear. If your source PNGs were made against real-hardware captures, pass `levels=HARDWARE_LEVELS` for a closer match.
- **Shadow/highlight mode** isn't handled. In that mode, the same CRAM word shows at half or boosted intensity, so you'd pick the color from the normal-mode value.