#!/usr/bin/env python3
"""
integration_echo.py — CS 130 Assignment 3

Starts the compiled web‑server exactly once, then runs several request/response
checks.  Exits non‑zero on the first mismatch so CTest/CI marks the build
failed.

NOTE: The server is started once and reused across test cases due to HTTP requests
being independent and no shared state between requests.
"""
from __future__ import annotations
import os, signal, socket, subprocess, sys, tempfile, time, textwrap
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

# -----------------------------------------------------------------------------
# Locate the server binary produced by CMake (`build/bin/webserver`).  Fail
# fast if the build step hasn’t produced it.
# -----------------------------------------------------------------------------
ROOT = Path(__file__).resolve().parent.parent
SERVER_BIN = ROOT / "build" / "bin" / "webserver"
if not SERVER_BIN.is_file():
    sys.exit(f"[integration] binary missing: {SERVER_BIN}")

# -----------------------------------------------------------------------------
# Helpers — short, single‑purpose functions
# -----------------------------------------------------------------------------
def get_free_port() -> int:
    """Ask the OS for an unused TCP port (bind to 0, read the assignment)."""
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]

def wait_until_listening(port: int, timeout: float = 5) -> bool:
    """Poll until something is accepting connections on *port*."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), 0.1):
                return True
        except OSError:
            time.sleep(0.05)
    return False

def curl_get(port: int) -> str:
    """Perform GET / via curl, returning the full response as LF‑terminated text."""
    cmd = [
        "curl", "-sS", "-i", f"http://127.0.0.1:{port}/",
        "-H", "Expect:", "-H", "User-Agent:", "-H", "Accept:"
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode:
        raise RuntimeError(res.stderr)
    return res.stdout.replace("\r\n", "\n")

def raw_request(port: int, request: bytes) -> str:
    """Send *request* bytes over a socket; return reply with LF newlines."""
    with socket.create_connection(("127.0.0.1", port), 2) as sock:
        sock.sendall(request)
        sock.shutdown(socket.SHUT_WR)
        chunks = sock.recv(4096)
        while (chunk := sock.recv(4096)):
            chunks += chunk
    return chunks.replace(b"\r\n", b"\n").decode()

# -----------------------------------------------------------------------------
# Expected‑response helpers keep test‑case definitions clean
# -----------------------------------------------------------------------------
def echo_200(port: int) -> str:
    """Exact 200‑OK response for a minimal GET / request."""
    req = f"GET / HTTP/1.1\nHost: 127.0.0.1:{port}\n\n"
    length = len(req) + req.count("\n")          # CRLF on wire
    return ( "HTTP/1.1 200 OK\n"
             "Content-Type: text/plain\n"
             f"Content-Length: {length}\n"
             "Connection: close\n"
             "\n" + req )

BAD_REQUEST_400 = textwrap.dedent("""\
    HTTP/1.1 400 Bad Request
    Content-Length: 0
    Connection: close

""").replace("\r\n", "\n")

# -----------------------------------------------------------------------------
# Dataclass describes one test scenario; keeps main() readable
# -----------------------------------------------------------------------------
@dataclass
class TestCase:
    name: str
    send: Callable[[int], str]        # function(port) -> actual_response
    expect: Callable[[int], str] | str

    def run(self, port: int) -> bool:
        actual = self.send(port)
        expected = self.expect(port) if callable(self.expect) else self.expect
        if actual == expected:
            print(f"✓ {self.name}")
            return True
        print(f"[integration] {self.name} FAILED", file=sys.stderr)
        print("----- expected -----\n", expected, sep="")
        print("----- actual -------\n", actual, sep="")
        return False

# -----------------------------------------------------------------------------
# Test suite — append new cases here, nothing else to touch
# -----------------------------------------------------------------------------
CASES: list[TestCase] = [
    TestCase(
        "valid GET /",
        curl_get,                 
        echo_200
    ),
    TestCase(
        "invalid method BAD /", 
        lambda p: raw_request(
            p, 
            f"BAD / HTTP/1.1\r\nHost: 127.0.0.1:{p}\r\n\r\n".encode()
        ), 
        BAD_REQUEST_400),
]

# -----------------------------------------------------------------------------
def main() -> int:
    port = get_free_port()

    # Write throw‑away config file
    with tempfile.TemporaryDirectory() as tmp:
        conf = Path(tmp) / "test.conf"
        conf.write_text(f"port {port};\n")

        # Launch server once, reuse for all cases
        server = subprocess.Popen([str(SERVER_BIN), str(conf)],
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            if not wait_until_listening(port):
                print("[integration] server never opened port", file=sys.stderr)
                return 1

            # Execute each test, abort on first failure
            for case in CASES:
                if not case.run(port):
                    return 1

            print("✓ all integration cases passed")
            return 0

        finally:
            # Guaranteed cleanup — even if a test raises
            server.send_signal(signal.SIGTERM)
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()

# -----------------------------------------------------------------------------
if __name__ == "__main__":
    sys.exit(main())
