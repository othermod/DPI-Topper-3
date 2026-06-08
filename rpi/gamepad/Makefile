# Cross-compilers
CC_32 = arm-linux-gnueabihf-gcc
CC_64 = aarch64-linux-gnu-gcc

CFLAGS_COMMON = -O3 -Wall -Wextra -std=c11 -fomit-frame-pointer -ffast-math

# Build for 32-bit architecture
32:
	@mkdir -p 32
	$(CC_32) -o 32/gamepad gamepad.c $(CFLAGS_COMMON)
	$(CC_32) -o 32/gamepad_config gamepad_config.c $(CFLAGS_COMMON)

# Build for 64-bit architecture
64:
	@mkdir -p 64
	$(CC_64) -o 64/gamepad gamepad.c $(CFLAGS_COMMON)
	$(CC_64) -o 64/gamepad_config gamepad_config.c $(CFLAGS_COMMON)

# Clean build artifacts
clean:
	rm -rf 32 64

.PHONY: 32 64 clean
