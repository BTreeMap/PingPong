CC = gcc
CFLAGS = -O2 -Wall
BUILD_DIR = build
TARGETS = client server
TARGETS_BIN = $(TARGETS:%=$(BUILD_DIR)/%)

BPF_DIR = src/bpf
BPF_OBJ_USER = $(BUILD_DIR)/pingpong_user
BPF_OBJ_SKEL = $(BUILD_DIR)/pingpong_kern.skel.h
BPF_OBJ_KERN = $(BUILD_DIR)/pingpong_kern.bpf.o

all: $(TARGETS_BIN) $(BPF_OBJ_USER)

$(BUILD_DIR)/%: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BPF_OBJ_KERN): $(BPF_DIR)/pingpong_kern.bpf.c
	$(CC) $(CFLAGS) -g -O2 -target bpf -c $< -o $@

$(BPF_OBJ_SKEL): $(BPF_OBJ_KERN)
	bpftool gen skeleton $< > $@

$(BPF_OBJ_USER): $(BPF_DIR)/pingpong_user.c $(BPF_OBJ_SKEL)
	$(CC) $(CFLAGS) -g -O2 $< -lbpf -lelf -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) results.csv
	rm -f $(TARGETS_BIN) $(BPF_OBJ_USER) $(BPF_OBJ_KERN) $(BPF_OBJ_SKEL) $(BUILD_DIR)/*
