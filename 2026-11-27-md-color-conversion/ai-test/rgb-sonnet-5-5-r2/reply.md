```python
"""Convert 24-bit RGB colors to Sega Mega Drive / Genesis CRAM color words.

CRAM word layout (9-bit color, 3 bits per channel):

    bit: 15 14 13 12 | 11 10  9  8 |  7  6  5  4 |  3  2  1  0
          0  0  0  0 |  B  B  B  0 |  G  G  G  0 |  R  R  R  0

Each channel has 8 intensity levels (0-7), stored in bits 1-3 of its nibble,
so the stored nibble values are 0x0, 0x2, 0x4, ..., 0xE.

Each channel is matched to its nearest level independently. Because the
channels are independent, this also gives the minimum Euclidean RGB distance.
"""

from typing import Sequence, Tuple

__all__ = [
    "LINEAR_LEVELS",
    "HARDWARE_LEVELS",
    "rgb_to_md",
    "md_to_rgb",
]

# Evenly spaced levels: round(n * 255 / 7). This is the common convention used
# by most homebrew tools and emulators, and is the default here.
LINEAR_LEVELS: Tuple[int, ...] = tuple(round(n * 255 / 7) for n in range(8))
# -> (0, 36, 73, 109, 146, 182, 219, 255)

# Approximate measured output of the real VDP DAC (normal, non-shadow/
# highlight mode). The real hardware is non-linear. Use this table if you want
# matches that reflect how colors look on actual hardware.
HARDWARE_LEVELS: Tuple[int, ...] = (0, 52, 87, 116, 144, 172, 206, 255)


def _check_channel(name: str, value: int) -> None:
    if not isinstance(value, int) or isinstance(value, bool):
        raise TypeError(f"{name} must be an int, got {type(value).__name__}")
    if not 0 <= value <= 255:
        raise ValueError(f"{name} must be in range 0-255, got {value}")


def _check_levels(levels: Sequence[int]) -> None:
    if len(levels) != 8:
        raise ValueError(f"levels must contain exactly 8 entries, got {len(levels)}")


def _nearest_level(value: int, levels: Sequence[int]) -> int:
    """Return the index (0-7) of the level closest to value.

    If two levels are equally close, the lower index wins.
    """
    best_index = 0
    best_dist = abs(value - levels[0])
    for index in range(1, 8):
        dist = abs(value - levels[index])
        if dist < best_dist:
            best_index = index
            best_dist = dist
    return best_index


def rgb_to_md(
    r: int,
    g: int,
    b: int,
    *,
    levels: Sequence[int] = LINEAR_LEVELS,
) -> int:
    """Convert a 24-bit RGB color to the closest Mega Drive CRAM color word.

    Args:
        r, g, b: Channel values, 0-255 each.
        levels: Keyword-only. Eight 8-bit intensities that the hardware levels
            0-7 are assumed to produce. Defaults to LINEAR_LEVELS. Pass
            HARDWARE_LEVELS to match against real DAC output instead.

    Returns:
        A 16-bit CRAM word in the form 0000BBB0GGG0RRR0.

    Raises:
        TypeError: If a channel is not an int.
        ValueError: If a channel is out of range or levels is not 8 entries.

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
    _check_levels(levels)

    r_level = _nearest_level(r, levels)
    g_level = _nearest_level(g, levels)
    b_level = _nearest_level(b, levels)

    return (b_level << 9) | (g_level << 5) | (r_level << 1)


def md_to_rgb(
    word: int,
    *,
    levels: Sequence[int] = LINEAR_LEVELS,
) -> Tuple[int, int, int]:
    """Convert a CRAM color word back to 24-bit RGB (useful for previews).

    Bits outside the 0x0EEE mask are ignored, just as the VDP ignores them.

    Examples:
        >>> md_to_rgb(0x0EEE)
        (255, 255, 255)
        >>> md_to_rgb(rgb_to_md(200, 100, 50))
        (219, 109, 36)
    """
    if not isinstance(word, int) or isinstance(word, bool):
        raise TypeError(f"word must be an int, got {type(word).__name__}")
    _check_levels(levels)

    r_level = (word >> 1) & 0x7
    g_level = (word >> 5) & 0x7
    b_level = (word >> 9) & 0x7
    return (levels[r_level], levels[g_level], levels[b_level])


if __name__ == "__main__":
    import doctest

    doctest.testmod()
```

**Notes:**
- The returned word is ready to write to CRAM as a big-endian 16-bit value, for example `word.to_bytes(2, "big")` or `struct.pack(">H", word)`.
- **Which level table to use:** `LINEAR_LEVELS` (the default) matches what most tools and emulators assume. `HARDWARE_LEVELS` approximates the real console's non-linear DAC. If your art looks different on real hardware than in an emulator, try `rgb_to_md(r, g, b, levels=HARDWARE_LEVELS)`.
- For a PNG converter, add a cache such as `functools.lru_cache` or a dict keyed by `(r, g, b)`. Images reuse the same colors many times, so this saves a lot of repeated work.