# PingPong

**PingPong** is a lightweight eBPF-based demonstration tool for measuring end-to-end latency in the Linux TCP/IP stack. It consists of a client and a server that exchange “ping-pong” messages over a `SOCK_STREAM` (TCP) socket. An eBPF program attached to both endpoints records timestamp events as packets traverse the kernel’s networking layers, then computes:

- **Client stack latency**: time spent inside the TCP/IP layers on the client host  
- **Network latency**: time spent “in flight” on the wire  
- **Server stack latency**: time spent inside the TCP/IP layers on the server host  

Once the experiment completes, PingPong aggregates all measurements and generates a cumulative distribution function (CDF) plot of the total latency, along with a breakdown of each component.

## Features

- **Fine-grained tracing**: Uses eBPF to timestamp packets at key entry/exit points in the kernel’s TCP/IP path  
- **Customizable workload**: Specify both the **message size** and the **number of round-trip ping-pongs**  
- **No overlap**: Client waits for each pong before sending the next ping, ensuring clean measurements  
- **CDF output**: Produces a CDF diagram and numeric summary for easy visualization and analysis  

## How It Works

1. **Setup**: The server binds and listens on a TCP port.  
2. **Configuration**: The client connects, then you specify:
   - `-s, --size <bytes>`: Size of each ping payload  
   - `-n, --count <number>`: Total number of ping-pong exchanges  
3. **Measurement**:  
   - The client sends a ping of the specified size.  
   - When the packet enters the kernel send path, the eBPF program timestamps it.  
   - On the server, upon packet receive, another timestamp is taken before echoing it back.  
   - Finally, on the client, the returning packet is timestamped at entry to the receive path.  
4. **Analysis**: The tool correlates timestamps to calculate:
   - Client stack processing time  
   - Network transit time  
   - Server stack processing time  
5. **Output**:  
   - A `.csv` file of raw measurements  
   - A CDF plot (e.g., using GNUplot or Python/Matplotlib)  
   - A summary report showing percentile breakdowns for each latency component  

## Usage

```bash
# On the server machine:
sudo ./pingpong-server --port 12345

# On the client machine:
sudo ./pingpong-client --addr 192.0.2.10 --port 12345 \
    --size 1024 --count 10000 \
    --output results.csv
