# Dockerfile for PingPong server
# Uses precompiled binaries packaged per architecture
ARG PLATFORM
FROM alpine:3.22

# Extract the compiled binaries into /app
COPY --chown=root:root pingpong /app/
WORKDIR /app/pingpong

# Launch the server by default
ENTRYPOINT ["./pingpong-ebpf"]
