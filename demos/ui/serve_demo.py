"""Loopback-only UI demo and serialized adapter to the existing C++ library."""
import json
import os
import subprocess
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

HERE = Path(__file__).resolve().parent
PROJECT = Path(os.environ.get("LOGISTICS_PROJECT", HERE.parents[1]))
PORT = 8765

def bridge_binary():
    configured = os.environ.get("LOGISTICS_DEMO_BRIDGE")
    if configured:
        return Path(configured).resolve()
    for candidate in (HERE / "bridge-build/Release/demo_bridge.exe", HERE / "bridge-build/demo_bridge", HERE / "bridge-build/demo_bridge.exe"):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Build the demo first: cmake -S . -B bridge-build && cmake --build bridge-build --config Release")

class Bridge:
    def __init__(self):
        self.lock = threading.Lock()
        self.process = subprocess.Popen(
            [str(bridge_binary()),
             str(PROJECT / "config/network.json"), str(PROJECT / "config/orders.json")],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            encoding="utf-8", text=True, bufsize=1,
        )

    def command(self, body):
        with self.lock:
            if self.process.poll() is not None:
                raise RuntimeError("C++ demo process has stopped")
            self.process.stdin.write(json.dumps(body, ensure_ascii=False) + "\n")
            self.process.stdin.flush()
            line = self.process.stdout.readline()
            if not line:
                raise RuntimeError("C++ demo returned no response")
            return json.loads(line)

    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.terminate()
                self.process.wait(timeout=3)

class Handler(SimpleHTTPRequestHandler):
    bridge = None
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(HERE / "dist"), **kwargs)

    def allowed(self):
        if self.headers.get("Host") not in {f"127.0.0.1:{PORT}", f"localhost:{PORT}"}:
            self.send_error(403, "Local demo only")
            return False
        origin = self.headers.get("Origin")
        if origin and origin not in {f"http://127.0.0.1:{PORT}", f"http://localhost:{PORT}"}:
            self.send_error(403, "Origin is not the local demo")
            return False
        return True

    def reply(self, value, status=200):
        content = json.dumps(value, ensure_ascii=False, allow_nan=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(content)

    def do_GET(self):
        if not self.allowed():
            return
        if urlsplit(self.path).path == "/api/state":
            try:
                self.reply(self.bridge.command({"cmd": "state"}))
            except Exception as error:
                self.reply({"ok": False, "error": str(error)}, 500)
        else:
            super().do_GET()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        super().end_headers()

    def do_POST(self):
        if not self.allowed():
            return
        if self.path != "/api/command":
            self.send_error(404)
            return
        if self.headers.get_content_type() != "application/json":
            self.send_error(415)
            return
        try:
            size = int(self.headers.get("Content-Length", "0"))
            if not 0 < size <= 8192:
                raise ValueError("Invalid request length")
            body = json.loads(self.rfile.read(size))
            if not isinstance(body, dict) or body.get("cmd") not in {"state", "reset", "advance", "insert"}:
                raise ValueError("Unsupported command")
            self.reply(self.bridge.command(body))
        except (ValueError, TypeError) as error:
            self.reply({"ok": False, "error": str(error)}, 400)
        except Exception as error:
            self.reply({"ok": False, "error": str(error)}, 500)

def main():
    bridge = Bridge()
    try:
        if not bridge.command({"cmd": "state"}).get("ok"):
            raise RuntimeError("Could not initialize C++ simulation")
        Handler.bridge = bridge
        with ThreadingHTTPServer(("127.0.0.1", PORT), Handler) as server:
            print(f"Dynamic-order demo: http://127.0.0.1:{PORT} (Ctrl+C to stop)", flush=True)
            server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        bridge.close()

if __name__ == "__main__":
    main()
