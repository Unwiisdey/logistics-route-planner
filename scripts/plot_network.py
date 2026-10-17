"""Draw a network and its route from the C++ Visualizer CSV export.

Python 3.10+; matplotlib is optional for the C++ application, required here.
No path planning is performed in this script.
"""

import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
import textwrap


def read_table(directory, filename, columns):
    with (Path(directory) / filename).open(encoding="utf-8-sig", newline="") as handle:
        reader = csv.DictReader(handle)
        if not set(columns).issubset(reader.fieldnames or []):
            raise ValueError(f"{filename}: missing required columns")
        rows = list(reader)
        if any(None in row or any(value is None for value in row.values()) for row in rows):
            raise ValueError(f"{filename}: inconsistent row width")
        return rows


def number(value):
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("all numeric values must be finite")
    return result


def load_data(directory):
    nodes = read_table(directory, "nodes.csv", ["id", "type", "x", "y"])
    edges = read_table(directory, "edges.csv", ["id", "from_id", "to_id", "distance", "time", "cost", "traffic_status"])
    route = read_table(directory, "route.csv", ["step", "node_id", "incoming_edge_id", "arrival_minutes"])
    stops = read_table(directory, "stops.csv", ["stop_index", "role", "node_id", "route_step"])
    summaries = read_table(directory, "summary.csv", ["strategy", "status", "warehouse", "start", "delivery_count",
                                                        "total_distance", "total_time", "total_cost"])
    if not nodes or not route or len(summaries) != 1:
        raise ValueError("export must contain nodes, a route and one summary")
    summary = summaries[0]
    if summary["status"] != "feasible" or summary["strategy"] not in {"distance", "cost"}:
        raise ValueError("only completed distance/cost plans can be plotted")
    by_id = {}
    for node in nodes:
        if node["id"] in by_id or node["type"] not in {"warehouse", "delivery_point", "transfer_station"}:
            raise ValueError("duplicate node ID or unknown node type")
        node["x"], node["y"] = number(node["x"]), number(node["y"])
        by_id[node["id"]] = node
    edge_by_id = {}
    for edge in edges:
        if edge["id"] in edge_by_id or edge["from_id"] not in by_id or edge["to_id"] not in by_id:
            raise ValueError("duplicate road ID or missing endpoint")
        for metric in ("distance", "time", "cost"):
            edge[metric] = number(edge[metric])
            if edge[metric] < 0:
                raise ValueError("road weights cannot be negative")
        if edge["traffic_status"] not in {"clear", "congested", "closed"}:
            raise ValueError("unknown traffic status")
        edge_by_id[edge["id"]] = edge
    for index, step in enumerate(route):
        if int(step["step"]) != index or step["node_id"] not in by_id:
            raise ValueError("route steps must be consecutive and reference existing nodes")
        step["arrival_minutes"] = number(step["arrival_minutes"])
        if index == 0:
            if step["incoming_edge_id"]:
                raise ValueError("first route step cannot have an incoming edge")
        else:
            edge = edge_by_id.get(step["incoming_edge_id"])
            if not edge or (edge["from_id"], edge["to_id"]) != (route[index - 1]["node_id"], step["node_id"]):
                raise ValueError("route does not follow exported roads")
            if edge["traffic_status"] == "closed":
                raise ValueError("route uses a closed road")
    if route[0]["node_id"] != summary["start"] or route[-1]["node_id"] != summary["warehouse"]:
        raise ValueError("route start/return does not match summary")
    for index, stop in enumerate(stops):
        step = int(stop["route_step"])
        if int(stop["stop_index"]) != index or not 0 <= step < len(route) or stop["node_id"] != route[step]["node_id"]:
            raise ValueError("stop does not match route")
        if stop["role"] not in {"start", "delivery", "return"}:
            raise ValueError("unknown stop role")
    for metric in ("distance", "time", "cost"):
        summary["total_" + metric] = number(summary["total_" + metric])
        actual = sum(edge_by_id[step["incoming_edge_id"]][metric] for step in route[1:])
        if not math.isclose(actual, summary["total_" + metric], rel_tol=1e-9, abs_tol=1e-9):
            raise ValueError("summary totals do not match route")
    return {"nodes": by_id, "edges": edges, "route": route, "stops": stops, "summary": summary}


def build_figure(data, weight="auto"):
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D
    from matplotlib.patches import FancyArrowPatch

    summary = data["summary"]
    metric = summary["strategy"] if weight == "auto" else weight
    if metric not in {"distance", "time", "cost"}:
        raise ValueError("weight must be auto, distance, time or cost")
    unit = {"distance": "km", "time": "min", "cost": "CNY"}[metric]
    nodes = data["nodes"]
    road_steps = defaultdict(list)
    for index, step in enumerate(data["route"][1:], 1):
        road_steps[step["incoming_edge_id"]].append(index)
    directions = {(edge["from_id"], edge["to_id"]) for edge in data["edges"]}
    styles = {"warehouse": ("s", "#315f8b", "Warehouse"),
              "delivery_point": ("o", "#187f78", "Delivery point"),
              "transfer_station": ("D", "#936928", "Transfer station")}
    fig, axes = plt.subplots(1, 2, figsize=(18, 10))
    fig.patch.set_facecolor("#fafbfc")
    fig.subplots_adjust(left=0.045, right=0.975, top=0.80, bottom=0.24, wspace=0.12)
    fig.text(0.045, 0.95, "LOGISTICS ROUTE PLANNER", fontsize=12, color="#5f6e7e", weight="bold")
    fig.text(0.045, 0.905, f"{'Distance' if summary['strategy'] == 'distance' else 'Cost'} strategy | single-vehicle tour",
             fontsize=24, weight="bold", color="#203245")
    fig.text(0.045, 0.86,
             f"{summary['delivery_count']} deliveries    |    {summary['total_distance']:.2f} km    |    "
             f"{summary['total_time']:.2f} min    |    CNY {summary['total_cost']:.2f}",
             fontsize=14, color="#203245")
    for panel, ax in enumerate(axes):
        ax.set_facecolor("#fafbfc")
        for edge in data["edges"]:
            source, target = nodes[edge["from_id"]], nodes[edge["to_id"]]
            x1, y1, x2, y2 = source["x"], source["y"], target["x"], target["y"]
            dx, dy = x2 - x1, y2 - y1
            length = math.hypot(dx, dy)
            rad = 0.16 if (edge["to_id"], edge["from_id"]) in directions else 0.0
            selected = panel == 1 and edge["id"] in road_steps
            color = "#db5c32" if selected else "#b5c1cc"
            if edge["traffic_status"] == "closed":
                color = "#aa8e99"
            ax.add_patch(FancyArrowPatch((x1, y1), (x2, y2), arrowstyle="-|>", mutation_scale=12,
                                        linewidth=2.5 if selected else 0.9,
                                        color=color, alpha=1 if panel == 0 or selected else 0.38,
                                        connectionstyle=f"arc3,rad={rad}", shrinkA=13, shrinkB=13,
                                        linestyle="--" if edge["traffic_status"] == "closed" else "-",
                                        zorder=3 if selected else 1))
            label = f"{edge[metric]:g}" if panel == 0 else (
                "#" + ",".join(map(str, road_steps[edge["id"]])) if selected else "")
            if label:
                offset = rad * length / 2 + 0.07
                lx = (x1 + x2) / 2 + (dy / length * offset if length else 0.1)
                ly = (y1 + y2) / 2 - (dx / length * offset if length else 0.1)
                angle = math.degrees(math.atan2(dy, dx))
                if angle > 90: angle -= 180
                if angle < -90: angle += 180
                ax.text(lx, ly, label, ha="center", va="center", rotation=angle,
                        rotation_mode="anchor", fontsize=7.5, color="#a64324" if selected else "#43576a",
                        bbox={"facecolor": "#fafbfc", "edgecolor": "none", "pad": 0.5}, zorder=5)
        for node in nodes.values():
            marker, color, _ = styles[node["type"]]
            ax.scatter(node["x"], node["y"], s=460, marker=marker, color=color, edgecolors="white", linewidths=1.2, zorder=6)
            ax.text(node["x"], node["y"], node["id"], ha="center", va="center", color="white", fontsize=7.5, weight="bold", zorder=7)
        ax.set_title(f"Network | labels: {metric} ({unit})" if panel == 0 else "Planned route | # = traversal step", loc="left", pad=18, fontsize=12)
        ax.set_aspect("equal", adjustable="box")
        ax.margins(0.10)
        ax.set_xlabel("x coordinate")
        if panel == 0: ax.set_ylabel("y coordinate")
        ax.tick_params(colors="#82909d", labelsize=9)
        ax.spines[["top", "right"]].set_visible(False)
        ax.spines[["left", "bottom"]].set_color("#d4dce3")
    legend = [Line2D([], [], marker=marker, color="none", markerfacecolor=color, markersize=9, label=label)
              for marker, color, label in styles.values()]
    legend.append(Line2D([], [], color="#db5c32", linewidth=2.5, label="Selected directed road"))
    fig.legend(handles=legend, loc="lower left", bbox_to_anchor=(0.035, 0.165), ncol=4, frameon=False, fontsize=10)
    order = " > ".join(stop["node_id"] for stop in data["stops"])
    fig.text(0.045, 0.14, "DELIVERY ORDER (transit-only nodes omitted)", fontsize=9, color="#5f6e7e", weight="bold")
    fig.text(0.045, 0.113, textwrap.fill(order, width=150), fontsize=9, color="#203245", va="top", linespacing=1.5)
    fig.text(0.045, 0.035, "Nearest-neighbor + Dijkstra. Repeated road steps are grouped on arrows. "
             "Time windows and global tour optimality are not enforced.", fontsize=9, color="#5f6e7e")
    return fig


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output", type=Path, default=Path("exports/route.png"))
    parser.add_argument("--weight", choices=["auto", "distance", "time", "cost"], default="auto")
    parser.add_argument("--show", action="store_true", help="also open an interactive plot window")
    args = parser.parse_args()
    try:
        import matplotlib
        if not args.show:
            matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        data = load_data(args.input_dir)
        figure = build_figure(data, args.weight)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        figure.savefig(args.output, dpi=180, facecolor=figure.get_facecolor())
        print(f"Saved plot: {args.output}")
        if args.show:
            plt.show()
        plt.close(figure)
        return 0
    except (OSError, ValueError, ImportError) as error:
        parser.exit(1, f"Plot error: {error}\nInstall dependencies: python -m pip install -r scripts/requirements.txt\n")


if __name__ == "__main__":
    raise SystemExit(main())
