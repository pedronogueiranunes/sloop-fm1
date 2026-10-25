#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Serve the packaged editor locally on macOS, Windows or Linux, including its audio assets."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
import webbrowser

root = Path(__file__).resolve().parent
page = '/webapp/editor/' if (root / 'webapp/editor/index.html').is_file() else '/editor.html'
handler = partial(SimpleHTTPRequestHandler, directory=str(root))
with ThreadingHTTPServer(('127.0.0.1', 0), handler) as server:
    url = f'http://localhost:{server.server_port}{page}'
    print(f'SLOOP Editor: {url}\nKeep this window open. Press Ctrl+C to close the editor server.', flush=True)
    opened = False
    if sys.platform == 'darwin':
        opened = subprocess.run(['open', '-a', 'Google Chrome', url], check=False, capture_output=True).returncode == 0
    if not opened:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
