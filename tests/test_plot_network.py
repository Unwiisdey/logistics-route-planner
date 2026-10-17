"""CSV consumer tests use only Python's standard library (no matplotlib needed)."""
import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from plot_network import load_data


class PlotDataTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.write("nodes.csv", ["id,type,x,y", "W,warehouse,0,0", "A,delivery_point,2,0"])
        self.write("edges.csv", ["id,from_id,to_id,distance,time,cost,traffic_status", "WA,W,A,2,3,4,clear", "AW,A,W,2,3,4,clear"])
        self.write("route.csv", ["step,node_id,incoming_edge_id,arrival_minutes", "0,W,,480", "1,A,WA,483", "2,W,AW,486"])
        self.write("stops.csv", ["stop_index,role,node_id,route_step", "0,start,W,0", "1,delivery,A,1", "2,return,W,2"])
        self.write("summary.csv", ["strategy,status,warehouse,start,delivery_count,total_distance,total_time,total_cost", "distance,feasible,W,W,1,4,6,8"])

    def write(self, filename, lines):
        (self.directory / filename).write_text("\n".join(lines) + "\n", encoding="utf-8")

    def test_valid(self):
        data = load_data(self.directory)
        self.assertEqual(data["summary"]["total_distance"], 4)
        self.assertEqual(data["route"][-1]["node_id"], "W")

    def test_missing_headers(self):
        self.write("nodes.csv", ["wrong", "value"])
        with self.assertRaisesRegex(ValueError, "columns"):
            load_data(self.directory)

    def test_broken_route(self):
        self.write("route.csv", ["step,node_id,incoming_edge_id,arrival_minutes", "0,W,,480", "1,A,AW,483", "2,W,WA,486"])
        with self.assertRaisesRegex(ValueError, "follow"):
            load_data(self.directory)

    def test_wrong_total(self):
        self.write("summary.csv", ["strategy,status,warehouse,start,delivery_count,total_distance,total_time,total_cost", "distance,feasible,W,W,1,99,6,8"])
        with self.assertRaisesRegex(ValueError, "totals"):
            load_data(self.directory)

    def test_closed_route(self):
        self.write("edges.csv", ["id,from_id,to_id,distance,time,cost,traffic_status", "WA,W,A,2,3,4,closed", "AW,A,W,2,3,4,clear"])
        with self.assertRaisesRegex(ValueError, "closed"):
            load_data(self.directory)

    def test_nonfinite_coordinate(self):
        self.write("nodes.csv", ["id,type,x,y", "W,warehouse,nan,0", "A,delivery_point,2,0"])
        with self.assertRaisesRegex(ValueError, "finite"):
            load_data(self.directory)


if __name__ == "__main__":
    unittest.main()
