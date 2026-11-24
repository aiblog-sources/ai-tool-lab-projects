import unittest
from datetime import date

from textkit.workdays import business_days


class BusinessDaysTest(unittest.TestCase):
    def test_weekend_only(self):
        self.assertEqual(business_days(date(2026, 11, 7), date(2026, 11, 8)), 0)

    def test_end_before_start(self):
        with self.assertRaises(ValueError):
            business_days(date(2026, 11, 9), date(2026, 11, 2))

    def test_full_week(self):
        self.assertEqual(business_days(date(2026, 11, 2), date(2026, 11, 9)), 5)
