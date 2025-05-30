# PingPong

**PingPong** is a simple eBPF-based tool for measuring end-to-end latency in the Linux TCP/IP stack. A client and server exchange “ping-pong” messages over a TCP (`SOCK_STREAM`) socket while an eBPF program records timestamps inside the kernel at these points:

- **Client stack**: when the packet enters and leaves the client’s TCP/IP layers  
- **Network**: time in flight on the wire  
- **Server stack**: when the packet enters and leaves the server’s TCP/IP layers  

Once the run is complete, PingPong writes all raw timestamps to a CSV file and then uses a Python script to generate a cumulative distribution function (CDF) plot and calculate percentile breakdowns for each component.

## How It Works

1. **Server startup**  
   - Binds and listens on a TCP port.  
2. **Client setup**  
   - Connects to the server.  
   - You specify:
     - `--size <bytes>`: payload size of each ping  
     - `--count <number>`: total number of ping-pong exchanges  
3. **Measurement loop** (repeated `count` times)  
   - Client sends a ping; eBPF timestamps at send-entry.  
   - Server timestamps at receive-entry, then sends a pong back.  
   - Client timestamps the returning packet at receive-entry.  
4. **Data analysis**  
   - The client collects all timestamps, matches send and receive events, and computes:
     - Client stack processing time  
     - Network transit time  
     - Server stack processing time  
5. **Output**  
   - Raw data in `results.csv`  
   - A CDF plot drawn by `scripts/plot_cdf.py` (Python)  
   - A summary report with percentile values for each latency component  

## Building and Running

```bash
make all

# On server
sudo ./pingpong-server --port 12345
sudo ./pingpong-ebpf --

# On client
sudo ./pingpong-client \
  --addr 192.0.2.10 \
  --port 12345 \
  --size 1024 \
  --count 10000 \
  --output results.csv

# Plot CDF
python3 scripts/plot_cdf.py \
  --input results.csv \
  --output latency_cdf.png
```

## Dependencies

- Linux kernel ≥ 4.18 with eBPF support
- `clang` and `llvm` (to build the eBPF program)
- `libbpf` or another BPF loader
- Python ≥ 3.6 (for the CDF script)

## License

MIT License — see `LICENSE` for details.
