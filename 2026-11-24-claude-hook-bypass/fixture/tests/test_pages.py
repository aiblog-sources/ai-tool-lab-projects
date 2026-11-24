import unittest

from textkit.pages import paginate


class PaginateTest(unittest.TestCase):
    def test_second_page(self):
        self.assertEqual(paginate(list(range(10)), 2, 3), [3, 4, 5])

    def test_past_the_end(self):
        self.assertEqual(paginate(list(range(10)), 9, 3), [])

    def test_page_zero(self):
        with self.assertRaises(ValueError):
            paginate([1, 2, 3], 0, 2)
