#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serves the GitHub Pages site locally the way .github/workflows/pages.yml lays it out: docs/site at the root,
docs/guide at /guide/. Straight from the files, so an edit shows on reload.
   python3 tools/site/serve.py [port]   (default 8790)"""
import functools
import http.server
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SITE = os.path.join(ROOT, "docs", "site")
GUIDE = os.path.join(ROOT, "docs", "guide")


class Handler(http.server.SimpleHTTPRequestHandler):
    def translate_path(self, path):
        p = path.split("?", 1)[0].split("#", 1)[0]
        if p == "/guide":
            p = "/guide/"
        if p.startswith("/guide/"):
            base, rest = GUIDE, p[len("/guide"):]
        else:
            base, rest = SITE, p
        full = os.path.normpath(os.path.join(base, rest.lstrip("/")))
        return full if full.startswith(base) else base

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8790
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", port), functools.partial(Handler, directory=SITE))
    print(f"Elasticat site on http://127.0.0.1:{port}/", flush=True)
    srv.serve_forever()
