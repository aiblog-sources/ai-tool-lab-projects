"""
Mega Drive / Genesis color conversion helpers.

CRAM color word layout (9-bit color, 3 bits per channel):

    bit: 15..12 11 10 9 8  7 6 5 4  3 2 1 0
         0000   B  B  B 0  G G G 0  R R R 0

Each channel is a 3-bit level (0-7) stored shifted left by one bit,
so channel values in the word are always even (0x0, 0x2, ... 0xE).
"""

from typing import Sequence, Tuple

# 8-bit RGB intensities for each 3-bit hardware level.
#
# LINEAR_LEVELS spreads the 8 levels evenly over 0-255. It is the common
# convention used by most tools and emulators. It matches how artists
# usually pick colors from a "Mega Drive palette" in an image editor.
LINEAR_LEVELS: Tuple[int, ...] = tuple(round(i * 255 / 7) for i in range(8))
# -> (0, 36, 73, 109, 146, 182, 219, 255)

# NONLINEAR_LEVELS approximates the measured output of the real VDP DAC
# in normal (non-shadow/highlight) mode. Use it if your source art was
# drawn against real-hardware captures or an accurate emulator.
NONLINEAR_LEVELS: Tuple[int, ...] = (0, 52, 87, 116, 144, 172, 206, 255)


def _nearest_level(value: int, levels: Sequence[int]) -> int:
    """Return the index (0-7) of the level closest to an 8-bit value.

    Ties resolve to the lower level, which keeps the result deterministic.
    """
    best_index = 0
    best_dist = abs(value - levels[0])
    for index in range(1, len(levels)):
        dist = abs(value - levels[index])
        if dist < best_dist:
            best_index = index
            best_dist = dist
    return best_index


def _check_channel(name: str, value: int) -> None:
    if not isinstance(value, int) or isinstance(value, bool):
        raise TypeError(f"{name} must be an int, got {type(value).__name__}")
    if not 0 <= value <= 255:
        raise ValueError(f"{name} must be in range 0-255, got {value}")


def rgb_to_md(r: int, g: int, b: int,
              levels: Sequence[int] = LINEAR_LEVELS) -> int:
    """Convert a 24-bit RGB color to the closest Mega Drive CRAM color word.

    Each channel is mapped on its own to the nearest of the 8 hardware
    levels. The color space is separable per channel, so this also gives
    the closest color by Euclidean RGB distance.

    Args:
        r, g, b: Channel intensities, 0-255.
        levels:  Eight ascending 8-bit intensities for hardware levels 0-7.
                 Defaults to LINEAR_LEVELS. Pass NONLINEAR_LEVELS to match
                 real-hardware DAC output.

    Returns:
        16-bit CRAM word in the form 0000BBB0GGG0RRR0 (e.g. 0x0EEE = white).

    Raises:
        TypeError:  If a channel is not an int.
        ValueError: If a channel is out of range or `levels` is malformed.
    """
    _check_channel("r", r)
    _check_channel("g", g)
    _check_channel("b", b)
    if len(levels) != 8:
        raise ValueError(f"levels must contain 8 entries, got {len(levels)}")

    r3 = _nearest_level(r, levels)
    g3 = _nearest_level(g, levels)
    b3 = _nearest_level(b, levels)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def md_to_rgb(word: int,
              levels: Sequence[int] = LINEAR_LEVELS) -> Tuple[int, int, int]:
    """Convert a CRAM color word back to 24-bit RGB (handy for previews/tests).

    Unused bits are ignored.
    """
    if not isinstance(word, int) or isinstance(word, bool):
        raise TypeError(f"word must be an int, got {type(word).__name__}")
    if len(levels) != 8:
        raise ValueError(f"levels must contain 8 entries, got {len(levels)}")

    r3 = (word >> 1) & 0x7
    g3 = (word >> 5) & 0x7
    b3 = (word >> 9) & 0x7
    return levels[r3], levels[g3], levels[b3]


__all__ = [
    "LINEAR_LEVELS",
    "NONLINEAR_LEVELS",
    "rgb_to_md",
    "md_to_rgb",
]
