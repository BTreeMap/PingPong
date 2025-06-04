#!/usr/bin/env python3
"""
Modular eBPF pingpong analyzer: parses recorded event logs, matches full ping-pong cycles, computes client/server/network latencies, and writes CSV (and optional CDF plot).
"""
import argparse
import csv
import re
import sys
import os
import subprocess
from typing import List, Dict, Optional

EVENT_RE = re.compile(
    r"ts:(?P<ts>\d+)\s+sock:(?P<sock>\d+)\s+pid:(?P<pid>\d+)\s+type:(?P<type>\w+)\s+srtt:(?P<srtt>\d+)\s+(?P<addr>.+)"
)
ADDR_RE = re.compile(
    r"(?P<src>\[?[0-9A-Fa-f:\.]+\]?):(?P<srcp>\d+)\s*->\s*(?P<dst>\[?[0-9A-Fa-f:\.]+\]?):(?P<dstp>\d+)"
)


class Event:
    def __init__(
        self,
        ts_us: float,
        sock: int,
        evt_type: str,
        src: str,
        srcp: int,
        dst: str,
        dstp: int,
        srtt_us: int,
    ):
        self.ts = ts_us
        self.sock = sock
        self.type = evt_type
        self.src = src.strip("[]")
        self.srcp = srcp
        self.dst = dst.strip("[]")
        self.dstp = dstp
        self.srtt_us = srtt_us


def parse_args():
    p = argparse.ArgumentParser(description="Analyze eBPF pingpong event logs.")
    p.add_argument(
        "--inputs", nargs="+", required=True, help="One or more eBPF event log files"
    )
    p.add_argument("--output", default="results.csv", help="CSV output filename")
    p.add_argument(
        "--client-ip", default="100.80.0.1", help="Client IP address to filter events"
    )
    p.add_argument(
        "--server-ip", default="100.80.0.0", help="Server IP address to filter events"
    )
    p.add_argument(
        "--smart-skip",
        action="store_true",
        default=False,
        help="Enable intelligent trimming of incomplete cycle runs",
    )
    p.add_argument("--plot", action="store_true", help="Generate CDF plot after CSV")
    p.add_argument(
        "--plot-output",
        default="plots/latency_cdf.png",
        help="Output image for CDF plot",
    )
    return p.parse_args()


def parse_line(line: str) -> Optional[Event]:
    m = EVENT_RE.search(line)
    if not m:
        return None
    ts_us = int(m.group("ts")) / 1000.0
    sock = int(m.group("sock"))
    evt_type = m.group("type")
    # capture kernel srtt (microseconds)
    srtt_us = int(m.group("srtt"))
    addr = m.group("addr")
    m2 = ADDR_RE.search(addr)
    if not m2:
        return None
    return Event(
        ts_us=ts_us,
        sock=sock,
        evt_type=evt_type,
        src=m2.group("src"),
        srcp=int(m2.group("srcp")),
        dst=m2.group("dst"),
        dstp=int(m2.group("dstp")),
        srtt_us=srtt_us,
    )


def load_events(filepath: str) -> List[Event]:
    evts = []
    with open(filepath, "r") as f:
        for line in f:
            e = parse_line(line)
            if e:
                evts.append(e)
    return sorted(evts, key=lambda e: e.ts)


def extract_cycles(events: List[Event], client_ip: str, server_ip: str) -> List[Dict]:
    cycles = []
    i = 0
    n = len(events)
    while i < n:
        e = events[i]
        # start on client send_entry
        if e.type == "send_entry" and e.src == client_ip:
            cycle = {}
            cycle["send_entry"] = e.ts
            cycle["srtt_us"] = e.srtt_us
            sock = e.sock
            # find send_exit
            i += 1
            while i < n:
                if events[i].type == "send_exit" and events[i].sock == sock:
                    cycle["send_exit"] = events[i].ts
                    break
                i += 1
            # server recv_entry/exit
            i += 1
            while i < n:
                if (
                    events[i].type == "recv_entry"
                    and events[i].sock == sock
                    and events[i].src == server_ip
                ):
                    cycle["server_recv_entry"] = events[i].ts
                    break
                i += 1
            i += 1
            while i < n:
                if (
                    events[i].type == "recv_exit"
                    and events[i].sock == sock
                    and events[i].src == server_ip
                ):
                    cycle["server_recv_exit"] = events[i].ts
                    break
                i += 1
            # server send_entry/exit
            i += 1
            while i < n:
                if (
                    events[i].type == "send_entry"
                    and events[i].sock == sock
                    and events[i].src == server_ip
                ):
                    cycle["server_send_entry"] = events[i].ts
                    break
                i += 1
            i += 1
            while i < n:
                if (
                    events[i].type == "send_exit"
                    and events[i].sock == sock
                    and events[i].src == server_ip
                ):
                    cycle["server_send_exit"] = events[i].ts
                    break
                i += 1
            # client recv_entry/exit
            i += 1
            while i < n:
                if (
                    events[i].type == "recv_entry"
                    and events[i].sock == sock
                    and events[i].dst == client_ip
                ):
                    cycle["client_recv_entry"] = events[i].ts
                    break
                i += 1
            i += 1
            while i < n:
                if (
                    events[i].type == "recv_exit"
                    and events[i].sock == sock
                    and events[i].dst == client_ip
                ):
                    cycle["client_recv_exit"] = events[i].ts
                    break
                i += 1
            cycles.append(cycle)
        else:
            i += 1
    return cycles


def compute_metrics(cycle: Dict) -> Dict:
    return {
        "client_stack_us": cycle["send_exit"] - cycle["send_entry"],
        "server_stack_us": cycle["server_recv_exit"] - cycle["server_recv_entry"],
        "network_latency_us": cycle["srtt_us"],
    }


def write_csv(output: str, metrics: List[Dict]):
    fields = ["seq"] + list(metrics[0].keys())
    with open(output, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields)
        w.writeheader()
        for i, m in enumerate(metrics):
            row = {"seq": i, **m}
            w.writerow(row)
    print(f"Written {len(metrics)} records to {output}")


def main():
    args = parse_args()
    # load and merge events from all input files
    evts = []
    for f in args.inputs:
        evts.extend(load_events(f))
    evts.sort(key=lambda e: e.ts)
    cycles = extract_cycles(evts, args.client_ip, args.server_ip)

    # intelligent trimming: retain only longest contiguous run of complete cycles if requested
    if args.smart_skip:

        def is_complete(c: Dict) -> bool:
            return all(
                key in c
                for key in (
                    "send_entry",
                    "send_exit",
                    "server_recv_entry",
                    "server_recv_exit",
                )
            )

        complete_flags = [is_complete(c) for c in cycles]
        max_len = 0
        best_start = 0
        curr_start = None
        for idx, flag in enumerate(complete_flags):
            if flag and curr_start is None:
                curr_start = idx
            if not flag and curr_start is not None:
                length = idx - curr_start
                if length > max_len:
                    max_len = length
                    best_start = curr_start
                curr_start = None
        # tail
        if curr_start is not None:
            length = len(complete_flags) - curr_start
            if length > max_len:
                max_len = length
                best_start = curr_start
        if max_len > 0:
            cycles = cycles[best_start : best_start + max_len]
        else:
            print("No complete cycle runs found; check inputs.", file=sys.stderr)
            sys.exit(1)
    if not cycles:
        print("No cycles extracted after skipping; check parameters.", file=sys.stderr)
        sys.exit(1)
    metrics = [compute_metrics(c) for c in cycles]
    write_csv(args.output, metrics)
    if args.plot:
        # ensure output directory exists
        os.makedirs(os.path.dirname(args.plot_output), exist_ok=True)
        plot_py = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "plot_cdf.py")
        )
        cmd = [
            sys.executable,
            plot_py,
            "--input",
            args.output,
            "--output",
            args.plot_output,
        ]
        print(f"Generating CDF plot to {args.plot_output}")
        subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()
