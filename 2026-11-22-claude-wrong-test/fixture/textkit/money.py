from decimal import Decimal, ROUND_HALF_UP


def round_money(amount) -> Decimal:
    """Round an amount to cents, half away from zero.

    The amount is read from its decimal text, so round_money(2.675) is Decimal("2.68") even though the float
    2.675 is stored as 2.67499... Negative amounts round away from zero too: -2.675 gives Decimal("-2.68").

    >>> round_money("1.005")
    Decimal('1.01')
    """
    return Decimal(str(amount)).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)
