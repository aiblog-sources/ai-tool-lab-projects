```python
"""Convert 24-bit RGB colors to Sega Mega Drive / Genesis CRAM color words.

CRAM word layout (9-bit color, 3 bits per channel):

    bit:  15..12  11..9  8  7..5  4  3..1  0
          0000    BBB    0  GGG   0  RRR   0

So the word is ``(b3 << 9) | (g3 << 5) | (r3 << 1)``, where each of
r3/g3/b3 is in the range 0..7.

The default mode maps each channel linearly (level n -> n * 255 / 7), which
matches most emulators and common asset tools. The "measured" mode instead
targets approximate real hardware DAC output (normal, non-shadow/highlight
mode), which is non-linear. Choose whichever matches how you preview art.

Because the channels are independent, picking the nearest level per channel
also gives the color with the smallest Euclidean RGB distance among all 512
Mega Drive colors.
"""

from typing import Sequence, Tuple

__all__ = [
    "LINEAR_LEVELS",
    "MEASURED_LEVELS",
    "rgb_to_md",
    "md_to_rgb",
]

# Intensity (0-255) of each 3-bit level, evenly spaced.
LINEAR_LEVELS: Tuple[int, ...] = tuple(round(n * 255 / 7) for n in range(8))
# -> (0, 36, 73, 109, 146, 182, 219, 255)

# Approximate measured hardware output for normal (non-S/H) mode.
MEASURED_LEVELS: Tuple[int, ...] = (0, 52, 87, 116, 144, 172, 206, 255)


def _check_channel(name: str, value: int) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{name} must be an int, got {type(value).__name__}")
    if not 0 <= value <= 255:
        raise ValueError(f"{name} must be in 0..255, got {value}")
    return value


def _nearest_level(value: int, levels: Sequence[int]) -> int:
    """Return the index (0..7) of the level closest to value.

    Ties go to the lower level, so the result is deterministic.
    """
    best_index = 0
    best_dist = abs(value - levels[0])
    for index in range(1, len(levels)):
        dist = abs(value - levels[index])
        if dist < best_dist:
            best_index = index
            best_dist = dist
    return best_index


def rgb_to_md(r: int, g: int, b: int, *, measured: bool = False) -> int:
    """Convert a 24-bit RGB color to the closest Mega Drive CRAM color word.

    Args:
        r, g, b: Channel values in 0..255.
        measured: If True, match against approximate real-hardware DAC
            levels instead of evenly spaced levels.

    Returns:
        A 16-bit CRAM word in the format 0000 BBB0 GGG0 RRR0.

    Raises:
        TypeError: If a channel is not an int.
        ValueError: If a channel is outside 0..255.

    Examples:
        >>> hex(rgb_to_md(255, 255, 255))
        '0xeee'
        >>> hex(rgb_to_md(255, 0, 0))
        '0xe'
        >>> hex(rgb_to_md(0, 0, 255))
        '0xe00'
    """
    _check_channel("r", r)
    _check_channel("g", g)
    _check_channel("b", b)

    levels = MEASURED_LEVELS if measured else LINEAR_LEVELS
    r3 = _nearest_level(r, levels)
    g3 = _nearest_level(g, levels)
    b3 = _nearest_level(b, levels)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def md_to_rgb(word: int, *, measured: bool = False) -> Tuple[int, int, int]:
    """Convert a CRAM color word back to 24-bit RGB (useful for previews).

    Unused bits in the word are ignored.
    """
    if isinstance(word, bool) or not isinstance(word, int):
        raise TypeError(f"word must be an int, got {type(word).__name__}")
    if not 0 <= word <= 0xFFFF:
        raise ValueError(f"word must be in 0..0xFFFF, got {word}")

    levels = MEASURED_LEVELS if measured else LINEAR_LEVELS
    r3 = (word >> 1) & 0x7
    g3 = (word >> 5) & 0x7
    b3 = (word >> 9) & 0x7
    return levels[r3], levels[g3], levels[b3]
```

**Notes:**
- Output format is `0000 BBB0 GGG0 RRR0`, the value you write to CRAM or store in a palette in ROM.
- By default the function picks the nearest of 8 evenly spaced levels. This matches most emulators and existing tools.
- `measured=True` matches against approximate real-hardware DAC levels instead. Use it if you tune art against a real console.
- `md_to_rgb` lets you preview the quantized palette or check round trips.