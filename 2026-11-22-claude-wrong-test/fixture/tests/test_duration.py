import unittest

from textkit.duration import parse_duration


class ParseDurationTest(unittest.TestCase):
    def test_hours_and_minutes(self):
        self.assertEqual(parse_duration("1h30m"), 5400)

    def test_bad_unit(self):
        with self.assertRaises(ValueError):
            parse_duration("10x")

    def test_bare_number(self):
        self.assertEqual(parse_duration("90"), 90)
