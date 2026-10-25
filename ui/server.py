"""Project web UI: loopback HTTP and a serialized adapter to the C++ core."""
import argparse
import json
import os
import subprocess
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

HERE = Path(__file__).resolve().parent
PROJECT = Path(os.environ.get("LOGISTICS_PROJECT", HERE.parent)).resolve()
PORT = 8765

def bridge_binary():
    configured = os.environ.get("LOGISTICS_UI_BRIDGE") or os.environ.get("LOGISTICS_DEMO_BRIDGE")
    if configured:
        return Path(configured).resolve()
    for candidate in (PROJECT / "build/Release/logistics_ui_bridge.exe", PROJECT / "build/logistics_ui_bridge", PROJECT / "build/logistics_ui_bridge.exe"):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError("Build the UI first: cmake -S . -B build && cmake --build build --config Release")

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
                raise RuntimeError("C++ UI process has stopped")
            self.process.stdin.write(json.dumps(body, ensure_ascii=False) + "\n")
            self.process.stdin.flush()
            line = self.process.stdout.readline()
            if not line:
                raise RuntimeError("C++ UI returned no response")
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
        port = self.server.server_port
        if self.headers.get("Host") not in {f"127.0.0.1:{port}", f"localhost:{port}"}:
            self.send_error(403, "Local UI only")
            return False
        origin = self.headers.get("Origin")
        if origin and origin not in {f"http://127.0.0.1:{port}", f"http://localhost:{port}"}:
            self.send_error(403, "Origin is not the local UI")
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
            if not isinstance(body, dict) or body.get("cmd") not in {"state", "reset", "advance", "insert", "traffic"}:
                raise ValueError("Unsupported command")
            self.reply(self.bridge.command(body))
        except (ValueError, TypeError) as error:
            self.reply({"ok": False, "error": str(error)}, 400)
        except Exception as error:
            self.reply({"ok": False, "error": str(error)}, 500)

def run(port=PORT, open_browser=False):
    bridge = Bridge()
    try:
        if not bridge.command({"cmd": "state"}).get("ok"):
            raise RuntimeError("Could not initialize C++ simulation")
        Handler.bridge = bridge
        with ThreadingHTTPServer(("127.0.0.1", port), Handler) as server:
            url = f"http://127.0.0.1:{server.server_port}/"
            print(f"Logistics UI: {url} (Ctrl+C to stop)", flush=True)
            if open_browser:
                import webbrowser
                webbrowser.open(url)
            server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        bridge.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Local logistics web UI")
    parser.add_argument("--port", type=int, default=PORT)
    options = parser.parse_args()
    run(options.port)
