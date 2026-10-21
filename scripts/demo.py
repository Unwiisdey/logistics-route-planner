"""Run the course demonstration and validate exported results (Python standard library).

Add --plots for PNGs, --gif for a short sampled traffic animation (matplotlib required).
"""
import argparse
import csv
import json
from pathlib import Path
import subprocess
import sys
from plot_traffic import load_traffic

ROOT = Path(__file__).resolve().parents[1]


def find_app():
    for path in [ROOT / "build/Release/logistics_app.exe", ROOT / "build/logistics_app",
                 ROOT / "build/Debug/logistics_app.exe", ROOT / "build/logistics_app.exe"]:
        if path.is_file(): return path
    raise FileNotFoundError("build logistics_app first, or pass --app PATH")


def run_demo(app, output, plots=False, gif=False):
    app, output = Path(app).resolve(), Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    def execute(name, args, expected=0):
        completed = subprocess.run([str(app), *args], cwd=ROOT, text=True, encoding="utf-8", errors="replace", capture_output=True)
        text = completed.stdout + completed.stderr
        (output / f"{name}.txt").write_text(text, encoding="utf-8")
        if completed.returncode != expected:
            raise RuntimeError(f"{name}: expected exit {expected}, got {completed.returncode}\n{text[-2000:]}")
        print(f"PASS {name}")
        return text
    execute("01-network", ["--summary", "--check-size"])
    for strategy in ("distance", "cost"):
        execute(f"02-{strategy}", ["--plan", strategy, "--export", str(output / strategy)])
    common = ["--plan", "distance", "--simulate", "--scenario", "config/orders.json", "--capacity", "100"]
    execute("03-orders", [*common, "--export", str(output / "orders")])
    execute("04-traffic", [*common, "--traffic", "--traffic-seed", "42", "--export", str(output / "traffic")])
    execute("05-invalid-capacity", ["--plan", "distance", "--simulate", "--scenario", "config/orders.json", "--capacity", "1"], expected=1)
    data, frames = load_traffic(output / "traffic")
    final = frames[-1]
    if final["node_id"] != "W01" or final["load"] != 0 or not final["finished"]:
        raise RuntimeError("traffic demonstration did not finish empty at warehouse")
    with (output / "traffic/deliveries.csv").open(encoding="utf-8", newline="") as handle:
        deliveries = list(csv.DictReader(handle))
    if final["delivered_count"] != len(deliveries) or any(row["status"] == "pending" for row in deliveries):
        raise RuntimeError("accepted orders were not completed")
    summary = {"app": app.name, "finished_minutes": final["time"], "delivered": final["delivered_count"],
               "late": final["late_count"], "penalty": final["penalty_points"], "frames": len(frames),
               "traffic_changes": len(data["traffic"]), "active_revision": final["route_revision"]}
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    if plots or gif:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from plot_traffic import create_view
        fig, update = create_view(data, frames)
        selected = min(80, len(frames) - 1)
        update(selected); fig.savefig(output / "traffic.png", dpi=140)
        if gif:
            from matplotlib.animation import FuncAnimation, PillowWriter
            # Sample at most 32 actual recorded frames, including the final state.
            indices = sorted({round(i * (len(frames) - 1) / 31) for i in range(32)})
            animation = FuncAnimation(fig, update, frames=indices, interval=250, repeat=False)
            animation.save(output / "traffic.gif", writer=PillowWriter(fps=4), dpi=75)
        plt.close(fig)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "exports/final-demo")
    parser.add_argument("--plots", action="store_true")
    parser.add_argument("--gif", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(run_demo(args.app or find_app(), args.output, args.plots, args.gif), indent=2))
    except (OSError, ValueError, RuntimeError, ImportError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__": main()
