#!/usr/bin/env python3
"""
Generate CDF plot and percentile summary from PingPong results CSV.
"""
import argparse
import csv
import sys
import numpy as np
import matplotlib.pyplot as plt


def parse_args():
    p = argparse.ArgumentParser(
        description="Plot CDF of latency components and show percentiles."
    )
    p.add_argument("--input", required=True, help="Path to results CSV")
    p.add_argument("--output", required=True, help="Output image file for CDF plot")
    p.add_argument(
        "--percentiles",
        nargs="*",
        type=float,
        default=[50, 90, 99],
        help="List of percentiles to compute (e.g., 50 90 99)",
    )
    return p.parse_args()


def load_metrics(csv_path):
    data = {}
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        # initialize lists for each metric except seq
        for field in reader.fieldnames:
            if field != "seq":
                data[field] = []
        for row in reader:
            for field, vals in data.items():
                try:
                    vals.append(float(row[field]))
                except ValueError:
                    pass
    return data


def compute_percentiles(data, percentiles):
    stats = {}
    for name, vals in data.items():
        arr = np.array(vals)
        stats[name] = {p: np.percentile(arr, p) for p in percentiles}
    return stats


def plot_cdf(data, output_path):
    plt.figure(figsize=(8, 5))
    for name, vals in data.items():
        arr = np.sort(np.array(vals))
        y = np.linspace(0, 1, len(arr), endpoint=True)
        plt.plot(arr, y, label=name)
    plt.xlabel("Latency (us)")
    plt.ylabel("CDF")
    plt.title("PingPong Latency CDF")
    plt.grid(True)
    plt.legend(loc="lower right")
    plt.tight_layout()
    plt.savefig(output_path, dpi=300)
    print(f"Saved CDF plot to {output_path}")


def print_summary(stats):
    print("\nLatency Percentile Summary (microseconds):")
    for name, pct in stats.items():
        parts = [f"{int(p)}th={pct[p]:.2f}" for p in sorted(pct)]
        print(f"  {name}: " + ", ".join(parts))


def main():
    args = parse_args()
    data = load_metrics(args.input)
    if not data:
        print("No data loaded; check CSV file.", file=sys.stderr)
        sys.exit(1)
    stats = compute_percentiles(data, args.percentiles)
    print_summary(stats)
    plot_cdf(data, args.output)


if __name__ == "__main__":
    main()
