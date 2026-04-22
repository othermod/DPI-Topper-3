CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11 -fomit-frame-pointer -ffast-math
LDFLAGS = -s

SRCS = gamepad.c gamepad_config.c
BINDIR = bin

all: cross

cross: $(BINDIR)/gamepad-armhf $(BINDIR)/gamepad-arm64

$(BINDIR)/gamepad-armhf: $(SRCS) | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/gamepad-arm64: $(SRCS) | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR):
	mkdir -p $@

native: $(BINDIR)/gamepad

$(BINDIR)/gamepad: $(SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BINDIR)

.PHONY: all native cross clean
