#!/usr/bin/env python3
"""
Run the eBPF user-space monitor, parse entry/exit events, match pairs, and output CSV and optionally plot CDFs.
"""
import argparse
import subprocess
import sys
import re
import csv
import os
import time

EVENT_PAT = re.compile(r"ts:(\d+) pid:(\d+) type:(\w+)")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze eBPF pingpong events: launch BPF, server, client, match entry/exit pairs."
    )
    parser.add_argument(
        "--addr", required=True, help="Server IP address for client to connect to"
    )
    parser.add_argument(
        "--control-port",
        type=int,
        required=True,
        help="Control port for negotiation between client and server",
    )
    parser.add_argument(
        "--exp-port",
        type=int,
        required=True,
        help="Experimental TCP port for ping-pong traffic",
    )
    parser.add_argument(
        "--size",
        type=int,
        required=True,
        help="Payload size in bytes for ping-pong messages",
    )
    parser.add_argument(
        "--count",
        type=int,
        default=0,
        help="Number of request/response to record (0 for unlimited)",
    )
    parser.add_argument("--output", required=True, help="CSV output filename")
    parser.add_argument(
        "--plot", action="store_true", help="Generate CDF plot after CSV"
    )
    parser.add_argument(
        "--plot-output", default="latency_cdf.png", help="Output image for CDF plot"
    )
    return parser.parse_args()


def main():
    args = parse_args()
    # ensure positive count
    if args.count <= 0:
        print(
            "Please specify --count > 0 for number of ping-pong exchanges.",
            file=sys.stderr,
        )
        sys.exit(1)
    # locate binaries
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    ebpf_bin = os.path.join(root, "build", "pingpong-ebpf")
    server_bin = os.path.join(root, "build", "pingpong-server")
    client_bin = os.path.join(root, "build", "pingpong-client")
    # launch eBPF monitor with port-based filter
    ebpf_proc = subprocess.Popen(
        [ebpf_bin, "--dport", str(args.exp_port)],
        stdout=subprocess.PIPE,
        stderr=sys.stderr,
        text=True,
    )
    time.sleep(0.1)
    # launch pingpong server
    server_proc = subprocess.Popen(
        [server_bin, "--port", str(args.control_port)],
        stdout=subprocess.DEVNULL,
        stderr=sys.stderr,
    )
    time.sleep(0.1)
    # launch pingpong client (CSV to /dev/null)
    client_proc = subprocess.Popen(
        [
            client_bin,
            "--addr",
            args.addr,
            "--control-port",
            str(args.control_port),
            "--exp-port",
            str(args.exp_port),
            "--size",
            str(args.size),
            "--count",
            str(args.count),
            "--output",
            os.devnull,
        ],
        stdout=subprocess.DEVNULL,
        stderr=sys.stderr,
    )
    send_q = []
    recv_q = []
    pending = []
    records = []
    seq = 0

    try:
        # read events from eBPF monitor
        for line in ebpf_proc.stdout:
            m = EVENT_PAT.search(line)
            if not m:
                continue
            ts_ns = int(m.group(1))
            evt = m.group(3)
            # convert to microseconds
            ts_us = ts_ns / 1000.0
            if evt == "send_entry":
                send_q.append(ts_us)
            elif evt == "send_exit":
                if send_q:
                    t0 = send_q.pop(0)
                    pending.append({"send_entry_us": t0, "send_exit_us": ts_us})
            elif evt == "recv_entry":
                recv_q.append(ts_us)
            elif evt == "recv_exit":
                if recv_q and pending:
                    t1 = recv_q.pop(0)
                    rec = pending.pop(0)
                    rec.update({"recv_entry_us": t1, "recv_exit_us": ts_us})
                    records.append(rec)
                    seq += 1
                    if args.count and seq >= args.count:
                        break
    except KeyboardInterrupt:
        pass
    finally:
        # clean up subprocesses
        client_proc.terminate()
        server_proc.terminate()
        ebpf_proc.terminate()

    # write CSV
    with open(args.output, "w", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "seq",
                "send_entry_us",
                "send_exit_us",
                "recv_entry_us",
                "recv_exit_us",
            ],
        )
        writer.writeheader()
        for i, rec in enumerate(records):
            row = {"seq": i, **rec}
            writer.writerow(row)
    print(f"Written {len(records)} records to {args.output}")

    # optional plot
    if args.plot:
        import subprocess as sp

        plot_py = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "plot_cdf.py")
        )
        cmd2 = [plot_py, "--input", args.output, "--output", args.plot_output]
        print(f"Generating CDF plot to {args.plot_output}")
        sp.run(cmd2, check=True)


if __name__ == "__main__":
    main()
