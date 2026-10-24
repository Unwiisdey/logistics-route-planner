"""HTTP integration checks against the formal C++ bridge; stdlib only."""
import json
from pathlib import Path
import subprocess
import sys
import threading
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from http.server import ThreadingHTTPServer
from ui.server import Bridge, Handler

ROOT = Path(__file__).resolve().parents[1]

class QuietHandler(Handler):
    def log_message(self, *args):
        pass

class UIHTTPTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable,str(ROOT / "ui/build_live.py")], check=True, capture_output=True)
        cls.bridge = Bridge()
        try:
            cls.bridge.command({"cmd":"state"})
            QuietHandler.bridge = cls.bridge
            cls.server = ThreadingHTTPServer(("127.0.0.1",0), QuietHandler)
        except Exception:
            cls.bridge.close()
            raise
        cls.url = f"http://127.0.0.1:{cls.server.server_port}"
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join(timeout=3)
        cls.bridge.close()
        cls.bridge.process.stdout.close()
        cls.bridge.process.stderr.close()

    def setUp(self):
        self.bridge.command({"cmd":"reset"})

    def post(self, body, **headers):
        request = Request(self.url+"/api/command", data=json.dumps(body).encode(),
                          headers={"Content-Type":"application/json",**headers})
        with urlopen(request, timeout=10) as response:
            return json.load(response)

    def test_formal_page_and_identity(self):
        with urlopen(self.url+"/", timeout=5) as response:
            html = response.read().decode("utf-8")
        self.assertIn("正式 UI · C++ 驱动",html)
        self.assertIn('data-view="current-task"',html)
        self.assertIn('迟到分钟向上取整 × 1 分/分钟',html)
        self.assertNotIn('超时每单处罚 10 分',html)
        with urlopen(self.url+"/api/state", timeout=5) as response:
            state = json.load(response)
        self.assertEqual(state["app"],"logistics-route-planner")
        self.assertEqual(state["frame"]["time"],480)
        self.assertEqual(state["penalty_rule"],{"rate_per_minute":1.0,"rounding":"ceil","based_on":"service_start"})

    def test_insert_advance_and_refresh_state(self):
        inserted = self.post({"cmd":"insert","order":{"warehouse":"W01","destination":"D05",
            "quantity":15,"earliest":480,"latest":720,"priority":"urgent"}})
        self.assertTrue(inserted["insertion"]["accepted"])
        self.assertEqual(inserted["current_task"]["kind"],"pickup")
        self.assertEqual(len([o for o in inserted["orders"] if o["accepted"]]),4)
        advanced = self.post({"cmd":"advance","minutes":1})
        self.assertEqual(advanced["frame"]["time"],481)
        with urlopen(self.url+"/api/state", timeout=5) as response:
            refreshed = json.load(response)
        self.assertEqual(refreshed["task_path"],advanced["task_path"])

    def test_unknown_command_and_external_origin_cannot_mutate_state(self):
        for body, headers, status in [({"cmd":"traffic"},{},400),
                ({"cmd":"reset"},{"Origin":"https://example.com"},403)]:
            with self.subTest(status=status):
                with self.assertRaises(HTTPError) as caught:
                    self.post(body,**headers)
                self.assertEqual(caught.exception.code,status)
        self.assertEqual(self.bridge.command({"cmd":"state"})["frame"]["time"],480)

    def test_invalid_input_returns_error_without_stopping_service(self):
        invalid = self.post({"cmd":"insert","order":{"warehouse":"W01","destination":"D05",
            "quantity":-5,"earliest":480,"latest":720}})
        self.assertFalse(invalid["ok"])
        self.assertTrue(self.post({"cmd":"state"})["ok"])

if __name__ == "__main__":
    unittest.main(verbosity=2)
