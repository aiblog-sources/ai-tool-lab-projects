import unittest
from decimal import Decimal

from textkit.money import round_money


class RoundMoneyTest(unittest.TestCase):
    def test_string_half(self):
        self.assertEqual(round_money("1.005"), Decimal("1.01"))

    def test_negative_string(self):
        self.assertEqual(round_money("-0.125"), Decimal("-0.13"))

    def test_float_input(self):
        self.assertEqual(round_money(2.675), Decimal("2.68"))
