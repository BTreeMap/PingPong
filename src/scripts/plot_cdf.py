#!/usr/bin/env python3
"""
Read raw timestamp CSV and plot CDFs for client stack, network transit, and round-trip times.
"""
import argparse
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


def plot_cdf(data, label, color):
    sorted_data = np.sort(data)
    y = np.arange(1, len(sorted_data) + 1) / len(sorted_data)
    plt.plot(sorted_data, y, label=label, color=color)


def main():
    parser = argparse.ArgumentParser(description="Plot CDF of latency components")
    parser.add_argument(
        "--input", required=True, help="Input CSV file with timestamp data"
    )
    parser.add_argument(
        "--output", required=True, help="Output image filename (e.g., latency_cdf.png)"
    )
    args = parser.parse_args()

    df = pd.read_csv(args.input)
    # compute latency components in microseconds
    client_stack = df["send_exit_us"] - df["send_entry_us"]
    network = df["recv_entry_us"] - df["send_exit_us"]
    round_trip = df["recv_entry_us"] - df["send_entry_us"]

    plt.figure(figsize=(8, 6))
    plot_cdf(client_stack, "Client stack (us)", "blue")
    plot_cdf(network, "Network transit (us)", "green")
    plot_cdf(round_trip, "Round-trip (us)", "red")

    plt.xlabel("Latency (microseconds)")
    plt.ylabel("CDF")
    plt.title("Latency CDF")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(args.output)
    print(f"CDF plot saved to {args.output}")


if __name__ == "__main__":
    main()
