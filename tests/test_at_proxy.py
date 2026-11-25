import os, select, socket, sys, threading, time, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from at_proxy import AtModem

MENU = b'iHello\tfake\t(NULL)\t0\r\n1Docs\t/docs\tlocalhost\t70\r\n.\r\n'


class Gopher(threading.Thread):
    """One-shot gopher server on localhost: reads a selector line, sends MENU, closes."""
    def __init__(s):
        super().__init__(daemon=True)
        s.srv = socket.socket(); s.srv.bind(('127.0.0.1', 0)); s.srv.listen(1)
        s.port, s.selector = s.srv.getsockname()[1], None

    def run(s):
        c, _ = s.srv.accept()
        data = b''
        while not data.endswith(b'\r\n'):
            data += c.recv(256)
        s.selector = data
        c.sendall(MENU); c.close()


class Harness:
    def __init__(s, **kw):
        s.rx = bytearray()
        s.m = AtModem(s.rx.extend, **kw)

    def cmd(s, line, until=b'OK\r\n', data=None, timeout=3):
        s.rx.clear()
        if line is not None:
            s.m.feed(line.encode() + b'\r\n')
        if data is not None:
            s.m.feed(data)
        t = time.time()
        while until not in s.rx and time.time() - t < timeout:
            socks = s.m.sockets()
            if socks:
                select.select(socks, [], [], 0.02)
            s.m.poll()
        return bytes(s.rx)


class TestAtProxy(unittest.TestCase):
    def test_basic(self):
        h = Harness()
        self.assertIn(b'OK', h.cmd('AT'))
        self.assertIn(b'OK', h.cmd('ATE0'))
        self.assertEqual(h.cmd('AT'), b'\r\nOK\r\n')
        self.assertIn(b'AT version:', h.cmd('AT+GMR'))
        self.assertIn(b'+CWJAP:"host-network"', h.cmd('AT+CWJAP?'))

    def test_forbidden(self):
        log = []
        h = Harness(log=log.append)
        for c in ('AT+RST', 'AT+RESTORE', 'AT+UART_DEF=115200,8,1,0,2'):
            self.assertIn(b'ERROR', h.cmd(c, until=b'ERROR'))
        self.assertEqual(sum('FORBIDDEN' in m for m in log), 3)

    def _fetch(self, passive):
        g = Gopher(); g.start()
        h = Harness(); h.cmd('ATE0')
        if passive:
            h.cmd('AT+CIPRECVMODE=1')
        self.assertIn(b'CONNECT', h.cmd('AT+CIPSTART="TCP","127.0.0.1",%d' % g.port))
        self.assertIn(b'> ', h.cmd('AT+CIPSEND=7', until=b'> '))
        r = h.cmd(None, until=b'SEND OK', data=b'/docs\r\n')
        self.assertIn(b'SEND OK', r)
        got = bytearray()
        if passive:
            r += h.cmd(None, until=b'+IPD,%d\r\n' % len(MENU))
            self.assertIn(b'+IPD,%d\r\n' % len(MENU), r)
            r = h.cmd('AT+CIPRECVDATA=2048')
            hdr = b'+CIPRECVDATA:%d,' % len(MENU)
            self.assertIn(hdr + MENU, r)
            self.assertIn(b'CLOSED', r + h.cmd(None, until=b'CLOSED'))
            got += MENU
        else:
            r += h.cmd(None, until=b'CLOSED')
            i = r.index(b'+IPD,')
            n = int(r[i + 5:r.index(b':', i)])
            got += r[r.index(b':', i) + 1:][:n]
        g.join(2)
        self.assertEqual(g.selector, b'/docs\r\n')
        self.assertEqual(bytes(got), MENU)

    def test_fetch_active(self):
        self._fetch(False)

    def test_fetch_passive(self):
        self._fetch(True)


if __name__ == '__main__':
    unittest.main()
