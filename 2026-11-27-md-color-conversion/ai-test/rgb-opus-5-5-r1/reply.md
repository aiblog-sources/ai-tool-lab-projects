```python
"""Convert 24-bit RGB colors to Sega Mega Drive / Genesis CRAM color words.

CRAM word layout (9-bit color, 3 bits per channel):

    bit: 15..12 11..9 8  7..5  4  3..1  0
         0000   BBB   0  GGG   0  RRR   0

Each channel is a 3-bit intensity (0-7) stored in the upper three bits of a
nibble, so the low bit of each nibble is always 0. Valid words lie in 0x000-0xEEE.
"""

from typing import Sequence, Tuple

__all__ = [
    "LINEAR_LEVELS",
    "HARDWARE_LEVELS",
    "rgb_to_md",
    "md_to_rgb",
]

# Evenly spaced 8-bit equivalents of the 3-bit levels: round(i * 255 / 7).
# Most homebrew tools and emulators in "linear" mode assume this scale.
LINEAR_LEVELS: Tuple[int, ...] = tuple(round(i * 255 / 7) for i in range(8))
# -> (0, 36, 73, 109, 146, 182, 219, 255)

# Approximate output of real hardware's non-linear DAC in normal (not
# shadow/highlight) mode, as measured on Model 1 consoles.
HARDWARE_LEVELS: Tuple[int, ...] = (0, 52, 87, 116, 144, 172, 206, 255)


def _check_channel(name: str, value: int) -> None:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an int, got {type(value).__name__}")
    if not 0 <= value <= 255:
        raise ValueError(f"{name} must be in 0..255, got {value}")


def _nearest_level(value: int, levels: Sequence[int]) -> int:
    """Return the index (0-7) of the level closest to value.

    Ties resolve to the lower index, so results are deterministic.
    """
    best_index = 0
    best_dist = abs(value - levels[0])
    for index in range(1, len(levels)):
        dist = abs(value - levels[index])
        if dist < best_dist:
            best_index = index
            best_dist = dist
    return best_index


def rgb_to_md(
    r: int,
    g: int,
    b: int,
    levels: Sequence[int] = LINEAR_LEVELS,
) -> int:
    """Convert a 24-bit RGB color to the closest Mega Drive CRAM color word.

    Each channel is quantized independently to the nearest of the eight
    Mega Drive intensity levels. The CRAM channels are independent, so this
    also gives the closest color overall by Euclidean distance in RGB.

    Args:
        r, g, b: Channel values in 0..255.
        levels: Eight ascending 8-bit values that the 3-bit levels 0..7
            represent. Defaults to LINEAR_LEVELS. Pass HARDWARE_LEVELS to
            match how colors look on real hardware.

    Returns:
        The CRAM word, 0x0BGR with 3-bit channels in bits 1-3, 5-7, 9-11.

    Examples:
        >>> hex(rgb_to_md(255, 255, 255))
        '0xeee'
        >>> hex(rgb_to_md(255, 0, 0))
        '0xe'
        >>> hex(rgb_to_md(0, 0, 255))
        '0xe00'
        >>> hex(rgb_to_md(0, 0, 0))
        '0x0'
    """
    _check_channel("r", r)
    _check_channel("g", g)
    _check_channel("b", b)
    if len(levels) != 8:
        raise ValueError(f"levels must contain exactly 8 values, got {len(levels)}")

    r3 = _nearest_level(r, levels)
    g3 = _nearest_level(g, levels)
    b3 = _nearest_level(b, levels)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def md_to_rgb(
    word: int,
    levels: Sequence[int] = LINEAR_LEVELS,
) -> Tuple[int, int, int]:
    """Convert a CRAM color word back to 24-bit RGB (useful for previews).

    Bits outside the 0x0EEE mask are ignored, as the hardware ignores them.
    """
    if isinstance(word, bool) or not isinstance(word, int):
        raise TypeError(f"word must be an int, got {type(word).__name__}")
    if len(levels) != 8:
        raise ValueError(f"levels must contain exactly 8 values, got {len(levels)}")

    r3 = (word >> 1) & 0x7
    g3 = (word >> 5) & 0x7
    b3 = (word >> 9) & 0x7
    return (levels[r3], levels[g3], levels[b3])
```

Notes:
- **Output format:** The function returns a 16-bit word. Write it big-endian when you put it in ROM data, for example with `struct.pack(">H", word)`. The 68000 is big-endian.
- **Which levels to use:** The default `LINEAR_LEVELS` matches what most art tools and emulators assume. If you want the conversion to match how colors look on real hardware, pass `levels=HARDWARE_LEVELS`. That table is approximate because the hardware's DAC output is not linear.
- **Shadow/highlight mode:** It is not handled here. It doesn't change how CRAM is encoded. It only changes how the hardware displays the colors.