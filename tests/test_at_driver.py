"""The C AT driver (src/net/at.c, built as build/at_host) against tools/at_proxy.py over a pty."""
import os, select, socket, subprocess, sys, threading, tty, unittest
HERE = os.path.dirname(__file__)
sys.path.insert(0, os.path.join(HERE, '..', 'tools'))
from at_proxy import AtModem

AT_HOST = os.path.join(HERE, '..', 'build', 'at_host')
BIG = bytes((i * 7 + i // 251) & 0xFF for i in range(40000))


class Server(threading.Thread):
    def __init__(s, reply):
        super().__init__(daemon=True)
        s.srv = socket.socket(); s.srv.bind(('127.0.0.1', 0)); s.srv.listen(1)
        s.port, s.reply, s.selector = s.srv.getsockname()[1], reply, None

    def run(s):
        c, _ = s.srv.accept()
        d = b''
        while not d.endswith(b'\r\n'):
            d += c.recv(256)
        s.selector = d
        c.sendall(s.reply); c.close()


class Modem(threading.Thread):
    def __init__(s):
        super().__init__(daemon=True)
        s.master, slave = os.openpty()
        tty.setraw(slave)
        s.path = os.ttyname(slave)
        s.slave = slave
        s.log = []
        s.m = AtModem(lambda b: os.write(s.master, b), log=s.log.append)
        s.stop = False

    def run(s):
        while not s.stop:
            r, _, _ = select.select([s.master] + s.m.sockets(), [], [], 0.01)
            if s.master in r:
                try:
                    s.m.feed(os.read(s.master, 4096))
                except OSError:
                    return
            s.m.poll()


@unittest.skipUnless(os.path.exists(AT_HOST), 'build/at_host missing (make test builds it)')
class TestAtDriver(unittest.TestCase):
    def run_host(self, *args, timeout=30):
        m = Modem(); m.start()
        try:
            p = subprocess.run([AT_HOST, m.path, *args], capture_output=True, timeout=timeout)
        finally:
            m.stop = True; m.join(1)
        forbidden = [l for l in m.log if 'FORBIDDEN' in l]
        self.assertEqual(forbidden, [])
        return p

    def test_status(self):
        p = self.run_host('status')
        self.assertEqual(p.returncode, 0, p.stdout)
        self.assertIn(b'version=1.7.4.0', p.stdout)
        self.assertIn(b'ssid=host-network', p.stdout)
        self.assertIn(b'ip=192.168.4.2', p.stdout)

    def test_link(self):
        p = self.run_host('link')
        self.assertIn(b'r=0 link=host-network rssi=-40', p.stdout)

    def test_scan_join_forget(self):
        p = self.run_host('scan')
        self.assertIn(b'ap host-network -40 3', p.stdout)
        p = self.run_host('join', 'my "net",x', 'p\\a,ss')
        self.assertIn(b'r=0', p.stdout)
        self.assertEqual(self.run_host('forget').returncode, 0)

    def test_get_menu(self):
        menu = b'iHello\tfake\t(NULL)\t0\r\n1Docs\t/docs\tlocalhost\t70\r\n.\r\n'
        s = Server(menu); s.start()
        p = self.run_host('get', '127.0.0.1', str(s.port), '/docs')
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(p.stdout, menu)
        self.assertEqual(s.selector, b'/docs\r\n')

    def test_get_big(self):
        s = Server(BIG); s.start()
        p = self.run_host('get', '127.0.0.1', str(s.port), '', timeout=60)
        self.assertEqual(p.returncode, 0, p.stderr)
        self.assertEqual(len(p.stdout), len(BIG))
        self.assertEqual(p.stdout, BIG)

    def test_refused(self):
        srv = socket.socket(); srv.bind(('127.0.0.1', 0)); port = srv.getsockname()[1]; srv.close()
        p = self.run_host('get', '127.0.0.1', str(port), '')
        self.assertIn(b'open r=1', p.stdout)


if __name__ == '__main__':
    unittest.main()
