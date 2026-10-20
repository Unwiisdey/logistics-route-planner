"""Simulation replay checks using only the Python standard library."""
import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from plot_simulation import load_simulation


class SimulationPlotTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        tables = {
            "nodes.csv": "id,type,x,y\nW,warehouse,0,0\nA,delivery_point,10,0\n",
            "edges.csv": "id,from_id,to_id,distance,time,cost,traffic_status\nWA,W,A,1,5,1,clear\nAW,A,W,1,5,1,clear\n",
            "route.csv": "step,node_id,incoming_edge_id,arrival_minutes\n0,W,,480\n1,A,WA,485\n2,W,AW,490\n",
            "stops.csv": "stop_index,role,node_id,route_step\n0,start,W,0\n1,delivery,A,1\n2,return,W,2\n",
            "summary.csv": "strategy,status,warehouse,start,delivery_count,total_distance,total_time,total_cost\ndistance,feasible,W,W,1,2,10,2\n",
        }
        for name, text in tables.items(): (self.path / name).write_text(text, encoding="utf-8")
        self.rows = [dict(time=482, clock="08:02", status="driving", node_id="", edge_id="WA", from_id="W", to_id="A",
                          remaining_minutes=3, progress=.4, x=4, y=0, load=7, completed_edges=0,
                          delivered_count=0, late_count=0, penalty_points=0, finished=0)]

    def load(self):
        with (self.path / "simulation.csv").open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=self.rows[0].keys())
            writer.writeheader(); writer.writerows(self.rows)
        return load_simulation(self.path)

    def test_position(self):
        _, frames = self.load()
        self.assertEqual(frames[0]["x"], 4)

    def test_wrong_active_edge(self):
        self.rows[0]["edge_id"] = "AW"
        with self.assertRaisesRegex(ValueError, "active route"): self.load()

    def test_wrong_coordinates(self):
        self.rows[0]["x"] = 8
        with self.assertRaisesRegex(ValueError, "coordinates"): self.load()

    def test_time_reversal(self):
        self.rows.append(dict(self.rows[0], time=481))
        with self.assertRaisesRegex(ValueError, "monotonic"): self.load()

    def test_false_completion(self):
        self.rows[0]["finished"] = 1
        with self.assertRaisesRegex(ValueError, "finished"): self.load()

    def test_waiting_position(self):
        self.rows[0].update(status="waiting", node_id="A", edge_id="", from_id="", to_id="", completed_edges=1, progress=0, x=10)
        self.assertEqual(self.load()[1][0]["status"], "waiting")

    def test_nonfinite_state(self):
        self.rows[0]["progress"] = "nan"
        with self.assertRaisesRegex(ValueError, "finite"): self.load()


if __name__ == "__main__": unittest.main()
