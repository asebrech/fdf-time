"""Serve a GIF directory to the Rebble dev-portal page.

Both header groups below are load-bearing; without them the browser fetch
hangs with no error and the tab freezes. See tools/rebble_screenshots.js.

Usage: serve_gifs.py <gifs_dir>   (listens on 127.0.0.1:8733)
"""
import http.server
import os
import socketserver

import sys
D = sys.argv[1] if len(sys.argv) > 1 else "."


class H(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"  # real Content-Length, keep-alive

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "*")
        # Chrome's Private Network Access: an HTTPS page reaching a loopback
        # address needs this or the fetch hangs forever with no error, which
        # froze the renderer and looked like a dead server.
        self.send_header("Access-Control-Allow-Private-Network", "true")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, *a):
        pass


os.chdir(D)
# Threading: a single-threaded server deadlocks as soon as the browser holds
# a connection open.
srv = socketserver.ThreadingTCPServer(("127.0.0.1", 8733), H)
srv.daemon_threads = True
srv.allow_reuse_address = True
srv.serve_forever()
