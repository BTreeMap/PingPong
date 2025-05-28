CC = gcc
CFLAGS = -O2 -Wall
TARGETS = pingpong-server pingpong-client

all: $(TARGETS)

pingpong-server: server.c
	$(CC) $(CFLAGS) -o $@ server.c

pingpong-client: client.c
	$(CC) $(CFLAGS) -o $@ client.c

clean:
	rm -f $(TARGETS) results.csv
