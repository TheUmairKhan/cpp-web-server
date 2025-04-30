#!/usr/bin/env python3
"""End-to-end checks for the web-server with echo + static support."""
from __future__ import annotations
import os, signal, socket, subprocess, sys, tempfile, textwrap, time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

# ───── locate the compiled binary ────────────────────────────────────────────
ROOT = Path(__file__).resolve().parent.parent
for sub in ("build_coverage", "build"):
    SERVER_BIN = ROOT / sub / "bin" / "webserver"
    if SERVER_BIN.is_file():
        break
if not SERVER_BIN.is_file():
    sys.exit(f"[integration] binary missing: {SERVER_BIN}")

# ───── helpers ───────────────────────────────────────────────────────────────
def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]

def wait_listening(p: int, timeout=5) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", p), 0.1):
                return True
        except OSError:
            time.sleep(0.05)
    return False

def curl(url: str, binary: bool = False) -> str | bytes:
    res = subprocess.run(
        ["curl", "-sS", "-i", url,
         "-H", "Expect:", "-H", "User-Agent:", "-H", "Accept:"],
        capture_output=True, text=not binary)
    if res.returncode:
        raise RuntimeError(res.stderr.decode() if binary else res.stderr)
    output = res.stdout if binary else res.stdout.replace("\r\n", "\n")
    return output

def raw(port: int, request: str) -> str:
    with socket.create_connection(("127.0.0.1", port), 2) as sock:
        sock.sendall(request.encode())
        sock.shutdown(socket.SHUT_WR)
        data = sock.recv(4096)
        while chunk := sock.recv(4096):
            data += chunk
    return data.replace(b"\r\n", b"\n").decode()

# ───── expected replies ──────────────────────────────────────────────────────
def echo_200(port: int) -> str:
    req = f"GET / HTTP/1.1\nHost: 127.0.0.1:{port}\n\n"
    ln  = len(req) + req.count("\n")
    return ( "HTTP/1.1 200 OK\n"
             "Content-Type: text/plain\n"
             f"Content-Length: {ln}\n"
             "Connection: close\n"
             "\n" + req )

def file_200(body: str, ctype="text/plain") -> str:
    return ( "HTTP/1.1 200 OK\n"
             f"Content-Type: {ctype}\n"
             f"Content-Length: {len(body)}\n"
             "Connection: close\n"
             "\n" + body )

BAD_REQUEST_400 = textwrap.dedent("""\
    HTTP/1.1 400 Bad Request
    Content-Type: text/plain
    Content-Length: 11
    Connection: close

    Bad Request""").replace("\r\n", "\n")

BAD_REQUEST_404 = textwrap.dedent("""\
HTTP/1.1 404 Not Found
Content-Type: text/plain
Content-Length: 25
Connection: close

404 Error: File not found""").replace("\r\n", "\n")

# ───── testcase wrapper ──────────────────────────────────────────────────────
@dataclass
class Case:
    name: str
    send: Callable[[int], str]
    expect: Callable[[int], str] | str
    def run(self, port: int) -> bool:
        got  = self.send(port)
        want = self.expect(port) if callable(self.expect) else self.expect
        ok = got == want
        print(f"{'✓' if ok else '✗'} {self.name}")
        if not ok:
            print("── expected ─────────────────────────────────────\n", want, sep="")
            print("── got ──────────────────────────────────────────\n", got,  sep="")
        return ok

# ───── main driver ───────────────────────────────────────────────────────────
def main() -> int:
    port = free_port()

    with tempfile.TemporaryDirectory() as tmp:
        stat_root = Path(tmp) / "static"
        stat_root.mkdir()
        txt_body = "Hello, integration!"
        html_body = "<html><body><h1>Test Page</h1></body></html>"
        jpg_bytes = b"\xff\xd8\xff\xe0" + b"FakeJPEG" + b"\xff\xd9"

        (stat_root / "hello.txt").write_text(txt_body)
        (stat_root / "index.html").write_text(html_body)
        (stat_root / "image.jpg").write_bytes(jpg_bytes)

        cfg = Path(tmp) / "test.conf"
        cfg.write_text(f"""
            port {port};
            route / {{
              handler echo;
            }}
            route /static {{
              handler static;
              root {stat_root};
            }}
            route /public {{
              handler static;
              root {stat_root};
            }}
        """)

        srv = subprocess.Popen([str(SERVER_BIN), str(cfg)],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            if not wait_listening(port):
                print("[integration] server never opened port", file=sys.stderr)
                print(srv.stderr.read().decode())
                return 1

            cases = [
                Case("echo /",
                     lambda p: raw(p,
                       f"GET / HTTP/1.1\r\nHost: 127.0.0.1:{p}\r\n\r\n"),
                     echo_200),
                Case("static file",
                     lambda p: curl(f"http://127.0.0.1:{p}/static/hello.txt"),
                     lambda _p: file_200(txt_body)),
                Case("bad verb",
                     lambda p: raw(p,
                       f"BAD / HTTP/1.1\r\nHost: 127.0.0.1:{p}\r\n\r\n"),
                     BAD_REQUEST_400),
                Case("static HTML file",
                    lambda p: curl(f"http://127.0.0.1:{p}/static/index.html"),
                    lambda _p: file_200(html_body, ctype="text/html")),
                Case("static JPG file",
                    lambda p: curl(f"http://127.0.0.1:{p}/static/image.jpg", binary=True),
                    lambda _p: (
                        b"HTTP/1.1 200 OK\r\n"
                        b"Content-Type: image/jpeg\r\n"
                        + f"Content-Length: {len(jpg_bytes)}\r\n".encode()
                        + b"Connection: close\r\n\r\n"
                        + jpg_bytes
                    )),
                Case("static file from different route",
                    lambda p: curl(f"http://127.0.0.1:{p}/public/hello.txt"),
                    lambda _p: file_200(txt_body)),
                Case("unknown file",
                    lambda p: curl(f"http://127.0.0.1:{p}/static/missing"),
                    BAD_REQUEST_404),
            ]

            for c in cases:
                if not c.run(port):
                    return 1
            return 0
        finally:
            srv.send_signal(signal.SIGTERM)
            try:
                srv.wait(timeout=3)
            except subprocess.TimeoutExpired:
                srv.kill()

if __name__ == "__main__":
    sys.exit(main())
