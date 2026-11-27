```python
"""SNES (BGR555) to Sega Mega Drive / Genesis CRAM color conversion.

SNES color word (15-bit BGR555; bit 15 is unused):

    0BBBBBGG GGGRRRRR
    R = bits 0-4, G = bits 5-9, B = bits 10-14   (0-31 per channel)

Mega Drive CRAM word (9-bit BGR333, each channel left-shifted by 1):

    0000BBB0 GGG0RRR0
    R = bits 1-3, G = bits 5-7, B = bits 9-11    (0-7 per channel)

The Mega Drive's video DAC is non-linear. To find the *closest* color,
each SNES channel is expanded to 8-bit intensity and matched against the
commonly cited measured Mega Drive output levels (normal, non-shadow/
highlight mode). The SNES DAC is treated as linear.
"""

__all__ = ["snes_to_md", "MD_LEVELS"]

# Commonly cited measured Mega Drive DAC output levels (0-255) for the
# 3-bit channel values 0..7 in normal mode.
MD_LEVELS = (0, 52, 87, 116, 144, 172, 206, 255)


def _expand5(v: int) -> int:
    """Expand a 5-bit channel value to 8 bits (0-31 -> 0-255)."""
    return (v << 3) | (v >> 2)


def _nearest_md_level(intensity: int) -> int:
    """Return the 3-bit MD channel value whose output is closest to intensity.

    Ties go to the lower value (min() keeps the first minimum).
    """
    return min(range(8), key=lambda i: abs(MD_LEVELS[i] - intensity))


# Precomputed 5-bit SNES channel -> 3-bit MD channel lookup table.
_CHANNEL_LUT = tuple(_nearest_md_level(_expand5(v)) for v in range(32))


def snes_to_md(color: int) -> int:
    """Convert a SNES BGR555 color word to the closest Mega Drive CRAM word.

    Args:
        color: SNES color word, 0x0000-0xFFFF. Bit 15 is unused by the
            SNES and is ignored.

    Returns:
        Mega Drive CRAM color word in the form 0000BBB0GGG0RRR0
        (0x0000-0x0EEE).

    Raises:
        TypeError: if color is not an int.
        ValueError: if color is outside 0x0000-0xFFFF.
    """
    if not isinstance(color, int) or isinstance(color, bool):
        raise TypeError(f"color must be an int, got {type(color).__name__}")
    if not 0 <= color <= 0xFFFF:
        raise ValueError(f"color out of range for a 16-bit word: {color:#x}")

    r = _CHANNEL_LUT[color & 0x1F]
    g = _CHANNEL_LUT[(color >> 5) & 0x1F]
    b = _CHANNEL_LUT[(color >> 10) & 0x1F]

    return (b << 9) | (g << 5) | (r << 1)
```

**Notes**

- **Bit layouts.**
  - The SNES stores red in the low bits, as `0bbbbbgg gggrrrrr`.
  - The Mega Drive CRAM uses `0000bbb0 ggg0rrr0`. Each 3-bit channel sits one bit up from a nibble boundary. So the result is always a value that masks cleanly with `0x0EEE`.
- **Why it uses a lookup table instead of `round(v * 7 / 31)`.** The Mega Drive DAC is non-linear. Its mid-tones are brighter than a linear ramp would give. Matching against the measured levels in `MD_LEVELS` gives visually closer results.
  - If you want the plain linear mapping instead, change `MD_LEVELS` to `tuple(round(i * 255 / 7) for i in range(8))`.
- **Examples:**
  - `snes_to_md(0x7FFF)` → `0x0EEE` (white)
  - `snes_to_md(0x001F)` → `0x000E` (pure red)
  - `snes_to_md(0x7C00)` → `0x0E00` (pure blue)
  - `snes_to_md(0x0000)` → `0x0000` (black)
- **Precision loss.** Going from 32 levels to 8 per channel means different SNES colors can collapse to the same MD color. If palette entries must stay distinct, check for duplicates after converting a palette.