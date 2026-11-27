"""Convert SNES BGR555 colors to Sega Mega Drive (Genesis) CRAM color words.

SNES color word (15-bit, bit 15 unused):
    0bbbbbgg gggrrrrr   -> 5 bits per channel, R in bits 0-4

Mega Drive CRAM word (9-bit color in a 16-bit word):
    0000bbb0 ggg0rrr0   -> 3 bits per channel, R in bits 1-3
"""

from typing import Sequence, Tuple

__all__ = ["snes_to_md", "snes_palette_to_md", "MD_DAC_LEVELS"]

# Approximate measured analog output levels (0-255) of the Mega Drive VDP DAC
# for each 3-bit channel value, in normal (non-shadow/highlight) mode.
# The real DAC is not linear, so these give a closer visual match than a
# straight linear scale.
MD_DAC_LEVELS: Tuple[int, ...] = (0, 52, 87, 116, 144, 172, 206, 255)


def _nearest_linear(v5: int) -> int:
    """Map a 5-bit channel (0-31) to the nearest 3-bit channel (0-7), linearly."""
    # round(v5 * 7 / 31) using integer arithmetic (ties round up)
    return (v5 * 14 + 31) // 62


def _nearest_dac(v5: int) -> int:
    """Map a 5-bit channel to the 3-bit value whose DAC level is closest."""
    target = v5 * 255 / 31  # SNES output is effectively linear
    return min(range(8), key=lambda n: abs(MD_DAC_LEVELS[n] - target))


def snes_to_md(color: int, accurate_dac: bool = False) -> int:
    """Convert a SNES BGR555 color word to the closest Mega Drive CRAM word.

    Args:
        color: SNES color word (0x0000-0xFFFF). Bit 15 is ignored, as on
            the SNES hardware.
        accurate_dac: If False (default), channels are scaled linearly from
            5 bits to 3 bits with rounding. If True, each channel is matched
            against the Mega Drive's measured, non-linear DAC output levels,
            which usually looks closer on real hardware/accurate emulators.

    Returns:
        Mega Drive CRAM color word in the form 0000BBB0GGG0RRR0.

    Raises:
        TypeError: If color is not an int.
        ValueError: If color is outside 0x0000-0xFFFF.
    """
    if not isinstance(color, int) or isinstance(color, bool):
        raise TypeError(f"color must be an int, got {type(color).__name__}")
    if not 0 <= color <= 0xFFFF:
        raise ValueError(f"color out of range for a 16-bit word: {color:#x}")

    color &= 0x7FFF  # bit 15 is unused on SNES

    r5 = color & 0x1F
    g5 = (color >> 5) & 0x1F
    b5 = (color >> 10) & 0x1F

    conv = _nearest_dac if accurate_dac else _nearest_linear
    r3, g3, b3 = conv(r5), conv(g5), conv(b5)

    return (b3 << 9) | (g3 << 5) | (r3 << 1)


def snes_palette_to_md(
    colors: Sequence[int], accurate_dac: bool = False
) -> list:
    """Convert a sequence of SNES color words to Mega Drive CRAM words."""
    return [snes_to_md(c, accurate_dac) for c in colors]
