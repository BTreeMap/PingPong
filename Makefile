# Compiler toolchains
CC        := gcc
BPF_CC    := clang

# Basic flags
CFLAGS    := -O2 -Wall
BPF_CFLAGS := -g -O2 -target bpf \
              -I$(KERNEL_HEADERS)/include \
              -I$(KERNEL_HEADERS)/include/uapi

# Where to find the kernel headers (defaults to the running kernel)
HOST_KERNEL_VERSION ?= $(shell uname -r)
KERNEL_HEADERS       ?= /lib/modules/$(HOST_KERNEL_VERSION)/build

# Build directories & targets
BUILD_DIR      := build
TARGETS        := client server
TARGETS_BIN    := $(TARGETS:%=$(BUILD_DIR)/%)

# BPF artifacts
BPF_DIR        := src/bpf
BPF_OBJ_KERN   := $(BUILD_DIR)/pingpong_kern.bpf.o
BPF_OBJ_SKEL   := $(BUILD_DIR)/pingpong_kern.skel.h
BPF_OBJ_USER   := $(BUILD_DIR)/pingpong_user

.PHONY: all clean

all: $(TARGETS_BIN) $(BPF_OBJ_USER)

# Build user‐space clients
$(BUILD_DIR)/%: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Compile the BPF .o with the proper kernel headers
$(BPF_OBJ_KERN): $(BPF_DIR)/pingpong_kern.bpf.c
	@mkdir -p $(BUILD_DIR)
	$(BPF_CC) $(CFLAGS) $(BPF_CFLAGS) \
	  -c $< -o $@

# Generate the skeleton header from the .o
$(BPF_OBJ_SKEL): $(BPF_OBJ_KERN)
	@mkdir -p $(BUILD_DIR)
	bpftool gen skeleton $< > $@

# Build the BPF loader/user program against libbpf
$(BPF_OBJ_USER): $(BPF_DIR)/pingpong_user.c $(BPF_OBJ_SKEL)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -g -O2 \
	  $< -lbpf -lelf \
	  -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) results.csv
