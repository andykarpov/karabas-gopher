#!/usr/bin/env python3
"""PC side of the USB serial backend: answers like an ESP8266 with the stock AT firmware, the TCP
connections are made by the PC. The browser runs the same AT driver as for a real ESP8266.

  at_proxy.py --tty /dev/serial/by-id/usb-Karabas_Go_...-if02 [--baud 115200] [-v]
  at_proxy.py --pty          # a pseudo terminal for local tests, its name is printed

Use the clean port of the board (CDC if02 or a USB-UART adapter): CDC if00 eats AT+KARABAS lines.

Subset (AT 1.7 / 2.x style): AT, ATE0/1, AT+GMR, AT+CWMODE, AT+CWJAP? / =, AT+CWQAP,
AT+CWAUTOCONN, AT+CWLAP, AT+CIFSR, AT+CIPSTA?, AT+CIPMUX, AT+CIPSTART, AT+CIPSEND, AT+CIPCLOSE,
AT+CIPSTATUS, AT+CIPRECVMODE (0 active +IPD,n:data / 1 passive +IPD,n + AT+CIPRECVDATA),
AT+CIPDNS / CIPSNTPCFG / UART_CUR (accepted, no effect). AT+RST, AT+RESTORE, AT+UART_DEF and
AT+SYSSTORE are refused with ERROR and a loud log line: the browser must never send them.
"""
import argparse, os, re, select, socket, sys, termios, time, tty

VERSION = ('AT version:1.7.4.0(karabas at_proxy)\r\nSDK version:3.0.4\r\n'
           'compile time:Nov 25 2026\r\nBin version(Wroom 02):1.7.4\r\n')
FORBIDDEN = ('AT+RST', 'AT+RESTORE', 'AT+UART_DEF', 'AT+SYSSTORE')
HOST_SSID = 'host-network'


class Link:
    def __init__(s, sock):
        s.sock, s.buf, s.closed = sock, bytearray(), False


class AtModem:
    """Byte-level AT engine; out(bytes) sends to the Z80 side, connect(host, port) -> socket."""

    def __init__(s, out, connect=None, log=None):
        s.out = out
        s.connect = connect or (lambda h, p: socket.create_connection((h, p), timeout=10))
        s.log = log or (lambda m: None)
        s.line = bytearray()
        s.echo, s.mux, s.passive, s.joined, s.skip_lf = True, False, False, True, False
        s.links = {}
        s.send_id, s.send_left, s.send_buf = None, 0, bytearray()

    # ---- Z80 -> modem
    def feed(s, data):
        for b in data:
            if s.skip_lf:                        # CR LF ends a command: the LF is not data
                s.skip_lf = False
                if b == 0x0A:
                    if s.echo:
                        s.out(b'\n')
                    continue
            if s.send_left:
                s.send_buf.append(b); s.send_left -= 1
                if not s.send_left:
                    s._send_done()
                continue
            if s.echo:
                s.out(bytes([b]))
            if b == 0x0A:
                continue
            if b == 0x0D:
                s.skip_lf = True
                cmd = s.line.decode('latin-1').strip(); s.line.clear()
                if cmd:
                    s._command(cmd)
                continue
            if len(s.line) < 512:
                s.line.append(b)

    def _reply(s, text):
        s.out(text.encode('latin-1'))

    def _ok(s, pre=''):
        s._reply(pre + '\r\nOK\r\n')

    def _err(s):
        s._reply('\r\nERROR\r\n')

    def _command(s, cmd):
        s.log('< ' + cmd)
        u = cmd.upper()
        if u.startswith(FORBIDDEN):
            s.log('!!! FORBIDDEN command from the browser: ' + cmd)
            return s._err()
        if u == 'AT':
            return s._ok()
        if u in ('ATE0', 'ATE1'):
            s.echo = u == 'ATE1'; return s._ok()
        if u == 'AT+GMR':
            return s._ok(VERSION)
        if u.startswith('AT+CWMODE'):
            return s._ok('+CWMODE:1\r\n' if u.endswith('?') else '')
        if u == 'AT+CWJAP?':
            if not s.joined:
                return s._ok('No AP\r\n')
            return s._ok('+CWJAP:"%s","00:00:00:00:00:00",1,-40\r\n' % HOST_SSID)
        if u.startswith('AT+CWJAP='):
            s.joined = True
            return s._ok('WIFI CONNECTED\r\nWIFI GOT IP\r\n')
        if u == 'AT+CWQAP':
            s.joined = False; return s._ok('WIFI DISCONNECT\r\n')
        if u.startswith('AT+CWAUTOCONN') or u.startswith('AT+CIPDNS') or u.startswith('AT+CIPSNTPCFG') \
                or u.startswith('AT+UART_CUR') or u.startswith('AT+CWLAPOPT'):
            return s._ok()
        if u == 'AT+CWLAP':
            return s._ok('+CWLAP:(3,"%s",-40,"00:00:00:00:00:00",1)\r\n' % HOST_SSID)
        if u == 'AT+CIFSR':
            return s._ok('+CIFSR:STAIP,"%s"\r\n+CIFSR:STAMAC,"00:00:00:00:00:00"\r\n' % s._ip())
        if u == 'AT+CIPSTA?':
            return s._ok('+CIPSTA:ip:"%s"\r\n' % s._ip())
        if u.startswith('AT+CIPMUX='):
            s.mux = u.endswith('1'); return s._ok()
        if u.startswith('AT+CIPRECVMODE='):
            s.passive = u.endswith('1'); return s._ok()
        if u.startswith('AT+CIPSTART='):
            return s._start(cmd[12:])
        if u.startswith('AT+CIPSEND='):
            return s._send(cmd[11:])
        if u.startswith('AT+CIPCLOSE'):
            lid = int(cmd.split('=')[1]) if '=' in cmd else 0
            l = s.links.pop(lid, None)
            if not l:
                return s._err()
            l.sock.close()
            return s._reply(('%d,CLOSED\r\n' % lid if s.mux else 'CLOSED\r\n') + '\r\nOK\r\n')
        if u.startswith('AT+CIPRECVDATA='):
            return s._recvdata(cmd[15:])
        if u.startswith('AT+CIPRECVLEN?'):
            lens = ','.join(str(len(s.links[i].buf)) if i in s.links else '0' for i in range(5))
            return s._ok('+CIPRECVLEN:%s\r\n' % lens)
        if u == 'AT+CIPSTATUS':
            st = 3 if s.links else 4
            rows = ''.join('+CIPSTATUS:%d,"TCP","0.0.0.0",0,0,0\r\n' % i for i in s.links)
            return s._ok('STATUS:%d\r\n%s' % (st, rows))
        s.log('? unknown: ' + cmd)
        s._err()

    def _ip(s):
        return '192.168.4.2'

    def _args(s, text):
        return [a.strip().strip('"') for a in text.split(',')]

    def _start(s, text):
        a = s._args(text)
        lid = int(a.pop(0)) if s.mux else 0
        if len(a) < 3 or a[0].upper() != 'TCP' or lid in s.links:
            return s._reply('ALREADY CONNECTED\r\n\r\nERROR\r\n' if lid in s.links else '\r\nERROR\r\n')
        try:
            sock = s.connect(a[1], int(a[2]))
        except OSError as e:
            s.log('connect %s:%s failed: %s' % (a[1], a[2], e))
            return s._reply('\r\nERROR\r\nCLOSED\r\n')
        sock.setblocking(False)
        s.links[lid] = Link(sock)
        s._reply(('%d,CONNECT\r\n' % lid if s.mux else 'CONNECT\r\n') + '\r\nOK\r\n')

    def _send(s, text):
        a = s._args(text)
        lid = int(a[0]) if s.mux else 0
        n = int(a[-1])
        if lid not in s.links or not 0 < n <= 2048:
            return s._err()
        s.send_id, s.send_left = lid, n
        s.send_buf.clear()
        s._reply('\r\nOK\r\n> ')

    def _send_done(s):
        l = s.links.get(s.send_id)
        if not l:
            return s._reply('\r\nSEND FAIL\r\n')
        l.sock.setblocking(True)
        l.sock.sendall(bytes(s.send_buf))
        l.sock.setblocking(False)
        s._reply('\r\nRecv %d bytes\r\n\r\nSEND OK\r\n' % len(s.send_buf))

    def _recvdata(s, text):
        a = s._args(text)
        lid = int(a[0]) if s.mux else 0
        n = int(a[-1])
        l = s.links.get(lid)
        if not l:
            return s._err()
        chunk = bytes(l.buf[:n]); del l.buf[:n]
        s.out(b'+CIPRECVDATA:%d,' % len(chunk) + chunk + b'\r\nOK\r\n')
        if l.closed and not l.buf:
            s._closed(lid)

    def _closed(s, lid):
        s.links.pop(lid, None)
        s._reply('%d,CLOSED\r\n' % lid if s.mux else 'CLOSED\r\n')

    # ---- network -> modem (call often)
    def poll(s, max_chunk=1460):
        for lid, l in list(s.links.items()):
            if l.closed:
                continue
            try:
                data = l.sock.recv(max_chunk)
            except BlockingIOError:
                continue
            except OSError:
                data = b''
            if not data:
                l.closed = True
                if not s.passive or not l.buf:
                    s._closed(lid)
                continue
            if s.passive:
                l.buf += data
                s._reply('+IPD,%s%d\r\n' % ('%d,' % lid if s.mux else '', len(l.buf)))
            else:
                s.out(b'+IPD,' + (b'%d,' % lid if s.mux else b'') + b'%d:' % len(data) + data)

    def sockets(s):
        return [l.sock for l in s.links.values() if not l.closed]


def open_tty(path, baud):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY)
    tty.setraw(fd)
    a = termios.tcgetattr(fd)
    speed = getattr(termios, 'B%d' % baud)
    a[4] = a[5] = speed
    a[2] |= termios.CLOCAL | termios.CREAD
    a[2] &= ~termios.HUPCL               # keep DTR up on close (as kg_board.py)
    termios.tcsetattr(fd, termios.TCSANOW, a)
    return fd


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument('--tty')
    g.add_argument('--pty', action='store_true')
    ap.add_argument('--baud', type=int, default=115200)
    ap.add_argument('-v', action='store_true', help='log the commands')
    a = ap.parse_args()
    if a.pty:
        fd, sfd = os.openpty()
        tty.setraw(sfd)
        print('pty:', os.ttyname(sfd), flush=True)
    else:
        fd = open_tty(a.tty, a.baud)
    log = (lambda m: print(time.strftime('%H:%M:%S'), m, file=sys.stderr, flush=True)) if a.v else None
    m = AtModem(lambda b: os.write(fd, b), log=log)
    while True:
        r, _, _ = select.select([fd] + m.sockets(), [], [], 0.05)
        if fd in r:
            m.feed(os.read(fd, 4096))
        m.poll()


if __name__ == '__main__':
    main()
