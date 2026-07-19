#!/usr/bin/env python3
# Test HTTP/HTTPS server for vp's network test suite (see net-tests.sh).
# Serves files from --dir plus a set of redirect/framing routes, on three
# listeners: plain http (127.0.0.1), https (127.0.0.1), and http ([::1]).
# Prints READY once everything is bound.

import argparse
import http.server
import os
import socket
import socketserver
import ssl
import sys
import threading


class FastHTTPServer(http.server.HTTPServer):
    """HTTPServer without the server_bind() reverse-DNS lookup.

    http.server.HTTPServer.server_bind() calls socket.getfqdn(), which can
    block for seconds on hosts with slow reverse resolution (observed
    hanging past the readiness timeout on GitHub's macOS runners). We only
    ever bind loopback, so the fqdn is not needed.
    """

    def server_bind(self):
        socketserver.TCPServer.server_bind(self)
        self.server_name = 'localhost'
        self.server_port = self.server_address[1]


class V6Server(FastHTTPServer):
    address_family = socket.AF_INET6


def make_handler(docroot, http_port, https_port):
    class Handler(http.server.SimpleHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=docroot, **kwargs)

        def redirect(self, code, location):
            self.send_response(code)
            if location is not None:
                self.send_header('Location', location)
            self.send_header('Content-Length', '0')
            self.end_headers()

        def do_GET(self):
            routes = {
                # two-hop chain: absolute path, then absolute URL
                '/r1': (302, '/r2'),
                '/r2': (301, 'http://127.0.0.1:%d/test1.ppm' % http_port),
                '/rel': (302, 'test1.ppm'),
                '/proto': (307, '//127.0.0.1:%d/test1.ppm' % http_port),
                '/loop': (302, '/loop'),
                '/noloc': (302, None),
                # http -> https, targeting our own https listener
                '/tls': (301, 'https://127.0.0.1:%d/test1.ppm' % https_port),
                # https -> http downgrade (only meaningful on the tls listener)
                '/down': (302, 'http://127.0.0.1:%d/test1.ppm' % http_port),
            }
            if self.path in routes:
                self.redirect(*routes[self.path])
            elif self.path == '/chunked':
                # hand-rolled chunked framing to exercise the decoder
                with open(os.path.join(docroot, 'test1.ppm'), 'rb') as f:
                    body = f.read()
                self.send_response(200)
                self.send_header('Content-Type', 'image/x-portable-pixmap')
                self.send_header('Transfer-Encoding', 'chunked')
                self.end_headers()
                for i in range(0, len(body), 7):
                    piece = body[i:i + 7]
                    self.wfile.write(b'%x\r\n%s\r\n' % (len(piece), piece))
                self.wfile.write(b'0\r\n\r\n')
            elif self.path == '/early':
                # interim 1xx block before the real response (RFC 9110);
                # written raw so send_response bookkeeping stays untouched
                self.wfile.write(b'HTTP/1.1 103 Early Hints\r\n'
                                 b'Link: </style.css>; rel=preload\r\n\r\n')
                self.path = '/test1.ppm'
                super().do_GET()
            elif self.path == '/':
                # bare-host fetch lands here; serve the image directly
                self.path = '/test1.ppm'
                super().do_GET()
            else:
                super().do_GET()

        def log_message(self, *args):
            pass

    return Handler


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', required=True)
    ap.add_argument('--http-port', type=int, required=True)
    ap.add_argument('--https-port', type=int, required=True)
    ap.add_argument('--v6-port', type=int, required=True)
    ap.add_argument('--cert', required=True)
    ap.add_argument('--key', required=True)
    args = ap.parse_args()

    handler = make_handler(args.dir, args.http_port, args.https_port)

    plain = FastHTTPServer(('127.0.0.1', args.http_port), handler)

    tls = FastHTTPServer(('127.0.0.1', args.https_port), handler)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(args.cert, args.key)
    tls.socket = ctx.wrap_socket(tls.socket, server_side=True)

    servers = [plain, tls]

    # IPv6 loopback is best-effort: some CI runners (notably GitHub's
    # macOS images) have no ::1, and the v6 test skips rather than fails.
    try:
        v6 = V6Server(('::1', args.v6_port), handler)
        servers.append(v6)
        print('V6 ok', flush=True)
    except OSError as e:
        print('V6 unavailable: %s' % e, flush=True)

    for srv in servers:
        threading.Thread(target=srv.serve_forever, daemon=True).start()

    print('READY', flush=True)
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        sys.exit(0)


if __name__ == '__main__':
    main()
