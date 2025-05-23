#!/usr/bin/env python3
"""End-to-end checks for the web-server with echo + static support."""
from __future__ import annotations
import os, signal, socket, subprocess, sys, tempfile, textwrap, time, json
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

def test_crud_sequence(url: str, create_data: str) -> str:
    #Create entity with POST
    res_create = subprocess.run(
        ["curl", "-sS", "-X", "POST", url, "-d", create_data,
         "-H", "Content-Type: application/json"],
        capture_output=True, text=True
    )
    #Check entity creation
    assert res_create.returncode == 0, f"POST failed: {res_create.stderr}"
    response_body = res_create.stdout.split("\r\n\r\n")[-1]
    res_json = json.loads(response_body)

    #Check response id is valid 
    assert "id" in res_json, f"Missing ID in response: {response_body}"
    id_ = res_json["id"]

    #Retrieve the entity with GET
    res_get = subprocess.run(
        ["curl", "-sS", "-X", "GET", f"{url}/{id_}"],
        capture_output=True, text=True
    )
    #Check entity retrieval
    assert res_get.returncode == 0, f"GET failed: {res_get.stderr}"
    body = res_get.stdout.split("\r\n\r\n")[-1]
    #Check response matches data
    assert body.strip() == create_data, f"GET data mismatch: {body.strip()} != {create_data}"

    #Delete the entity
    res_delete = subprocess.run(
        ["curl", "-sS", "-X", "DELETE", f"{url}/{id_}"],
        capture_output=True, text=True
    )
    #Check delete validity
    assert res_delete.stdout.find("200 OK") == 0, f"DELETE failed: {res_delete.stderr}"

    #Attempt to retrieve again and expect 400
    res_get2 = subprocess.run(
        ["curl", "-sS", "-X", "GET", f"{url}/{id_}"],
        capture_output=True, text=True
    )
    #Output should be 400 since entity was deleted
    assert res_get2.stdout.find("400 Bad Request") == 0, f"GET failed: {res_get2.stderr}"
    return res_get2.stdout.replace("\r\n", "\n")

# ───── expected replies ──────────────────────────────────────────────────────
def echo_200(port: int) -> str:
    req = f"GET /echo HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n\r\n"
    ln = len(req)
    return ( "HTTP/1.1 200 OK\n"
             "Content-Type: text/plain\n"
             f"Content-Length: {ln}\n"
             "Connection: close\n"
             "\n" + req.replace("\r\n", "\n") )

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

NOT_FOUND_404 = textwrap.dedent("""\
    HTTP/1.1 404 Not Found
    Content-Type: text/plain
    Content-Length: 72
    Connection: close

    404 Not Found: The requested resource could not be found on this server.""").replace("\r\n", "\n")

BAD_REQUEST_404 = textwrap.dedent("""\
HTTP/1.1 404 Not Found
Content-Type: text/plain
Content-Length: 25
Connection: close

404 Error: File not found""").replace("\r\n", "\n")

OK_REQUEST_200 = textwrap.dedent("""\
    HTTP/1.1 200 OK
    Content-Type: text/plain
    Content-Length: 2
    Connection: close

    OK""").replace("\r\n", "\n")

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
        data_root = Path(tmp) / "crud"
        data_root.mkdir()
        txt_body = "Hello, integration!"
        html_body = "<html><body><h1>Test Page</h1></body></html>"
        jpg_bytes = b"\xff\xd8\xff\xe0" + b"FakeJPEG" + b"\xff\xd9"
        json_body = '{"message": "Hello, IntegrationWebserver!"}'

        (stat_root / "hello.txt").write_text(txt_body)
        (stat_root / "index.html").write_text(html_body)
        (stat_root / "image.jpg").write_bytes(jpg_bytes)

        cfg = Path(tmp) / "test.conf"
        cfg.write_text(textwrap.dedent(f"""\
            port {port};

            location /echo EchoHandler {{
            }}

            location /static StaticHandler {{
                root {stat_root};  # ← absolute path to tmp/static
            }}

            location /public StaticHandler {{
                root {stat_root};
            }}

            location /api CrudApiHandler {{
                root {data_root};
            }}

            location /health HealthHandler {{
            }}

            #Handle requests that don't match any other handler with 404
            location / NotFoundHandler {{
            }}
        """))

        srv = subprocess.Popen(
            [str(SERVER_BIN), str(cfg)],
            cwd=tmp,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        try:
            if not wait_listening(port):
                print("[integration] server never opened port", file=sys.stderr)
                print(srv.stderr.read().decode())
                return 1

            cases = [
                Case("echo",
                     lambda p: raw(p,
                       f"GET /echo HTTP/1.1\r\nHost: 127.0.0.1:{p}\r\n\r\n"),
                     echo_200),
                Case("static file",
                     lambda p: curl(f"http://127.0.0.1:{p}/static/hello.txt"),
                     lambda _p: file_200(txt_body)),
                Case("bad verb",
                     lambda p: raw(p,
                       f"BAD / HTTP/1.1\r\nHost: 127.0.0.1:{p}\r\n\r\n"),
                     NOT_FOUND_404),
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
                Case("Crud entity creation, retrieval, deletion",
                    lambda p: test_crud_sequence(f"http://127.0.0.1:{p}/api/test.json", json_body),
                    "400 Bad Request: ID does not exist"), 
                Case("health check",
                    lambda p: curl(f"http://127.0.0.1:{p}/health"),
                    OK_REQUEST_200),

                #Test paths that should fall through to the NotFoundHandler at '/'
                Case("root path falls through to NotFoundHandler",
                    lambda p: curl(f"http://127.0.0.1:{p}/"),
                    NOT_FOUND_404),
                Case("unmapped path falls through to NotFoundHandler",
                    lambda p: curl(f"http://127.0.0.1:{p}/this_path_does_not_exist"),
                    NOT_FOUND_404),
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
