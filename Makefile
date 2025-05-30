# Load environment variables from .env file if it exists
-include .env

# Compiler toolchains
CC        := gcc
BPF_CC    := clang

# Basic flags
CFLAGS    = -O2 -Wall -I$(BUILD_DIR) -I$(BPF_DIR) -Isrc -DARCH=$(ARCH)
# BPF compile flags; includes kernel headers and BTF header
BUILD_DIR := build
INCLUDE_DIR := $(BUILD_DIR)/include
# pick up your running‐kernel version

# Normalize architecture to supported values
ARCH := $(shell uname -m | sed -e 's/x86_64/amd64/' -e 's/aarch64/arm64/')
ifneq ($(ARCH),amd64)
ifneq ($(ARCH),arm64)
	$(error Unsupported architecture: $(ARCH_RAW). Only amd64 and arm64 are supported)
endif
endif

ifeq ($(ARCH),amd64)
  BPF_ARCH := __TARGET_ARCH_x86
else ifeq ($(ARCH),arm64)
  BPF_ARCH := __TARGET_ARCH_arm64
endif

HOST_KERNEL_VERSION  ?= $(shell uname -r)
KERNEL_HEADERS       ?= /usr/src/linux-headers-$(HOST_KERNEL_VERSION)
VMLINUX_HDR          ?= $(BUILD_DIR)/vmlinux.h

BPF_CFLAGS := -g -O2 -target bpf \
  -nostdinc \
  -I$(BUILD_DIR)/include \
  -I$(BPF_DIR) \
  -DARCH=$(ARCH) \
  -D$(BPF_ARCH)

# Build directories & targets
TARGETS        := client server
TARGETS_BIN    := $(TARGETS:%=$(BUILD_DIR)/%)

# BPF artifacts
BPF_DIR        := src/bpf
BPF_OBJ_KERN   := $(BUILD_DIR)/pingpong_kern.bpf.o
BPF_OBJ_SKEL   := $(BUILD_DIR)/pingpong_kern.skel.h
BPF_OBJ_USER   := $(BUILD_DIR)/pingpong_user

.PHONY: all clean

all: $(VMLINUX_HDR) $(TARGETS_BIN) $(BPF_OBJ_USER)

# Extract BTF and generate vmlinux.h
$(VMLINUX_HDR):
	@mkdir -p $(BUILD_DIR)
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > $@
	cp $@ $(BPF_DIR)/vmlinux.h

# Copy headers to the build directory
$(INCLUDE_DIR):
	@mkdir -p $@
	cp -r /usr/include/bpf $@

# Build common libraries and headers
$(BUILD_DIR)/common.o: src/common.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build user-space clients
$(BUILD_DIR)/%: src/%.c $(BPF_OBJ_SKEL) $(BUILD_DIR)/common.o $(BPF_DIR)/event_defs.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(BUILD_DIR)/common.o -lbpf -lelf $(LDFLAGS)

# Compile the BPF .o with the proper kernel headers and BTF
$(BPF_OBJ_KERN): $(BPF_DIR)/pingpong_kern.bpf.c $(VMLINUX_HDR) $(INCLUDE_DIR) $(BPF_DIR)/event_defs.h
	@mkdir -p $(BUILD_DIR)
	$(BPF_CC) $(CFLAGS) $(BPF_CFLAGS) \
	  -c $< -o $@

# Generate the skeleton header from the .o
$(BPF_OBJ_SKEL): $(BPF_OBJ_KERN)
	@mkdir -p $(BUILD_DIR)
	bpftool gen skeleton $< > $@
	cp $@ $(BPF_DIR)/pingpong_kern.skel.h

# Build the BPF loader/user program against libbpf
$(BPF_OBJ_USER): $(BPF_DIR)/pingpong_user.c $(BPF_OBJ_SKEL) $(BPF_DIR)/event_defs.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -g -O2 \
	  $< -lbpf -lelf \
	  -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) results.csv
