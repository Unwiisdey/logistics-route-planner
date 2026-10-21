"""Traffic replay invariants, independent of matplotlib and the C++ build."""
import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from plot_traffic import load_traffic, state_at


class TrafficPlotTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        tables = {
            "nodes.csv": "id,type,x,y\nW,warehouse,0,0\nA,delivery_point,4,0\nT,transfer_station,4,4\n",
            "edges.csv": "id,from_id,to_id,time,traffic_status\nWA,W,A,4,clear\nAW,A,W,5,clear\nAT,A,T,2,clear\nTW,T,W,2,clear\n",
            "route_revisions.csv": "revision,step,node_id,incoming_edge_id\n1,0,W,\n1,1,A,WA\n1,2,W,AW\n2,0,A,\n2,1,T,AT\n2,2,W,TW\n",
            "remaining_routes.csv": "frame,step,node_id,incoming_edge_id\n0,0,A,\n0,1,W,AW\n1,0,A,\n1,1,T,AT\n1,2,W,TW\n2,0,T,\n2,1,W,TW\n",
            "traffic.csv": "time,edge_id,before,after,before_status,after_status,trigger\n1,AW,5,10,clear,congested,1\n",
            "traversals.csv": "time,edge_id\n4,WA\n6,AT\n8,TW\n",
        }
        for name, text in tables.items(): (self.path / name).write_text(text, encoding="utf-8")
        self.frames = [dict(time=1, clock="00:01", status="driving", node_id="", edge_id="WA", from_id="W", to_id="A",
                            remaining_minutes=3, progress=.25, x=1, y=0, load=2, completed_edges=0, delivered_count=0,
                            late_count=0, penalty_points=0, finished=0, route_revision=1, estimated_return=9,
                            next_order="a", next_arrival=4, traffic_version=0, completed_traversals=0)]
        self.frames.append(dict(self.frames[0], traffic_version=1, estimated_return=8))
        self.frames.append(dict(self.frames[1], time=4, clock="00:04", edge_id="AT", from_id="A", to_id="T",
                                remaining_minutes=2, progress=0, x=4, y=0, load=0, delivered_count=1,
                                route_revision=2, next_order="", next_arrival="", completed_traversals=1))

    def load(self):
        with (self.path / "simulation.csv").open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=self.frames[0].keys())
            writer.writeheader(); writer.writerows(self.frames)
        return load_traffic(self.path)

    def test_same_timestamp_distinct_versions(self):
        data, frames = self.load()
        self.assertEqual(state_at(data, frames, 0)[0]["AW"]["time"], 5)
        self.assertEqual(state_at(data, frames, 1)[0]["AW"]["time"], 10)

    def test_pending_and_active_routes(self):
        data, frames = self.load()
        self.assertEqual([row["node_id"] for row in state_at(data, frames, 1)[1]], ["A", "T", "W"])
        self.assertEqual(frames[1]["edge_id"], "WA")
        self.assertEqual(frames[2]["route_revision"], 2)

    def test_backward_seek(self):
        data, frames = self.load()
        state_at(data, frames, 2)
        self.assertEqual(state_at(data, frames, 0)[0]["AW"]["traffic_status"], "clear")
        self.assertEqual(data["edges"]["AW"]["time"], 5)

    def test_invalid_traffic_chain(self):
        path = self.path / "traffic.csv"
        path.write_text(path.read_text().replace("1,AW,5,10", "1,AW,6,10"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "traffic history"): self.load()

    def test_future_traffic(self):
        self.frames[0].update(time=.5, traffic_version=1)
        with self.assertRaisesRegex(ValueError, "future traffic"): self.load()

    def test_missing_revision(self):
        self.frames[2]["route_revision"] = 7
        with self.assertRaisesRegex(ValueError, "revision"): self.load()

    def test_wrong_position(self):
        self.frames[1]["x"] = 3
        with self.assertRaisesRegex(ValueError, "coordinates"): self.load()

    def test_wrong_pending_anchor(self):
        path = self.path / "remaining_routes.csv"
        path.write_text(path.read_text().replace("1,0,A,\n1,1,T,AT\n1,2,W,TW", "1,0,T,\n1,1,W,TW"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "anchor"): self.load()

    def test_traversals_survive_revision(self):
        data, frames = self.load()
        self.assertEqual(frames[2]["completed_edges"], 0)
        self.assertEqual(data["traversals"][:frames[2]["completed_traversals"]][0]["edge_id"], "WA")
        self.frames[1]["completed_traversals"] = 1
        with self.assertRaisesRegex(ValueError, "completed traversals"): self.load()


if __name__ == "__main__": unittest.main()
