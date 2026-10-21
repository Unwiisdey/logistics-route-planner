"""Repeat the Release benchmark; save raw measurements and per-workload medians."""
import argparse
import csv
import io
from pathlib import Path
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--output", type=Path, default=Path("exports/benchmark"))
    args = parser.parse_args()
    if not 1 <= args.runs <= 50: parser.error("runs must be 1..50")
    try:
        rows = []
        for run in range(1, args.runs + 1):
            result = subprocess.run([str(args.binary.resolve()), "--full"], check=True, capture_output=True, text=True)
            samples = list(csv.DictReader(io.StringIO(result.stdout)))
            if len(samples) != 8: raise ValueError("expected eight benchmark rows per run")
            rows.extend(dict(run=run, **sample) for sample in samples)
        args.output.mkdir(parents=True, exist_ok=True)
        with (args.output / "raw.csv").open("w", encoding="utf-8", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=rows[0].keys()); writer.writeheader(); writer.writerows(rows)
        groups = {}
        for row in rows:
            key = tuple(row[field] for field in ("workload", "nodes", "edges", "cache_capacity"))
            groups.setdefault(key, []).append(row)
        medians = []
        for group in groups.values():
            for field in ("queries", "hits", "searches", "checksum"):
                if len({row[field] for row in group}) != 1: raise ValueError("non-timing results changed between runs")
            median = {field: value for field, value in group[0].items() if field not in {"run", "elapsed_ms"}}
            median.update(runs=len(group), median_ms=f"{statistics.median(float(row['elapsed_ms']) for row in group):.3f}")
            medians.append(median)
        with (args.output / "median.csv").open("w", encoding="utf-8", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=medians[0].keys()); writer.writeheader(); writer.writerows(medians)
        print(f"Saved {len(rows)} raw measurements and {len(medians)} medians to {args.output}")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__": main()
