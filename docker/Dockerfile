# Dockerfile for PingPong server
# Uses precompiled binaries packaged per architecture

# Builder stage: Used to compile and package the application
FROM ubuntu:noble AS builder

ARG DEBIAN_FRONTEND=noninteractive

# Install dependencies and Tailscale
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        gnupg \
        iproute2 \
        iptables \
        libbpf1 \
        lsb-release \
    && \
    # Install Tailscale
    TRACK=stable && \
    OS=ubuntu && \
    VERSION=$(lsb_release -cs) && \
    mkdir -p --mode=0755 /usr/share/keyrings && \
    curl "https://pkgs.tailscale.com/$TRACK/$OS/$VERSION.noarmor.gpg" | tee /usr/share/keyrings/tailscale-archive-keyring.gpg > /dev/null && \
    chmod 0644 /usr/share/keyrings/tailscale-archive-keyring.gpg && \
    curl https://pkgs.tailscale.com/$TRACK/$OS/$VERSION.tailscale-keyring.list | tee /etc/apt/sources.list.d/tailscale.list && \
    chmod 0644 /etc/apt/sources.list.d/tailscale.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        tailscale \
        tailscale-archive-keyring \
    && \
    apt-get clean && \
    rm -rf \
        /tmp/* \
        /var/cache/apt/* \
        /var/lib/apt/lists/* \
        /var/tmp/*

# Copy the compiled binaries into /opt.
COPY --chown=root:root pingpong /opt/pingpong/

# Copy the entrypoint script into the root directory.
COPY --chown=root:root root/ /

# Set the entrypoint script to be executable.
RUN chmod +x /entrypoint.sh

# Final stage: Create a minimal image with only the application and its runtime dependencies
FROM scratch

# Copy necessary files from the builder stage
COPY --from=builder / /

# Add the application directory to the PATH
ENV PATH="/opt/pingpong:${PATH}" \
    ROLE="server" \
    CONTROL_PORT="4242" \
    EXP_PORT="24242" \
    SERVER_ADDR="" \
    SIZE="1024" \
    COUNT="100" \
    CLIENT_OUTPUT="/var/lib/pingpong/client.csv"

# Set the user and working directory.
USER root
WORKDIR /opt/pingpong

# Launch the server by default.
ENTRYPOINT ["/entrypoint.sh"]
