import json
import subprocess
import unittest
from pathlib import Path
from serve_demo import PROJECT, bridge_binary

HERE = Path(__file__).resolve().parent

class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.process = subprocess.Popen([str(bridge_binary()),
            str(PROJECT / "config/network.json"), str(PROJECT / "config/orders.json")],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, encoding="utf-8")

    def tearDown(self):
        self.process.stdin.close()
        self.process.wait(timeout=5)
        self.process.stdout.close()
        self.process.stderr.close()

    def call(self, **body):
        self.process.stdin.write(json.dumps(body) + "\n")
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise RuntimeError(self.process.stderr.read())
        return json.loads(line)

    def order(self, **kwargs):
        order = dict(id="TEST-01", warehouse="W01", destination="D05", quantity=15,
                     priority="urgent", earliest=480, latest=720)
        order.update(kwargs)
        return self.call(cmd="insert", order=order)

    def test_accept_changes_route_and_completes_pickup_delivery(self):
        initial = self.call(cmd="state")
        accepted = self.order()
        self.assertTrue(accepted["insertion"]["accepted"])
        self.assertIn("D05", accepted["frame"]["future_nodes"])
        self.assertNotEqual(accepted["frame"]["future_nodes"], initial["frame"]["future_nodes"])
        self.assertEqual(accepted["frame"]["load"], initial["frame"]["load"])
        self.assertIsNone(next(o for o in accepted["orders"] if o["id"] == "TEST-01")["pickup"])
        seen_pickup = False
        current = accepted
        for _ in range(120):
            current = self.call(cmd="advance", minutes=5)
            self.assertTrue(current["ok"])
            self.assertLessEqual(current["frame"]["load"], 100)
            new_order = next(o for o in current["orders"] if o["id"] == "TEST-01")
            seen_pickup |= new_order["pickup"] is not None
            if current["frame"]["delivered_count"] == 4 and not current["frame"]["finished"]:
                self.assertEqual(current["task_path"], [{"kind":"return","order_id":None,"node_id":"W01"}])
            if current["frame"]["finished"]:
                break
        self.assertTrue(seen_pickup)
        self.assertTrue(current["frame"]["finished"])
        self.assertEqual(current["frame"]["delivered_count"], 4)
        self.assertEqual(current["frame"]["load"], 0)
        self.assertIsNotNone(new_order["completed"])
        self.assertEqual(current["task_path"], [])
        self.assertIsNone(current["current_task"])

    def test_rejections_preserve_vehicle_route_and_accepted_orders(self):
        baseline = self.call(cmd="state")
        for name, kwargs in [("overload", {"quantity":2000}), ("window", {"latest":480.01,"destination":"D25"})]:
            with self.subTest(name=name):
                result = self.order(id=name, **kwargs)
                self.assertFalse(result["insertion"]["accepted"])
                self.assertEqual(result["frame"]["future_nodes"], baseline["frame"]["future_nodes"])
                self.assertEqual(result["frame"]["load"], baseline["frame"]["load"])
                self.assertEqual(sum(o["accepted"] for o in result["orders"]), 3)
                self.assertEqual(result["task_path"], baseline["task_path"])

    def test_entry_duration_locked_when_current_edge_changes(self):
        initial = self.call(cmd="state")
        segment = initial["segment"]
        changed = self.call(cmd="traffic", road_id=segment["id"], minutes=segment["total"] * 2)
        current_edge = next(e for e in changed["edges"] if e["id"] == segment["id"])
        self.assertEqual(current_edge["time"], segment["total"] * 2)
        self.assertEqual(changed["segment"]["total"], segment["total"])
        self.assertEqual(changed["segment"]["arrival_at"], segment["arrival_at"])
        final = self.call(cmd="advance", minutes=segment["total"])
        self.assertEqual(final["last_trip"]["road_id"], segment["id"])
        self.assertAlmostEqual(final["last_trip"]["actual_minutes"], segment["total"])

    def test_invalid_input_and_duplicate_id_do_not_stop_process(self):
        self.assertFalse(self.order(quantity=-1)["ok"])
        self.assertTrue(self.call(cmd="state")["ok"])
        self.assertTrue(self.order()["insertion"]["accepted"])
        self.assertFalse(self.order()["insertion"]["accepted"])
        self.assertEqual(sum(o["accepted"] for o in self.call(cmd="state")["orders"]), 4)
        self.assertFalse(self.call(cmd="advance",minutes=1000)["ok"])

    def test_task_path_distinguishes_orders_at_same_node_and_survives_traffic(self):
        initial = self.call(cmd="state")
        self.assertEqual(len(initial["task_path"]), 4)
        self.assertEqual(initial["current_task"], initial["task_path"][0])
        self.assertEqual(initial["task_path"][-1]["kind"], "return")
        self.assertTrue(self.order(id="SAME-A", quantity=5)["insertion"]["accepted"])
        inserted = self.order(id="SAME-B", quantity=5)
        self.assertTrue(inserted["insertion"]["accepted"])
        actions = [(t["kind"], t["order_id"]) for t in inserted["task_path"]]
        for order_id in ["SAME-A", "SAME-B"]:
            self.assertLess(actions.index(("pickup", order_id)), actions.index(("delivery", order_id)))
        changed = self.call(cmd="traffic", road_id=inserted["segment"]["id"], minutes=12)
        self.assertEqual(changed["task_path"], inserted["task_path"])
        # Once cargo has been picked up its pickup action disappears, not its delivery.
        for _ in range(40):
            changed = self.call(cmd="advance", minutes=1)
            order = next(o for o in changed["orders"] if o["id"] == "SAME-A")
            if order["pickup"] is not None:
                break
        self.assertIsNotNone(order["pickup"])
        self.assertNotIn({"kind":"pickup","order_id":"SAME-A","node_id":"W01"}, changed["task_path"])
        self.assertIn({"kind":"delivery","order_id":"SAME-A","node_id":"D05"}, changed["task_path"])

    def test_insertion_during_unloading_retains_committed_current_task(self):
        current = self.call(cmd="state")
        for _ in range(600):
            if current["frame"]["status"] == "unloading":
                break
            current = self.call(cmd="advance", minutes=0.5)
        self.assertEqual(current["frame"]["status"], "unloading")
        active = current["frame"]["next_order"]
        inserted = self.order(id="WHILE-UNLOADING", quantity=1, latest=900)
        self.assertTrue(inserted["insertion"]["accepted"])
        self.assertEqual(inserted["current_task"], current["current_task"])
        self.assertEqual(inserted["current_task"]["order_id"], active)
        for _ in range(8):
            advanced = self.call(cmd="advance", minutes=0.5)
            order = next(o for o in advanced["orders"] if o["id"] == active)
            if order["completed"] is not None:
                break
        self.assertIsNotNone(order["completed"])
        self.assertFalse(any(t["order_id"] == active for t in advanced["task_path"]))

if __name__ == "__main__":
    unittest.main(verbosity=2)
