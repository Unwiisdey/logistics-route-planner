"""Replay C++ simulation snapshots, or export a selected frame / animated GIF.

Uses the existing matplotlib dependency. Playback is simulated time, not live traffic.
"""
import argparse
import math
from collections import defaultdict
from pathlib import Path
from plot_network import load_data, number, read_table


def load_simulation(directory):
    frames = read_table(directory, "simulation.csv", [
        "time", "clock", "status", "node_id", "edge_id", "from_id", "to_id", "remaining_minutes",
        "progress", "x", "y", "load", "completed_edges", "delivered_count", "late_count", "penalty_points", "finished"])
    if not frames:
        raise ValueError("simulation has no frames")
    if any(int(frame.get("route_revision", 0)) != 0 for frame in frames):
        raise ValueError("order-route revisions require route_revisions.csv; this viewer supports stage 4 static routes only")
    data = load_data(directory)
    previous_time, previous_step = -1, 0
    route = data["route"]
    for frame in frames:
        for field in ("time", "remaining_minutes", "progress", "x", "y", "load", "penalty_points"):
            frame[field] = number(frame[field])
        for field in ("completed_edges", "delivered_count", "late_count", "finished"):
            frame[field] = int(frame[field])
        step = frame["completed_edges"]
        if not previous_step <= step < len(route) or frame["time"] < previous_time:
            raise ValueError("simulation time/route progress must be monotonic and in range")
        if (not 0 <= frame["progress"] <= 1 or min(frame["time"], frame["remaining_minutes"], frame["load"], frame["penalty_points"]) < 0
                or not 0 <= frame["late_count"] <= frame["delivered_count"] <= int(data["summary"]["delivery_count"])
                or frame["finished"] not in {0, 1}):
            raise ValueError("invalid simulation numeric state")
        if frame["status"] == "driving":
            if (frame["node_id"] or step + 1 >= len(route) or frame["edge_id"] != route[step + 1]["incoming_edge_id"]
                    or (frame["from_id"], frame["to_id"]) != (route[step]["node_id"], route[step + 1]["node_id"])):
                raise ValueError("driving frame does not match active route edge")
            source, target = data["nodes"][frame["from_id"]], data["nodes"][frame["to_id"]]
            expected = [(1 - frame["progress"]) * source[axis] + frame["progress"] * target[axis] for axis in ("x", "y")]
        elif frame["status"] in {"idle", "waiting", "unloading"}:
            if frame["edge_id"] or frame["node_id"] != route[step]["node_id"]:
                raise ValueError("stationary frame does not match route node")
            node = data["nodes"][frame["node_id"]]
            expected = [node["x"], node["y"]]
        else:
            raise ValueError("unknown vehicle status")
        if not all(math.isclose(frame[axis], value, rel_tol=1e-9, abs_tol=1e-9) for axis, value in zip(("x", "y"), expected)):
            raise ValueError("vehicle coordinates do not match position")
        if frame["finished"] and (step != len(route) - 1 or frame["status"] != "idle"):
            raise ValueError("finished vehicle must be idle at the final route node")
        previous_time, previous_step = frame["time"], step
    return data, frames


def create_view(data, frames):
    import matplotlib.pyplot as plt
    from matplotlib.patches import FancyArrowPatch
    from matplotlib.lines import Line2D

    fig, ax = plt.subplots(figsize=(12, 9))
    fig.patch.set_facecolor("#fafbfc")
    ax.set_facecolor("#fafbfc")
    fig.subplots_adjust(left=.06, right=.97, top=.78, bottom=.22)
    fig.text(.06, .95, "LOGISTICS SIMULATION | STAGE 4", fontsize=13, weight="bold", color="#536779")
    title = fig.text(.06, .90, "", fontsize=22, weight="bold", color="#203245")
    info = fig.text(.06, .85, "", fontsize=12, color="#203245")
    location = fig.text(.06, .81, "", fontsize=10, color="#536779")
    nodes = data["nodes"]
    edges = {edge["id"]: edge for edge in data["edges"]}
    directions = {(edge["from_id"], edge["to_id"]) for edge in edges.values()}
    colors = {"done": "#27886d", "current": "#e36c25", "future": "#6098d1"}

    def arrow(edge, rad, color, width, zorder):
        a, b = nodes[edge["from_id"]], nodes[edge["to_id"]]
        patch = FancyArrowPatch((a["x"], a["y"]), (b["x"], b["y"]), arrowstyle="-|>",
                                mutation_scale=12, linewidth=width, color=color, shrinkA=12, shrinkB=12,
                                connectionstyle=f"arc3,rad={rad}", zorder=zorder)
        ax.add_patch(patch)
        return patch

    for edge in edges.values():
        rad = .16 if (edge["to_id"], edge["from_id"]) in directions else 0
        arrow(edge, rad, "#dce2e8", .8, 1)
    passes = defaultdict(int)
    artists = []
    for i, step in enumerate(data["route"][1:]):
        edge = edges[step["incoming_edge_id"]]
        rad = (.28 if (edge["to_id"], edge["from_id"]) in directions else 0) + .24 * passes[edge["id"]]
        passes[edge["id"]] += 1
        patch = arrow(edge, rad, colors["future"], 1.7, 2)
        a, b = nodes[edge["from_id"]], nodes[edge["to_id"]]
        dx, dy = b["x"] - a["x"], b["y"] - a["y"]
        label = ax.text((a["x"] + b["x"]) / 2 + dy * rad / 2,
                        (a["y"] + b["y"]) / 2 - dx * rad / 2, f"#{i + 1}", fontsize=7,
                        ha="center", va="center", color=colors["future"], zorder=4,
                        bbox={"facecolor": "#fafbfc", "edgecolor": "none", "pad": .4})
        artists.append((patch, label))
    styles = {"warehouse": ("s", "#315f8b"), "delivery_point": ("o", "#187f78"), "transfer_station": ("D", "#936928")}
    for node in nodes.values():
        marker, color = styles[node["type"]]
        ax.scatter(node["x"], node["y"], s=350, marker=marker, color=color, edgecolors="white", zorder=5)
        ax.text(node["x"], node["y"], node["id"], ha="center", va="center", color="white", fontsize=7, zorder=6)
    car = ax.scatter([], [], marker="*", s=420, color="#c82e87", edgecolors="white", linewidths=1.2, zorder=8)
    ax.set_aspect("equal", adjustable="box"); ax.margins(.12)
    ax.set_xlabel("x coordinate"); ax.set_ylabel("y coordinate")
    ax.spines[["top", "right"]].set_visible(False)
    legend = [Line2D([], [], color=color, linewidth=3, label=label)
              for color, label in [(colors["done"], "Completed traversal"), (colors["current"], "Current traversal"), (colors["future"], "Remaining traversal")]]
    legend.append(Line2D([], [], marker="*", color="none", markerfacecolor="#c82e87", markersize=16, label="Vehicle position"))
    fig.legend(handles=legend, loc="lower left", bbox_to_anchor=(.05, .14), ncol=4, frameon=False, fontsize=10)
    fig.text(.06, .105, "# = road traversal number. Repeated traversals use separate curves. Vehicle moves linearly between node coordinates.", fontsize=9, color="#536779")
    fig.text(.06, .075, "Time window checked at unloading start. Fixed penalty per late order; recorded once when delivery completes.", fontsize=9, color="#536779")

    def update(index):
        frame = frames[int(index)]
        completed = frame["completed_edges"]
        for i, (patch, label) in enumerate(artists):
            phase = "done" if i < completed else "current" if i == completed and frame["status"] == "driving" else "future"
            patch.set_color(colors[phase]); label.set_color(colors[phase])
            patch.set_linewidth(3.5 if phase == "current" else 1.8)
        car.set_offsets([[frame["x"], frame["y"]]])
        title.set_text(f"{frame['clock']}  |  {'completed' if frame['finished'] else frame['status']}")
        info.set_text(f"Load {frame['load']:.1f} kg  |  Delivered {frame['delivered_count']}/{data['summary']['delivery_count']}"
                      f"  |  Late {frame['late_count']}  |  Penalty {frame['penalty_points']:g} points")
        at = f"{frame['from_id']} > {frame['to_id']} ({frame['edge_id']})" if frame["edge_id"] else frame["node_id"]
        location.set_text(f"Position: {at}  |  Phase remaining: {frame['remaining_minutes']:.2f} min  |  Simulation time: {frame['time']:.2f} min")
        return [car, title, info, location]

    update(0)
    return fig, update


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("exports/simulation.png"))
    parser.add_argument("--frame", type=int, default=0)
    parser.add_argument("--show", action="store_true", help="open replay with a frame slider and play/pause button")
    parser.add_argument("--gif", type=Path, help="save animated replay with Pillow (included by matplotlib)")
    args = parser.parse_args()
    try:
        import matplotlib
        if not args.show: matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.animation import FuncAnimation, PillowWriter
        data, frames = load_simulation(args.input_dir)
        if not 0 <= args.frame < len(frames): raise ValueError(f"frame must be between 0 and {len(frames) - 1}")
        fig, update = create_view(data, frames)
        update(args.frame)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        fig.savefig(args.output, dpi=150)
        print(f"Saved frame {args.frame}: {args.output}; {len(frames)} snapshots available")
        if args.gif:
            args.gif.parent.mkdir(parents=True, exist_ok=True)
            animation = FuncAnimation(fig, update, frames=len(frames), interval=250, blit=False)
            animation.save(args.gif, writer=PillowWriter(fps=4), dpi=90)
            print(f"Saved replay: {args.gif}")
        if args.show:
            from matplotlib.widgets import Slider, Button
            update(args.frame)
            slider = Slider(fig.add_axes([.15, .025, .62, .025]), "Frame", 0, max(1, len(frames) - 1), valinit=args.frame, valstep=1)
            def select(value):
                update(min(int(value), len(frames) - 1)); fig.canvas.draw_idle()
            slider.on_changed(select)
            playing = [False]
            button = Button(fig.add_axes([.82, .015, .12, .045]), "Play / Pause")
            button.on_clicked(lambda _: playing.__setitem__(0, not playing[0]))
            def tick():
                if playing[0]: slider.set_val((int(slider.val) + 1) % len(frames))
            timer = fig.canvas.new_timer(interval=250); timer.add_callback(tick); timer.start()
            plt.show()
        plt.close(fig)
    except (OSError, ValueError, ImportError) as error:
        parser.exit(1, f"Simulation plot error: {error}\n")


if __name__ == "__main__":
    main()
