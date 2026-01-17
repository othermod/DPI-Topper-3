TARGET = gamepad
SRC = gamepad.c

CC = gcc
CFLAGS = -O3 -flto -Wall -Wextra -std=c11
CFLAGS += -fomit-frame-pointer -ffast-math -pipe
LDFLAGS = -flto -s

CC_32 = arm-linux-gnueabihf-gcc
CC_64 = aarch64-linux-gnu-gcc

CROSS_TARGETS = $(TARGET)-pi0 \
                $(TARGET)-pi3-32 $(TARGET)-pi3-64 \
                $(TARGET)-pi4-32 $(TARGET)-pi4-64 \
                $(TARGET)-pi5-32 $(TARGET)-pi5-64

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -march=native -mtune=native $< -o $@ $(LDFLAGS)

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

cross: $(CROSS_TARGETS)

clean:
	rm -f $(TARGET) $(CROSS_TARGETS)

.PHONY: cross clean
