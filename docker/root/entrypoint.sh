#!/bin/bash

# Before running this script, ensure to bind /dev/net/tun:/dev/net/tun 
# to the container. Additionally, grant the following capabilities:
#   - NET_ADMIN: Allows the container to manage networking aspects (Tailscale needs this flag to run correctly).
#   - NET_RAW: Required for raw socket operations (Tailscale uses this for its networking).
#   - SYS_ADMIN: Enables the container to perform system administration tasks (PingPong eBPF functionalities require this for correct functioning).
#   - BPF: Allows the container to load eBPF programs (PingPong uses this for loading eBPF programs).
#   - PERFMON: Required for performance monitoring (PingPong eBPF functionalities require this for correct functioning).
# During the first run, you may need to set up the credentials for Tailscale manually.

# Exit the script immediately if any command fails
set -ex

ROLE="${ROLE:-server}"  # Default role is 'server' if not set
CONTROL_PORT="${CONTROL_PORT:-4242}"  # Default control port is 4242 if not set
EXP_PORT="${EXP_PORT:-24242}"  # Default experiment port is 4243 if not set

if [ "$ROLE" != "server" ] && [ "$ROLE" != "client" ]; then
    echo "ERROR: Invalid ROLE specified. Must be 'server' or 'client'."
    exit 1
fi

# (Optional) Additional arguments to pass to the tailscaled command.
# Example: "--tun=userspace-networking"
TAILSCALED_EXTRA_ARGS="${TAILSCALED_EXTRA_ARGS:-}"

# Check if userspace networking is required (if /dev/net/tun does not exist)
if [ ! -e /dev/net/tun ]; then
    TAILSCALED_EXTRA_ARGS="$TAILSCALED_EXTRA_ARGS --tun=userspace-networking"
    echo "INFO: Using userspace networking for Tailscale."
fi

# Additional arguments to pass to the tailscale set command. 
# Default: "--advertise-exit-node --accept-dns=false --accept-routes=false --webclient"
TAILSCALE_EXTRA_ARGS="${TAILSCALE_EXTRA_ARGS:---advertise-exit-node --accept-dns=false --accept-routes=false --webclient}"

# Optimize Tailscale performance by configuring UDP generic receive offload (GRO)
echo "Optimizing Tailscale performance with UDP GRO settings..."
NETDEV=$(ip -o route get 8.8.8.8 | cut -f 5 -d " ")
ethtool -K $NETDEV rx-udp-gro-forwarding on rx-gro-list off || echo "Failed to set GRO options on $NETDEV, continuing..."

# Start the Tailscale daemon in the background
echo "Starting the Tailscale daemon with extra args: ${TAILSCALED_EXTRA_ARGS}"
tailscaled --statedir=/var/lib/tailscale ${TAILSCALED_EXTRA_ARGS} &

# Sleep for 5 seconds to allow Tailscale daemon to initialize
echo "Sleeping for 5 seconds to allow the Tailscale daemon to initialize..."
sleep 5

# Apply configuration settings with tailscale set.
echo "Setting Tailscale configuration with extra args: ${TAILSCALE_EXTRA_ARGS}"
tailscale set ${TAILSCALE_EXTRA_ARGS}

# Bring up the Tailscale connection.
echo "Connecting Tailscale..."
tailscale up

if [ "$ROLE" == "server" ]; then
    echo "Starting PingPong server..."
    # Start the PingPong eBPF component in the background
    pingpong-ebpf --sport $EXP_PORT > /var/lib/pingpong/ebpf.log 2> /var/lib/pingpong/ebpf.stderr &
    # Start the PingPong server in the foreground
    pingpong-server --port $CONTROL_PORT
else
    echo "Starting PingPong client..."
    # Start the PingPong eBPF component in the background
    pingpong-ebpf --dport $EXP_PORT > /var/lib/pingpong/ebpf.log 2> /var/lib/pingpong/ebpf.stderr &
    # Start the PingPong client in the foreground
    if [ -z "$SERVER_ADDR" ]; then
        echo "SERVER_ADDR environment variable is not set. Please enter the server address:"
        read -p "Server address: " SERVER_ADDR
        if [ -z "$SERVER_ADDR" ]; then
            echo "ERROR: Server address cannot be empty."
            exit 1
        fi
    fi
    pingpong-client --control-port $CONTROL_PORT --exp-port $EXP_PORT --addr $SERVER_ADDR --size $SIZE --count $COUNT --output $CLIENT_OUTPUT
fi