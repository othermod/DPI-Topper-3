CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11 -fomit-frame-pointer -ffast-math
LDFLAGS = -s

BINDIR = bin

all: cross

cross: $(BINDIR)/gamepad-armhf $(BINDIR)/gamepad-arm64 \
       $(BINDIR)/gamepad_config-armhf $(BINDIR)/gamepad_config-arm64

$(BINDIR)/gamepad-armhf: gamepad.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/gamepad-arm64: gamepad.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/gamepad_config-armhf: gamepad_config.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/gamepad_config-arm64: gamepad_config.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR):
	mkdir -p $@

native: $(BINDIR)/gamepad $(BINDIR)/gamepad_config

$(BINDIR)/gamepad: gamepad.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BINDIR)/gamepad_config: gamepad_config.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(BINDIR)

.PHONY: all native cross clean
