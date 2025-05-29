CC = gcc
CFLAGS = -O2 -Wall
BUILD_DIR = build
TARGETS = client server
TARGETS_BIN = $(TARGETS:%=$(BUILD_DIR)/%)

all: $(TARGETS_BIN)

$(BUILD_DIR)/%: src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILD_DIR) results.csv
