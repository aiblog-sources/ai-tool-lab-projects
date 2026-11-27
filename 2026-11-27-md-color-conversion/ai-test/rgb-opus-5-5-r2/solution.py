"""
Mega Drive / Genesis color conversion helpers.

CRAM color word layout (9-bit color stored in a 16-bit word):

    bit: 15..12  11..9  8  7..5  4  3..1  0
         0000    BBB    0  GGG   0  RRR   0

Each channel has 3 bits (0-7). The VDP's DAC output is not linear, so
"closest" color is chosen against measured output levels by default.
A linear mapping is also available for tools/emulators that expect it.
"""

from typing import Sequence, Tuple

__all__ = [
    "MD_LEVELS_MEASURED",
    "MD_LEVELS_LINEAR",
    "rgb_to_md",
    "md_to_rgb",
]

# Approximate measured VDP output levels (normal, non-shadow/highlight mode),
# scaled to 0-255. These are the commonly used values from hardware captures.
MD_LEVELS_MEASURED: Tuple[int, ...] = (0, 52, 87, 116, 144, 172, 206, 255)

# Naive linear levels (n * 255 / 7), matching many emulators and editors.
MD_LEVELS_LINEAR: Tuple[int, ...] = tuple(round(n * 255 / 7) for n in range(8))


def _check_channel(name: str, value: int) -> None:
    if not isinstance(value, int) or isinstance(value, bool):
        raise TypeError(f"{name} must be an int, got {type(value).__name__}")
    if not 0 <= value <= 255:
        raise ValueError(f"{name} must be in 0..255, got {value}")


def _nearest_level(value: int, levels: Sequence[int]) -> int:
    """Return the 3-bit index of the level closest to value.

    Ties resolve toward the lower index, keeping results deterministic.
    """
    best_index = 0
    best_dist = abs(value - levels[0])
    for index in range(1, len(levels)):
        dist = abs(value - levels[index])
        if dist < best_dist:
            best_index = index
            best_dist = dist
    return best_index


def rgb_to_md(r: int, g: int, b: int, levels: Sequence[int] = MD_LEVELS_MEASURED) -> int:
    """Convert a 24-bit RGB color to the closest Mega Drive CRAM color word.

    Args:
        r, g, b: Channel values in 0..255.
        levels: Eight output levels (0-255) for the 3-bit values 0..7.
            Defaults to measured hardware levels; pass MD_LEVELS_LINEAR
            for a plain linear mapping.

    Returns:
        CRAM word in the form 0000BBB0GGG0RRR0 (e.g. 0x0EEE for white).

    Because the channels are independent in the MD palette, picking the
    nearest level per channel also minimizes the total squared RGB error.
    """
    _check_channel("r", r)
    _check_channel("g", g)
    _check_channel("b", b)
    if len(levels) != 8:
        raise ValueError("levels must contain exactly 8 values")

    ri = _nearest_level(r, levels)
    gi = _nearest_level(g, levels)
    bi = _nearest_level(b, levels)
    return (bi << 9) | (gi << 5) | (ri << 1)


def md_to_rgb(word: int, levels: Sequence[int] = MD_LEVELS_MEASURED) -> Tuple[int, int, int]:
    """Convert a CRAM color word back to 24-bit RGB (useful for previews)."""
    if not isinstance(word, int) or not 0 <= word <= 0xFFFF:
        raise ValueError(f"word must be an int in 0..0xFFFF, got {word!r}")
    if len(levels) != 8:
        raise ValueError("levels must contain exactly 8 values")
    ri = (word >> 1) & 7
    gi = (word >> 5) & 7
    bi = (word >> 9) & 7
    return levels[ri], levels[gi], levels[bi]
