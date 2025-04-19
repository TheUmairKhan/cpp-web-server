#!/usr/bin/env python3
"""
integration_echo.py ― CS 130 · Assignment 3
------------------------------------------------
Purpose
-------
Run the *real* web‑server binary, talk to it over TCP,
and fail the build if the response differs from what a
browser (curl) should receive.

What it checks
--------------
1. Server starts and binds to a free port from a throw‑away
   config file.
2. A simple **GET /** request returns **HTTP/1.1 200 OK**.
3. **Every header** (Content‑Type, Content‑Length, Connection) matches.
4. Body exactly equals the request the server received.
5. Server is shut down no matter what (no orphan processes).
"""
import os, re, signal, socket, subprocess, sys, tempfile, time
from pathlib import Path

# --------------------------------------------------------------------------
# 1. Locate compiled server binary
# --------------------------------------------------------------------------
REPO_ROOT = Path(__file__).resolve().parent.parent      # project root
BIN_PATH  = REPO_ROOT / "build" / "bin" / "webserver"   # default CMake path
if not BIN_PATH.is_file():
    sys.exit(f"[integration] cannot find {BIN_PATH}")

# --------------------------------------------------------------------------
# 2. Utility: grab an unused TCP port
# --------------------------------------------------------------------------
def pick_free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))                        # 0 → free port
        return s.getsockname()[1]

# --------------------------------------------------------------------------
# 3. Main test routine
# --------------------------------------------------------------------------
def main() -> int:
    port = pick_free_port()

    with tempfile.TemporaryDirectory() as tmp:
        # 3a.  create minimal config (just the port)
        cfg = Path(tmp) / "test.conf"
        cfg.write_text(f"port {port};\n")

        # 3b.  launch server in background
        server = subprocess.Popen(
            [str(BIN_PATH), str(cfg)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )

        try:
            # 3c.  wait ≤5 s for port to accept connections
            for _ in range(50):
                try:
                    with socket.create_connection(("127.0.0.1", port), 0.1):
                        break
                except OSError:
                    time.sleep(0.1)
            else:
                sys.stderr.write("[integration] server never opened port\n")
                return 1

            # 3d.  send deterministic GET (suppress curl's extra headers)
            curl_cmd = [
                "curl", "-sS", "-i", f"http://127.0.0.1:{port}/",
                "-H", "Expect:", "-H", "User-Agent:", "-H", "Accept:"
            ]
            res = subprocess.run(curl_cmd, capture_output=True, text=True)
            if res.returncode:
                sys.stderr.write(res.stderr)
                return 1
            actual = res.stdout.replace("\r\n", "\n")   # normalise newlines

            # ---------- build expected full response -----------------------
            request = f"GET / HTTP/1.1\nHost: 127.0.0.1:{port}\n\n"
            # server calculates length using CRLF (2 bytes each), so +count("\n")
            wire_len = len(request) + request.count("\n")
            expected = (
                "HTTP/1.1 200 OK\n"
                "Content-Type: text/plain\n"
                f"Content-Length: {wire_len}\n"
                "Connection: close\n"
                "\n" +
                request
            )
            # ----------------------------------------------------------------

            if actual != expected:
                sys.stderr.write("[integration] full response mismatch\n")
                sys.stderr.write("----- expected -----\n" + expected)
                sys.stderr.write("----- actual -------\n" + actual)
                return 1

            print("✓ integration_echo – passed")
            return 0

        finally:
            # 3f.  always terminate server cleanly
            server.send_signal(signal.SIGTERM)
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()

# --------------------------------------------------------------------------
if __name__ == "__main__":
    sys.exit(main())
