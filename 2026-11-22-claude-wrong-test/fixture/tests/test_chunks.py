import unittest

from textkit.chunks import chunk


class ChunkTest(unittest.TestCase):
    def test_even_split(self):
        self.assertEqual(chunk([1, 2, 3, 4], 2), [[1, 2], [3, 4]])

    def test_zero_size(self):
        with self.assertRaises(ValueError):
            chunk([1, 2], 0)

    def test_leftover(self):
        self.assertEqual(chunk([1, 2, 3, 4, 5], 2), [[1, 2], [3, 4], [5]])
