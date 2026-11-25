# A tiny gopher server for board tests (Ф5 pictures): / = a menu of the files of ROOT, /<name> = the file.
# python3 tools/testsrv/gopher_srv.py ROOT HOST PORT (HOST = this PC as the board sees it); stop it after the test.
import os, socketserver, sys
ROOT, HOST, PORT = sys.argv[1], sys.argv[2], int(sys.argv[3])
class H(socketserver.StreamRequestHandler):
    def handle(self):
        sel = self.rfile.readline().strip().decode('latin1', 'replace')
        print('request', repr(sel), flush=True)
        if sel in ('', '/'):
            body = 'iTest pictures\tfake\t(NULL)\t0\r\n'
            for n in sorted(os.listdir(ROOT)):
                body += 'I%s (%d)\t/%s\t%s\t%d\r\n' % (n, os.path.getsize(os.path.join(ROOT, n)), n, HOST, PORT)
            self.wfile.write((body + '.\r\n').encode())
        else:
            p = os.path.join(ROOT, os.path.basename(sel))
            if os.path.isfile(p): self.wfile.write(open(p, 'rb').read())
socketserver.ThreadingTCPServer.allow_reuse_address = True
socketserver.ThreadingTCPServer(('0.0.0.0', PORT), H).serve_forever()
