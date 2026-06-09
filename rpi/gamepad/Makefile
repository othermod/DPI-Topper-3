# Cross-compilers
CC_32 = arm-linux-gnueabi-gcc -march=armv6zk -mfloat-abi=softfp
CC_64 = aarch64-linux-gnu-gcc

CFLAGS_COMMON = -O3 -lrt -static

# Build for 32-bit ARM (Pi Zero, Pi 1, Pi 2)
32:
	@mkdir -p 32
	@rm -f *.o
	$(CC_32) -o 32/gamepad gamepad.c $(CFLAGS_COMMON)
	$(CC_32) -o 32/mapper  mapper.c  $(CFLAGS_COMMON)

# Build for 64-bit ARM (Pi 3, Pi 4, Pi 5)
64:
	@mkdir -p 64
	@rm -f *.o
	$(CC_64) -o 64/gamepad gamepad.c $(CFLAGS_COMMON)
	$(CC_64) -o 64/mapper  mapper.c  $(CFLAGS_COMMON)

# Clean build artifacts
clean:
	rm -rf 32 64
	rm -f *.o

.PHONY: 32 64 clean
