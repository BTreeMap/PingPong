# Dockerfile for PingPong server
# Uses precompiled binaries packaged per architecture
FROM alpine:3.22

# Extract the compiled binaries into /opt

COPY --chown=root:root pingpong /opt/pingpong/
ENV PATH="/opt/pingpong:${PATH}"
WORKDIR /opt/pingpong

# Launch the server by default
ENTRYPOINT ["pingpong-ebpf"]
