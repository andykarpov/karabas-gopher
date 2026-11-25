import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from mkspg import build


class TestMkspg(unittest.TestCase):
    def test_header_and_blocks(self):
        b0, b1 = bytes(range(256)) * 64, b'\xAA' * 700          # 16384 + 700 bytes
        d = build([(0x20, b0), (0x21, b1)], 0xC000, 0xBFFE, 0x20, 'karabas gopher')
        self.assertEqual(d[0x20:0x2C], b'SpectrumProg')
        self.assertEqual(d[0x2C], 0x10)
        self.assertEqual(struct.unpack_from('<HHB', d, 0x30), (0xC000, 0xBFFE, 0x20))
        self.assertEqual(struct.unpack_from('<H', d, 0x3A)[0], 2)
        self.assertEqual(d[0x100:0x106], bytes([0x00, 31, 0x20, 0x80, 1, 0x21]))
        self.assertEqual(d[0x400:0x400 + 16384], b0)
        self.assertEqual(d[0x400 + 16384:0x400 + 16384 + 700], b1)
        self.assertEqual(len(d), 0x400 + 16384 + 1024)

    def test_limits(self):
        with self.assertRaises(ValueError):
            build([(0x20, bytes(16385))], 0xC000, 0, 0x20)
        with self.assertRaises(ValueError):
            build([(0xE0, b'x')], 0xC000, 0, 0x20)
        with self.assertRaises(ValueError):
            build([(0x20, b'x')], 0x3FFF, 0, 0x20)


if __name__ == '__main__':
    unittest.main()
