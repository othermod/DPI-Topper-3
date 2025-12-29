# Simplified Makefile
# make       - Build for current system
# make cross - Build for all Raspberry Pi models and architectures

TARGET = gamepad
SRC = gamepad.c
INSTALL_DIR = /usr/local/bin

# Base compiler flags
CFLAGS = -O3 -flto -Wall -Wextra -std=c11
CFLAGS += -fomit-frame-pointer -ffast-math -pipe
LDFLAGS = -flto -s

# Native compilation (default)
CC = gcc

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -march=native -mtune=native $< -o $@ $(LDFLAGS)

# Cross-compilation target
CC_32 = arm-linux-gnueabihf-gcc
CC_64 = aarch64-linux-gnu-gcc

CROSS_TARGETS = $(TARGET)-pi0 \
                $(TARGET)-pi3-32 $(TARGET)-pi3-64 \
                $(TARGET)-pi4-32 $(TARGET)-pi4-64 \
                $(TARGET)-pi5-32 $(TARGET)-pi5-64

cross: $(CROSS_TARGETS)
	@echo ""
	@echo "Built all cross-compiled binaries:"
	@ls -lh $(TARGET)-*
	@echo ""

$(TARGET)-pi0: $(SRC)
	$(CC_32) $(CFLAGS) -march=armv6 -mtune=arm1176jzf-s -mfpu=vfp -marm $< -o $@ $(LDFLAGS)

$(TARGET)-pi3-32: $(SRC)
	$(CC_32) $(CFLAGS) -march=armv8-a+crc -mtune=cortex-a53 -mfpu=neon-fp-armv8 $< -o $@ $(LDFLAGS)

$(TARGET)-pi3-64: $(SRC)
	$(CC_64) $(CFLAGS) -march=armv8-a+crc -mtune=cortex-a53 $< -o $@ $(LDFLAGS)

$(TARGET)-pi4-32: $(SRC)
	$(CC_32) $(CFLAGS) -march=armv8-a+crc -mtune=cortex-a72 -mfpu=neon-fp-armv8 $< -o $@ $(LDFLAGS)

$(TARGET)-pi4-64: $(SRC)
	$(CC_64) $(CFLAGS) -march=armv8-a+crc+simd -mtune=cortex-a72 $< -o $@ $(LDFLAGS)

$(TARGET)-pi5-32: $(SRC)
	$(CC_32) $(CFLAGS) -march=armv8-a+crc -mtune=cortex-a76 -mfpu=neon-fp-armv8 $< -o $@ $(LDFLAGS)

$(TARGET)-pi5-64: $(SRC)
	$(CC_64) $(CFLAGS) -march=armv8.2-a+crc+simd -mtune=cortex-a76 $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(CROSS_TARGETS)

install: $(TARGET)
	@if [ "$$(id -u)" != "0" ]; then \
		echo "You must be root to install. Please run with sudo."; \
		exit 1; \
	fi
	install -d $(INSTALL_DIR)
	install -m 755 $(TARGET) $(INSTALL_DIR)
	@echo "$(TARGET) installed to $(INSTALL_DIR)"

uninstall:
	@if [ "$$(id -u)" != "0" ]; then \
		echo "You must be root to uninstall. Please run with sudo."; \
		exit 1; \
	fi
	rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "$(TARGET) uninstalled"

.PHONY: all cross clean install uninstall
