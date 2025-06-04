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
from functools import partial

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


curr_dir = os.path.dirname(os.path.abspath(__file__))
root_dir = os.path.abspath(os.path.join(curr_dir, "..", ".."))
demo_dir = os.path.join(root_dir, "demo")


def parse_args():
    p = argparse.ArgumentParser(description="Analyze eBPF pingpong event logs.")
    p.add_argument(
        "--inputs",
        nargs="+",
        type=str,
        default=[
            os.path.join(demo_dir, "client.log"),
            os.path.join(demo_dir, "server.log"),
        ],
        help="One or more eBPF event log files",
    )
    p.add_argument(
        "--client-ip", default="100.80.0.1", help="Client IP address to filter events"
    )
    p.add_argument(
        "--server-ip", default="100.80.0.0", help="Server IP address to filter events"
    )
    p.add_argument(
        "--smart-skip",
        action="store_true",
        default=True,
        help="Enable intelligent trimming of incomplete cycle runs",
    )
    p.add_argument(
        "--plot", action="store_true", default=True, help="Generate CDF plot after CSV"
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


def extract_cycles(
    events: List[Event], client_ip: str, server_ip: str, subcall: bool = False
) -> List[Dict]:
    cycles = []
    i = 0
    n = len(events)
    while i < n:
        e = events[i]
        # start on client send_entry
        if e.type == "send_entry" and e.src == client_ip and e.dst == server_ip:
            cycle = {}
            cycle["send_entry"] = e.ts
            cycle["srtt_us"] = e.srtt_us
            sock = e.sock
            # find send_exit
            i += 1
            if i >= n:
                break
            assert (
                events[i].type == "send_exit"
                and events[i].sock == sock
                and events[i].src == client_ip
                and events[i].dst == server_ip
            )
            cycle["send_exit"] = events[i].ts
            # find recv_entry
            i += 1
            if i >= n:
                break
            assert (
                events[i].type == "recv_entry"
                and events[i].sock == sock
                and events[i].src == server_ip
                and events[i].dst == client_ip
            )
            cycle["recv_entry"] = events[i].ts
            # find recv_exit
            i += 1
            if i >= n:
                break
            assert (
                events[i].type == "recv_exit"
                and events[i].sock == sock
                and events[i].src == server_ip
                and events[i].dst == client_ip
            )
            cycle["recv_exit"] = events[i].ts
            cycles.append(cycle)
        i += 1
    if subcall:
        return cycles
    return cycles or extract_cycles(events, server_ip, client_ip, subcall=True)


def compute_metrics(cycle: Dict) -> Dict:
    return {
        "send_stack_us": cycle["send_exit"] - cycle["send_entry"],
        "recv_stack_us": cycle["recv_exit"] - cycle["recv_entry"],
        "network_latency_ms": cycle["srtt_us"] / 1000.0,
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


def process_input(
    input_path: str, client_ip: str, server_ip: str, smart_skip: bool = True
) -> List[Event]:
    """
    Process a single input file and return a list of parsed events.
    """
    events = load_events(input_path)

    # filter events by client/server IPs
    events = list(
        filter(
            lambda e: (e.src == client_ip and e.dst == server_ip)
            or (e.src == server_ip and e.dst == client_ip),
            events,
        )
    )

    # sort by timestamp
    events.sort(key=lambda e: e.ts)

    # intelligent trimming: retain only longest contiguous run of complete cycles if requested
    if smart_skip:

        def is_complete(index: int) -> bool:
            slice_ = list(map(lambda evt: evt.type, events[index : index + 4]))
            return all(
                key in slice_
                for key in (
                    "send_entry",
                    "send_exit",
                    "recv_entry",
                    "recv_exit",
                )
            )

        # print(events)
        complete_flags = [is_complete(i) for i in range(len(events) - 3)]
        # print(complete_flags)
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
        max_len -= max_len % 4  # ensure we have complete cycles
        events = events[best_start : best_start + max_len]

    if not events:
        print(f"No events found in {input_path} after filtering.", file=sys.stderr)
    else:
        print(f"Loaded {len(events)} events from {input_path}")
    return events


def main():
    args = parse_args()
    # load and merge events from all input files
    evts = []
    for f in args.inputs:
        evts.append(process_input(f, args.client_ip, args.server_ip, args.smart_skip))

    if not evts:
        print("No events extracted after skipping; check parameters.", file=sys.stderr)
        sys.exit(1)

    cycs = list(
        map(
            partial(extract_cycles, client_ip=args.client_ip, server_ip=args.server_ip),
            evts,
        )
    )

    metrics = [[compute_metrics(c) for c in cycle] for cycle in cycs]
    for f, m in zip(args.inputs, metrics):
        write_csv(os.path.splitext(f)[0] + ".csv", m)
    if args.plot:
        plot_py = os.path.abspath(os.path.join(curr_dir, "plot_cdf.py"))
        for f in args.inputs:
            if not os.path.exists(os.path.splitext(f)[0] + ".csv"):
                print(f"CSV file for {f} not found; skipping plot generation.")
            else:
                plot_input = os.path.splitext(f)[0] + ".csv"
                plot_output = os.path.splitext(f)[0] + "_cdf.png"
                cmd = [
                    sys.executable,
                    plot_py,
                    "--input",
                    plot_input,
                    "--output",
                    plot_output,
                ]
                print(f"Generating CDF plot to {plot_output}")
                subprocess.run(cmd, check=True)


if __name__ == "__main__":
    main()
