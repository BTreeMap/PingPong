# Dockerfile for PingPong server
# Uses precompiled binaries packaged per architecture
ARG PLATFORM
FROM alpine:3.22

# Extract the compiled binaries into /app
ADD artifacts/pingpong-${PLATFORM}.tar.gz /app
WORKDIR /app/pingpong-${PLATFORM}

# Launch the server by default
ENTRYPOINT ["./pingpong-ebpf"]
