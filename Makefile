# Compiler toolchains
CC        := gcc
BPF_CC    := clang

# Basic flags
CFLAGS    := -O2 -Wall
# BPF compile flags; includes kernel headers and BTF header
BUILD_DIR := build
# pick up your running‐kernel version
HOST_KERNEL_VERSION ?= $(shell uname -r)
KERNEL_HEADERS       ?= /usr/src/linux-headers-$(HOST_KERNEL_VERSION)
ARCH                 ?= $(shell uname -m | sed s/aarch64/arm64/)
VMLINUX_HDR          ?= $(BUILD_DIR)/vmlinux.h

BPF_CFLAGS := -g -O2 -target bpf \
  -nostdinc \
  -I$(KERNEL_HEADERS)/include \
  -I$(KERNEL_HEADERS)/include/uapi \
  -I$(KERNEL_HEADERS)/include/generated \
  -I$(KERNEL_HEADERS)/include/generated/uapi \
  -I$(KERNEL_HEADERS)/arch/$(ARCH)/include \
  -isystem /usr/include/bpf \
  -include $(VMLINUX_HDR)

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

# Build user-space clients
$(BUILD_DIR)/%: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

# Compile the BPF .o with the proper kernel headers and BTF
$(BPF_OBJ_KERN): $(BPF_DIR)/pingpong_kern.bpf.c $(VMLINUX_HDR)
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
