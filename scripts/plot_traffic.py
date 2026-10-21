"""Replay traffic, pending/active route revisions, vehicle position and ETA from C++ CSVs.

No routing is performed here. --show provides a time slider and play/pause.
"""
import argparse
import math
from collections import defaultdict
from pathlib import Path
from plot_network import number, read_table


def load_traffic(directory):
    nodes = {}
    for row in read_table(directory, "nodes.csv", ["id", "type", "x", "y"]):
        if row["id"] in nodes or row["type"] not in {"warehouse", "delivery_point", "transfer_station"}:
            raise ValueError("duplicate node or unknown type")
        row["x"], row["y"] = number(row["x"]), number(row["y"])
        nodes[row["id"]] = row
    edges = {}
    for row in read_table(directory, "edges.csv", ["id", "from_id", "to_id", "time", "traffic_status"]):
        if row["id"] in edges or row["from_id"] not in nodes or row["to_id"] not in nodes:
            raise ValueError("duplicate edge or missing endpoint")
        row["time"] = number(row["time"])
        if row["time"] < 0 or row["traffic_status"] not in {"clear", "congested", "closed"}:
            raise ValueError("invalid initial traffic")
        edges[row["id"]] = row

    def paths(filename, key):
        result = defaultdict(list)
        for row in read_table(directory, filename, [key, "step", "node_id", "incoming_edge_id"]):
            index = int(row[key])
            route = result[index]
            if index < 0 or int(row["step"]) != len(route) or row["node_id"] not in nodes:
                raise ValueError("invalid route step")
            edge = edges.get(row["incoming_edge_id"])
            if route:
                if not edge or (edge["from_id"], edge["to_id"]) != (route[-1]["node_id"], row["node_id"]):
                    raise ValueError("route does not follow exported roads")
            elif row["incoming_edge_id"]:
                raise ValueError("route starts with an incoming edge")
            route.append(row)
        return dict(result)

    revisions = paths("route_revisions.csv", "revision")
    future = paths("remaining_routes.csv", "frame")
    traffic = read_table(directory, "traffic.csv", ["time", "edge_id", "before", "after", "before_status", "after_status", "trigger"])
    current = {key: (value["time"], value["traffic_status"]) for key, value in edges.items()}
    previous_time = -1
    for change in traffic:
        for field in ("time", "before", "after"): change[field] = number(change[field])
        if (change["edge_id"] not in edges or change["time"] < previous_time or min(change["before"], change["after"]) < 0
                or change["after_status"] not in {"clear", "congested"} or change["trigger"] not in {"0", "1"}):
            raise ValueError("invalid traffic change")
        before, status = current[change["edge_id"]]
        if not math.isclose(before, change["before"], rel_tol=1e-12, abs_tol=1e-12) or status != change["before_status"]:
            raise ValueError("broken traffic history")
        current[change["edge_id"]] = change["after"], change["after_status"]
        previous_time = change["time"]
    frames = read_table(directory, "simulation.csv", [
        "time", "clock", "status", "node_id", "edge_id", "from_id", "to_id", "remaining_minutes", "progress", "x", "y", "load",
        "completed_edges", "delivered_count", "late_count", "penalty_points", "finished", "route_revision",
        "estimated_return", "next_order", "next_arrival", "traffic_version", "completed_traversals"])
    traversals = read_table(directory, "traversals.csv", ["time", "edge_id"])
    previous_time = -1
    for traversal in traversals:
        traversal["time"] = number(traversal["time"])
        if traversal["time"] < previous_time or traversal["edge_id"] not in edges:
            raise ValueError("invalid completed traversal")
        previous_time = traversal["time"]
    previous_time, previous_version, previous_revision, previous_step, previous_traversals = -1, 0, -1, 0, 0
    for index, frame in enumerate(frames):
        for field in ("time", "remaining_minutes", "progress", "x", "y", "load", "penalty_points"):
            frame[field] = number(frame[field])
        for field in ("completed_edges", "delivered_count", "late_count", "finished", "route_revision", "traffic_version", "completed_traversals"):
            frame[field] = int(frame[field])
        for field in ("estimated_return", "next_arrival"):
            frame[field] = number(frame[field]) if frame[field] else None
        version, revision, step = frame["traffic_version"], frame["route_revision"], frame["completed_edges"]
        route = revisions.get(revision)
        if (frame["time"] < previous_time or not previous_version <= version <= len(traffic) or revision < previous_revision
                or not route or not 0 <= step < len(route) or (revision == previous_revision and step < previous_step)):
            raise ValueError("invalid frame time, traffic version or route revision")
        if version and traffic[version - 1]["time"] > frame["time"]:
            raise ValueError("frame uses future traffic")
        count = frame["completed_traversals"]
        if not previous_traversals <= count <= len(traversals) or (count and traversals[count - 1]["time"] > frame["time"]):
            raise ValueError("frame uses invalid completed traversals")
        if (not 0 <= frame["progress"] <= 1 or min(frame["time"], frame["remaining_minutes"], frame["load"], frame["penalty_points"]) < 0
                or not 0 <= frame["late_count"] <= frame["delivered_count"] or frame["finished"] not in {0, 1}):
            raise ValueError("invalid frame numeric state")
        if frame["status"] == "driving":
            if (frame["node_id"] or step + 1 >= len(route) or frame["edge_id"] != route[step + 1]["incoming_edge_id"]
                    or (frame["from_id"], frame["to_id"]) != (route[step]["node_id"], route[step + 1]["node_id"])):
                raise ValueError("vehicle does not match active route")
            a, b = nodes[frame["from_id"]], nodes[frame["to_id"]]
            expected = [(1 - frame["progress"]) * a[axis] + frame["progress"] * b[axis] for axis in ("x", "y")]
            anchor = frame["to_id"]
        elif frame["status"] in {"idle", "waiting", "unloading"}:
            if frame["edge_id"] or frame["node_id"] != route[step]["node_id"]:
                raise ValueError("vehicle does not match stationary route node")
            expected = [nodes[frame["node_id"]][axis] for axis in ("x", "y")]
            anchor = frame["node_id"]
        else:
            raise ValueError("invalid vehicle status")
        if not all(math.isclose(frame[axis], value, rel_tol=1e-9, abs_tol=1e-9) for axis, value in zip(("x", "y"), expected)):
            raise ValueError("vehicle coordinates do not match position")
        if frame["finished"] and (frame["status"] != "idle" or step != len(route) - 1):
            raise ValueError("finished frame must be at route end")
        if index in future and future[index][0]["node_id"] != anchor:
            raise ValueError("future route does not match locked-phase anchor")
        previous_time, previous_version, previous_revision, previous_step = frame["time"], version, revision, step
        previous_traversals = count
    if not nodes or not frames or any(index >= len(frames) for index in future):
        raise ValueError("missing nodes/frames or extra future route")
    return {"nodes": nodes, "edges": edges, "revisions": revisions, "future": future, "traffic": traffic, "traversals": traversals}, frames


def state_at(data, frames, index):
    """Build one replay state; supports seeking backward without retaining stale congestion."""
    frame = frames[index]
    edges = {key: dict(edge) for key, edge in data["edges"].items()}
    for change in data["traffic"][:frame["traffic_version"]]:
        edges[change["edge_id"]].update(time=change["after"], traffic_status=change["after_status"])
    route = data["revisions"][frame["route_revision"]]
    future = data["future"].get(index)
    if future is None:
        anchor = frame["completed_edges"] + (frame["status"] == "driving")
        future = route[anchor:]
    return edges, future


def clock(minutes):
    if minutes is None: return "--"
    seconds = int(round(minutes * 60))
    return f"{seconds // 3600:02d}:{seconds // 60 % 60:02d}:{seconds % 60:02d}"


def create_view(data, frames):
    import matplotlib.pyplot as plt
    from matplotlib.lines import Line2D
    from matplotlib.patches import FancyArrowPatch
    fig, axes = plt.subplots(1, 2, figsize=(16, 9))
    fig.patch.set_facecolor("#fafbfc")
    fig.subplots_adjust(left=.04, right=.98, top=.75, bottom=.19, wspace=.13)
    fig.text(.04, .95, "LOGISTICS | TRAFFIC & LOCAL REPLANNING", fontsize=14, weight="bold", color="#4e6375")
    title = fig.text(.04, .90, "", fontsize=22, weight="bold", color="#21384b")
    info = fig.text(.04, .855, "", fontsize=11, color="#21384b")
    eta = fig.text(.04, .815, "", fontsize=11, color="#21384b")
    legend = [Line2D([], [], color=color, lw=3, label=label) for color, label in
              [("#d34445", "Congested"), ("#adb8c2", "Clear"), ("#78a78e", "Traversed"),
               ("#df751f", "Current road (locked)"), ("#8052ba", "Updated remaining route")]]
    fig.legend(handles=legend, loc="lower center", bbox_to_anchor=(.5, .12), ncol=5, frameon=False)
    fig.text(.04, .10, "Road labels: current travel minutes. Vehicle: star. Current road keeps its entry-time duration. ETA includes waiting/unloading.", fontsize=10, color="#4e6375")
    nodes = data["nodes"]
    directions = {(edge["from_id"], edge["to_id"]) for edge in data["edges"].values()}
    styles = {"warehouse": ("s", "#315f8b"), "delivery_point": ("o", "#187f78"), "transfer_station": ("D", "#936928")}

    def arrow(ax, edge, color, width, label=None, extra=0):
        a, b = nodes[edge["from_id"]], nodes[edge["to_id"]]
        rad = (.28 if (edge["to_id"], edge["from_id"]) in directions else 0) + extra
        ax.add_patch(FancyArrowPatch((a["x"], a["y"]), (b["x"], b["y"]), arrowstyle="-|>",
                                    mutation_scale=11, linewidth=width, color=color, shrinkA=10, shrinkB=10,
                                    connectionstyle=f"arc3,rad={rad}", zorder=2))
        if label is not None:
            dx, dy = b["x"] - a["x"], b["y"] - a["y"]
            ax.text((a["x"] + b["x"]) / 2 + dy * rad / 2, (a["y"] + b["y"]) / 2 - dx * rad / 2,
                    label, fontsize=7,
                    ha=("left" if dy * rad > 0 else "right") if abs(dy) > abs(dx) and rad else "center",
                    va="center", color=color, zorder=4,
                    bbox={"facecolor": "#fafbfc", "edgecolor": "none", "pad": .4})

    def update(index):
        index = int(index)
        frame = frames[index]
        edges, future = state_at(data, frames, index)
        for ax in axes:
            ax.clear(); ax.set_facecolor("#fafbfc"); ax.set_aspect("equal", adjustable="box")
            ax.spines[["top", "right"]].set_visible(False)
            ax.set_xlabel("x"); ax.set_ylabel("y")
        for edge in edges.values():
            color = "#d34445" if edge["traffic_status"] == "congested" else "#adb8c2"
            arrow(axes[0], edge, color, 1.5, f"{edge['time']:.1f}")
            arrow(axes[1], edge, "#e3e7eb", .6)
        traversed = {row["edge_id"] for row in data["traversals"][:frame["completed_traversals"]]}
        for road in traversed: arrow(axes[1], edges[road], "#78a78e", 1.6)
        passes = defaultdict(int)
        for i, step in enumerate(future[1:], 1):
            road = step["incoming_edge_id"]
            arrow(axes[1], edges[road], "#8052ba", 2.3, f"#{i} {edges[road]['time']:.1f}m", .13 + .19 * passes[road])
            passes[road] += 1
        if frame["edge_id"]: arrow(axes[1], edges[frame["edge_id"]], "#df751f", 3.4)
        for ax in axes:
            for node in nodes.values():
                marker, color = styles[node["type"]]
                ax.scatter(node["x"], node["y"], s=260, marker=marker, color=color, edgecolors="white", zorder=5)
                ax.text(node["x"], node["y"], node["id"], color="white", ha="center", va="center", fontsize=7, zorder=6)
            ax.scatter(frame["x"], frame["y"], s=360, marker="*", color="#ecba24", edgecolors="#523b11", zorder=8)
            ax.margins(.12)
        congested = sum(edge["traffic_status"] == "congested" for edge in edges.values())
        axes[0].set_title(f"Current network | {congested} congested roads", fontsize=12)
        axes[1].set_title("Remaining plan | updates apply after current road/service", fontsize=12)
        title.set_text(f"{clock(frame['time'])}  |  {frame['status']}  |  active revision {frame['route_revision']}")
        location = f"{frame['from_id']} > {frame['to_id']}" if frame["edge_id"] else frame["node_id"]
        info.set_text(f"Position {location}  |  Load {frame['load']:.1f} kg  |  Delivered {frame['delivered_count']}"
                      f"  |  Late {frame['late_count']}  |  Penalty {frame['penalty_points']:g}")
        eta.set_text(f"Next delivery {frame['next_order'] or '--'}: arrival {clock(frame['next_arrival'])}"
                     f"  |  Warehouse return {clock(frame['estimated_return'])}  |  Phase remaining {frame['remaining_minutes']:.2f} min")
        return [title, info, eta]
    update(0)
    return fig, update


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("exports/traffic.png"))
    parser.add_argument("--frame", type=int, default=0)
    parser.add_argument("--show", action="store_true")
    args = parser.parse_args()
    try:
        import matplotlib
        if not args.show: matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        data, frames = load_traffic(args.input_dir)
        if not 0 <= args.frame < len(frames): raise ValueError(f"frame must be 0..{len(frames) - 1}")
        fig, update = create_view(data, frames)
        update(args.frame)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(args.output, dpi=160)
        print(f"Saved {args.output}; frames={len(frames)}")
        if args.show:
            from matplotlib.animation import FuncAnimation
            from matplotlib.widgets import Button, Slider
            slider = Slider(fig.add_axes([.15, .035, .65, .025]), "Frame", 0, max(1, len(frames) - 1), valinit=args.frame, valstep=1)
            button = Button(fig.add_axes([.83, .025, .12, .045]), "Play / Pause")
            playing = [False]
            def seek(value):
                update(min(int(value), len(frames) - 1)); fig.canvas.draw_idle()
            def toggle(_event): playing[0] = not playing[0]
            def tick(_index):
                if playing[0]: slider.set_val((int(slider.val) + 1) % len(frames))
            slider.on_changed(seek); button.on_clicked(toggle)
            animation = FuncAnimation(fig, tick, interval=150, cache_frame_data=False)
            plt.show()
            _ = animation
        plt.close(fig)
    except (OSError, ValueError, KeyError, ImportError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__": main()
