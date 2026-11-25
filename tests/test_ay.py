"""The C AY players (src/player/stc.c, asc.c) built on the PC (build/ay_stc, build/ay_asc): the
registers of every frame of the synthetic modules in tests/data/ay (tests/mkaymod.py) are the same
as in the reference dumps (*.regs, made by AYFly), over the whole song, its loop and beyond."""
import glob, os, subprocess, unittest
HERE = os.path.dirname(__file__)
BUILD = os.path.join(HERE, '..', 'build')


class TestAy(unittest.TestCase):
    def test_modules(self):
        mods = sorted(glob.glob(os.path.join(HERE, 'data', 'ay', '*.stc')) +
                      glob.glob(os.path.join(HERE, 'data', 'ay', '*.asc')))
        self.assertEqual(len(mods), 6)
        for m in mods:
            ref = open(os.path.splitext(m)[0] + '.regs').read().splitlines()
            p = subprocess.run([os.path.join(BUILD, 'ay_' + m[-3:]), m, str(len(ref))],
                               capture_output=True, text=True)
            self.assertEqual(p.returncode, 0, m)
            got = p.stdout.splitlines()
            for i, (a, b) in enumerate(zip(got, ref)):
                self.assertEqual(a, b, '%s: frame %d' % (os.path.basename(m), i))
            self.assertEqual(len(got), len(ref))
            self.assertIn('flags 80', p.stderr, m)      # the end of the song was flagged


if __name__ == '__main__':
    unittest.main()
