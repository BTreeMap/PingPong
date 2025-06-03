# Dockerfile for PingPong server
# Uses precompiled binaries packaged per architecture
FROM ubuntu:noble AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        libbpf1 \
    && \
    rm -rf /tmp/* \
        /var/lib/apt/lists/*

# Extract the compiled binaries into /opt

COPY --chown=root:root pingpong /opt/pingpong/

FROM scratch

COPY --from=builder / /

ENV PATH="/opt/pingpong:${PATH}"
WORKDIR /opt/pingpong

# Launch the server by default
ENTRYPOINT ["pingpong-ebpf"]
