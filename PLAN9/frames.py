#!/usr/bin/env python3
"""frames.py - on the host: the Plan 9 guest's own frame for a browser.

    python3 frames.py [port]        (default 8790, on 127.0.0.1 only)

GET /frame.png   the guest's screen now: a screendump through the QEMU monitor
                 (run/monitor, as console.py does), as PNG; a frame younger than
                 a tenth of a second is served again rather than dumped twice
GET /vtt/<path>  a track from unbound/VTT (the narrative, the cue tracks)

Nothing in the guest changes; this only reads its screen. Every answer allows
any origin, so boxedPINE's space/guest.html can read it from its own server."""
import os, socket, sys, threading, time, io
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler

HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, 'run')
VTT = os.path.join(os.path.dirname(HERE), 'VTT')
lock = threading.Lock()
last = {'at': 0.0, 'png': b''}

def dump():
    """One screendump through the monitor, as PNG bytes."""
    from PIL import Image
    ppm = os.path.join(RUN, 'frame.ppm')
    with lock:
        if time.monotonic() - last['at'] < 0.1 and last['png']:
            return last['png']
        before = os.path.getmtime(ppm) if os.path.exists(ppm) else 0
        s = socket.socket(socket.AF_UNIX)
        s.connect(os.path.join(RUN, 'monitor'))
        s.send(f'screendump {ppm}\n'.encode())
        for _ in range(100):                      # wait for the new dump, at most a second
            if os.path.exists(ppm) and os.path.getmtime(ppm) != before and os.path.getsize(ppm) > 0:
                break
            time.sleep(0.01)
        s.close()
        time.sleep(0.02)                          # let QEMU finish writing
        out = io.BytesIO()
        Image.open(ppm).save(out, 'PNG', compress_level=1)
        last['at'], last['png'] = time.monotonic(), out.getvalue()
        return last['png']

class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, kind):
        self.send_response(code)
        self.send_header('Content-Type', kind)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Cache-Control', 'no-store')
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split('?')[0]
        if path == '/frame.png':
            try:
                return self.send(200, dump(), 'image/png')
            except Exception as e:
                return self.send(503, f'no frame: {e}\n'.encode(), 'text/plain')
        if path.startswith('/vtt/'):
            f = os.path.realpath(os.path.join(VTT, path[5:]))
            if f.startswith(VTT + os.sep) and f.endswith('.vtt') and os.path.isfile(f):
                return self.send(200, open(f, 'rb').read(), 'text/vtt')
        self.send(404, b'not here\n', 'text/plain')

    def log_message(self, *args):
        pass

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8790
    print(f'frames.py: http://127.0.0.1:{port}/frame.png', flush=True)
    ThreadingHTTPServer(('127.0.0.1', port), Handler).serve_forever()
