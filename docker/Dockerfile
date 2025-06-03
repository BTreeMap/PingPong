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
        iproute2 \
        iptables \
        libbpf1 \
        tailscale \
        tailscale-archive-keyring \
    && \
    rm -rf \
        /tmp/* \
        /var/cache/apt/* \
        /var/lib/apt/lists/* \
        /var/tmp/*

# Copy the compiled binaries into /opt.
COPY --chown=root:root pingpong /opt/pingpong/

# Final stage: Create a minimal image with only the application and its runtime dependencies
FROM scratch

# Copy necessary files from the builder stage
COPY --from=builder / /

# Add the application directory to the PATH
ENV PATH="/opt/pingpong:${PATH}"

# Set the user and working directory.
USER root
WORKDIR /opt/pingpong

# Launch the server by default.
ENTRYPOINT ["pingpong-ebpf"]
