import unittest

from textkit.slug import slugify


class SlugifyTest(unittest.TestCase):
    def test_punctuation(self):
        self.assertEqual(slugify("Hello, World!"), "hello-world")

    def test_strips_ends(self):
        self.assertEqual(slugify("  --Release notes--  "), "release-notes")

    def test_accented_title(self):
        self.assertEqual(slugify("Déjà vu"), "dj-vu")
