CC = gcc
CFLAGS = -O3 -Wall -Wextra -std=c11 -fomit-frame-pointer -ffast-math -pipe
LDFLAGS = -s

BINDIR = bin

all: cross

cross: $(BINDIR)/backlight-gamepad-armhf $(BINDIR)/backlight-gamepad-arm64 \
       $(BINDIR)/backlight-basic-armhf $(BINDIR)/backlight-basic-arm64 \
       $(BINDIR)/backlight-ctl-armhf $(BINDIR)/backlight-ctl-arm64

$(BINDIR)/backlight-gamepad-armhf: backlight-gamepad.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-gamepad-arm64: backlight-gamepad.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-basic-armhf: backlight-basic.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-basic-arm64: backlight-basic.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-ctl-armhf: backlight-ctl.c | $(BINDIR)
	arm-linux-gnueabihf-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-ctl-arm64: backlight-ctl.c | $(BINDIR)
	aarch64-linux-gnu-gcc $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR):
	mkdir -p $@

native: $(BINDIR)/backlight-gamepad $(BINDIR)/backlight-basic $(BINDIR)/backlight-ctl

$(BINDIR)/backlight-gamepad: backlight-gamepad.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-basic: backlight-basic.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(BINDIR)/backlight-ctl: backlight-ctl.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -rf $(BINDIR)

.PHONY: all native cross clean
