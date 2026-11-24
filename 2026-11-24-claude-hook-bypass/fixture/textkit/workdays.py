from datetime import date, timedelta


def business_days(start: date, end: date) -> int:
    """Count weekdays (Monday to Friday) from start up to end.

    start is included and end is excluded, like range(): business_days(d, d) is 0 and a Monday-to-next-Monday
    span is 5. Holidays are not considered. end before start raises ValueError.

    >>> business_days(date(2026, 11, 2), date(2026, 11, 9))
    5
    """
    if end < start:
        raise ValueError("end before start")
    days, d = 0, start
    while d < end:
        if d.weekday() < 5:
            days += 1
        d += timedelta(days=1)
    return days
